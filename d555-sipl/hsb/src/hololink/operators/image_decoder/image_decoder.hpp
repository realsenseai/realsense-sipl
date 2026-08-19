/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef SRC_OPERATORS_IMAGE_DECODER_IMAGE_DECODER
#define SRC_OPERATORS_IMAGE_DECODER_IMAGE_DECODER

#include <memory>

#include <hololink/core/csi_formats.hpp>

#include <holoscan/core/operator.hpp>
#include <holoscan/core/parameter.hpp>
#include <holoscan/utils/cuda_stream_handler.hpp>

#include <cuda.h>

namespace hololink::common {

class CudaFunctionLauncher;

} // namespace hololink::common

namespace hololink::operators {

class ImageDecoder : public holoscan::Operator {
public:
    HOLOSCAN_OPERATOR_FORWARD_ARGS(ImageDecoder);

    struct float3 {
        float x, y, z;
        float3 operator*(float t) const { return {x * t, y * t, z * t}; }
        float3 operator+(const float3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    };

    struct rs2_intrinsics {
        int width;
        int height;
        float ppx;
        float ppy;
        float fx;
        float fy;
        float coeffs[5];
        int model;
    };

    struct rs2_extrinsics {
        float rotation[9];
        float translation[3];
    };

    void start() override;
    void stop() override;
    void setup(holoscan::OperatorSpec& spec) override;
    void compute(holoscan::InputContext&, holoscan::OutputContext& op_output,
        holoscan::ExecutionContext&) override;

    void configure(uint32_t width, uint32_t height, hololink::csi::PixelFormat pixel_format,
        uint32_t frame_start_size, uint32_t frame_end_size, uint32_t line_start_size,
        uint32_t line_end_size, uint32_t margin_left = 0, uint32_t margin_top = 0,
        uint32_t margin_right = 0, uint32_t margin_bottom = 0);
    size_t get_csi_length();
    
    // Set intrinsics/extrinsics using raw values
    void set_depth_intrinsics(int width, int height, float ppx, float ppy, float fx, float fy);
    void set_rgb_intrinsics(int width, int height, float ppx, float ppy, float fx, float fy);
    void set_extrinsics(const float rotation[9], const float translation[3]);
    
    // Set intrinsics/extrinsics using structs (kept for compatibility)
    void set_depth_intrinsics(const rs2_intrinsics& intrinsics);
    void set_rgb_intrinsics(const rs2_intrinsics& intrinsics);
    void set_extrinsics(const rs2_extrinsics& extrinsics);

private:
    void initialize_alignment_resources();
    // Sizes d_aligned_depth_ from the depth dims actually in use; see the definition.
    void ensure_aligned_depth_buffer();
    // Re-uploads the host intrinsics/extrinsics whenever a setter has run since the last upload.
    // The setters only touch the host copies, so without this an app that calls set_rgb_intrinsics()
    // after start() reprojects against zeroed device intrinsics and produces a silently black frame.
    void upload_alignment_calibration_if_dirty();
    holoscan::Parameter<std::shared_ptr<holoscan::Allocator>> allocator_;
    holoscan::Parameter<int> cuda_device_ordinal_;
    holoscan::Parameter<std::string> out_tensor_name_;
    holoscan::Parameter<bool> align_depth_to_rgb;
    // Histogram equalisation of the colorised depth. The histogram pass costs a 256KB memset, a
    // full-frame compute_histogram and a single-thread 65536-iteration prefix sum per frame, so it
    // is only run when this is on. Ignored on the aligned path, which does not use the histogram.
    holoscan::Parameter<bool> equalize_depth;

    CUcontext cuda_context_ = nullptr;
    CUdevice cuda_device_ = 0;
    bool is_integrated_ = false;
    bool host_memory_warning_ = false;

    holoscan::CudaStreamHandler cuda_stream_handler_;

    std::shared_ptr<hololink::common::CudaFunctionLauncher> cuda_function_launcher_;

    uint32_t width_ = 0;
    uint32_t height_ = 0;
    hololink::csi::PixelFormat pixel_format_ = hololink::csi::PixelFormat::INVALID;
    uint32_t frame_start_size_ = 0;
    uint32_t frame_end_size_ = 0;
    uint32_t line_start_size_ = 0;
    uint32_t line_end_size_ = 0;

    uint32_t bytes_per_line_ = 0;
    size_t csi_length_ = 0;

    int* d_hist_ = nullptr;
    float3* d_colormap_ = nullptr;
    size_t colormap_size_ = 0;

    // Element type must match the kernels, which take `uint32_t* aligned_depth`.
    uint32_t* d_aligned_depth_ = nullptr;
    size_t aligned_depth_elems_ = 0;

    rs2_intrinsics h_depth_intrin_{};
    rs2_intrinsics h_rgb_intrin_{};
    rs2_extrinsics h_extrinsics_{};

    rs2_intrinsics* d_depth_intrinsics_ = nullptr;
    rs2_intrinsics* d_rgb_intrinsics_ = nullptr;
    rs2_extrinsics* d_depth_to_rgb_extrinsics_ = nullptr;

    // Set by every intrinsics/extrinsics setter, cleared once the device copies are refreshed.
    bool alignment_calibration_dirty_ = true;

};

} // namespace hololink::operators

#endif /* SRC_OPERATORS_IMAGE_DECODER_IMAGE_DECODER */
