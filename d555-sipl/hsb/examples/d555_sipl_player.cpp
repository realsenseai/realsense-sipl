/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-FileCopyrightText: Copyright (c) 2025-2026 RealSense AI. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdlib>
#include <stdexcept>
#include <string_view>

#include <holoscan/holoscan.hpp>
#include <holoscan/operators/holoviz/holoviz.hpp>

#include <hololink/common/holoargs.hpp>
#include <hololink/core/logging.hpp>
#include <hololink/operators/d555_sipl_capture/d555_sipl_capture.hpp>
#include <hololink/operators/image_decoder/image_decoder.hpp>

// SIPL reports both D555 streams as RAW16, so the frame layout comes from D555_STREAM, read
// the same way as the UDDF driver that started the stream (D555ModuleDriver.cpp): depth is Z16,
// RGB is YUYV.
static hololink::csi::PixelFormat d555_stream_pixel_format()
{
    const char* env = std::getenv("D555_STREAM");
    const std::string_view value { env != nullptr ? env : "" };
    if (value == "rgb" || value == "RGB" || value == "color") {
        return hololink::csi::PixelFormat::YUYV_422_8;
    }
    return hololink::csi::PixelFormat::RAW_16;
}

class Application : public holoscan::Application {
public:
    Application(
        const std::string& camera_config,
        const std::string& json_config,
        bool raw_output,
        bool headless,
        bool fullscreen,
        int frame_limit)
        : camera_config_(camera_config)
        , json_config_(json_config)
        , raw_output_(raw_output)
        , headless_(headless)
        , fullscreen_(fullscreen)
        , frame_limit_(frame_limit)
    {
    }

    void compose() override
    {
        using namespace holoscan;

        std::shared_ptr<Condition> condition;
        if (frame_limit_) {
            condition = make_condition<CountCondition>("count", frame_limit_);
        } else {
            condition = make_condition<BooleanCondition>("ok", true);
        }

        // SIPL capture operator captures either the RAW or ISP-Processed image via NvSIPL.
        auto sipl_capture = make_operator<hololink::operators::D555SIPLCaptureOp>(
            "d555_sipl_capture",
            camera_config_,
            json_config_,
            raw_output_,
            condition);
        const auto& camera_info = sipl_capture->get_camera_info();

        // Holoviz is used to render the image(s).
        const float view_width = 1.0f / camera_info.size();
        std::vector<ops::HolovizOp::InputSpec> specs;
        for (uint32_t i = 0; i < camera_info.size(); ++i) {
            ops::HolovizOp::InputSpec::View view;
            view.offset_x_ = view_width * i;
            view.offset_y_ = 0;
            view.width_ = view_width;
            view.height_ = 1;

            ops::HolovizOp::InputSpec spec(camera_info[i].output_name, ops::HolovizOp::InputType::COLOR);
            spec.views_.push_back(view);
            specs.push_back(spec);
        }

        auto visualizer = make_operator<holoscan::ops::HolovizOp>(
            "holoviz",
            Arg("tensors", specs),
            Arg("fullscreen", fullscreen_),
            Arg("headless", headless_));

        if (!raw_output_) {
            // When capturing ISP-processed images from SIPL, output directly
            // from SIPL to the vizualizer.
            add_flow(sipl_capture, visualizer, { { "output", "receivers" } });
        } else {
            // The D555 sends no Bayer data, so the RAW frames go to ImageDecoder, as in the CoE
            // players: it colorizes Z16 depth and converts YUYV to RGB. ImageDecoder reads the
            // first tensor of its input and D555_STREAM sets one format, so one camera per run.
            if (camera_info.size() != 1) {
                throw std::runtime_error(fmt::format(
                    "camera config has {} cameras; the D555 player shows one", camera_info.size()));
            }
            const auto pixel_format = d555_stream_pixel_format();
            const auto& info = camera_info[0];
            auto image_decoder = make_operator<hololink::operators::ImageDecoder>(
                fmt::format("image_decoder_{}", info.output_name),
                Arg("allocator", make_resource<holoscan::UnboundedAllocator>(
                                     fmt::format("decoder_pool_{}", info.output_name))),
                Arg("cuda_device_ordinal", 0),
                Arg("out_tensor_name", info.output_name),
                Arg("align_depth_to_rgb", false));
            // SIPL pads each line to its pitch; the decoder skips the padding as line-end bytes.
            const uint32_t line_bytes = info.width * 2;
            const uint32_t line_end = info.bytes_per_line > line_bytes ? info.bytes_per_line - line_bytes : 0;
            image_decoder->configure(info.width, info.height, pixel_format, info.offset, 0, 0, line_end);

            add_flow(sipl_capture, image_decoder, { { "output", "input" } });
            add_flow(image_decoder, visualizer, { { "output", "receivers" } });
        }
    }

private:
    std::string camera_config_;
    std::string json_config_;
    bool raw_output_;
    bool headless_;
    bool fullscreen_;
    int frame_limit_;
};

int main(int argc, char** argv)
{
    using namespace hololink::args;
    OptionsDescription options_description("Allowed options");

    hololink::logging::hsb_log_level = hololink::logging::HSB_LOG_LEVEL_DEBUG;

    // clang-format off
    options_description.add_options()
        ("camera-config", value<std::string>()->default_value(""), "Camera configuration to use")
        ("json-config", value<std::string>()->default_value(""), "JSON configuration file to use")
        ("list-configs", bool_switch()->default_value(false), "List available camera configurations then exit")
        ("raw", bool_switch()->default_value(true), "Use RAW capture path (uses CUDA-based ISP)")
        ("headless", bool_switch()->default_value(false), "Run in headless mode")
        ("fullscreen", bool_switch()->default_value(false), "Run in fullscreen mode")
        ("frame-limit", value<int>()->default_value(0), "Exit after receiving this many frames")
        ;
    // clang-format on

    auto variables_map = Parser().parse_command_line(argc, argv, options_description);

    try {
        if (variables_map["list-configs"].as<bool>()) {
            hololink::operators::D555SIPLCaptureOp::list_available_configs(
                variables_map["json-config"].as<std::string>());
        } else {
            auto app = std::make_unique<Application>(
                variables_map["camera-config"].as<std::string>(),
                variables_map["json-config"].as<std::string>(),
                variables_map["raw"].as<bool>(),
                variables_map["headless"].as<bool>(),
                variables_map["fullscreen"].as<bool>(),
                variables_map["frame-limit"].as<int>());
            app->run();
        }
        return 0;
    } catch (const std::exception& e) {
        HOLOSCAN_LOG_ERROR("Application failed: {}", e.what());
        return -1;
    }
}
