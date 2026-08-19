/*
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

#pragma once

#include <vector>

#include <hololink/core/csi_controller.hpp>
#include <holoscan/holoscan.hpp>

#include <NvSIPLCamera.hpp>
#include <NvSIPLCameraQuery.hpp>

namespace hololink::operators {

/**
 * @brief Operator that captures the RealSense D457 (over GMSL via NvSIPL) into a Holoscan pipeline.
 *
 * Unlike the stock SIPLCaptureOp, the D457 delivers every stream (depth Z16 / RGB YUYV / IR Y8I) as
 * RAW16 (MIPI DT 0x2E) with no Tegra ISP. This operator captures the raw (ICP) output only and emits
 * each pipeline's frame as a flat uint8 GXF Tensor (keyed by CameraInfo::output_name); per-stream
 * interpretation/colorization is the consumer's job (see examples/d457_sipl_player.py).
 *
 * It uses the same NvSIPL query DB + UDDF driver as the reference `nvsipl_camera` client; the
 * `D457_STREAM` / `D457_STREAMS` env vars (read by the driver) select the stream(s).
 */
class D457SIPLCaptureOp : public holoscan::Operator {
public:
    HOLOSCAN_OPERATOR_FORWARD_ARGS(D457SIPLCaptureOp)

    // Constructor with camera/JSON config + optional stream selection + link mask, since these are
    // required by methods called before holoscan::Parameter parsing.
    template <typename... ArgsT>
    explicit D457SIPLCaptureOp(const std::string& camera_config, const std::string& json_config,
        const std::string& stream, uint32_t link_mask, ArgsT&&... args)
        : holoscan::Operator(std::forward<ArgsT>(args)...)
        , camera_config_(camera_config)
        , json_config_(json_config)
        , stream_(stream)
        , link_mask_(link_mask)
    {
    }

    void setup(holoscan::OperatorSpec& spec) override;
    void start() override;
    void stop() override;
    void compute(holoscan::InputContext& op_input,
        holoscan::OutputContext& op_output,
        holoscan::ExecutionContext& context) override;

    // This structure provides camera details to an application that may be needed for
    // post-processing. `stream` is the D457 stream this pipeline carries ("depth"|"rgb"|"ir");
    // `link` is the GMSL link (physical camera) index this pipeline belongs to -- 0 for a
    // single-camera config, 0..N-1 for a multi-camera config (see apply_link_offsets()).
    struct CameraInfo {
        std::string output_name;
        std::string stream;
        uint32_t link;
        uint32_t offset;
        uint32_t width;
        uint32_t height;
        uint32_t bytes_per_line;
        hololink::csi::PixelFormat pixel_format;
        hololink::csi::BayerFormat bayer_format;
    };
    const std::vector<CameraInfo>& get_camera_info();

    static void list_available_configs(const std::string& json_config = "");

    static nvidia::gxf::Expected<void> buffer_release_callback(void* pointer);

private:
    struct PerCameraState {
        PerCameraState()
            : stop_thread_(std::make_unique<std::atomic<bool>>(false))
            , buffer_mutex_(std::make_unique<std::mutex>())
            , buffer_available_(std::make_unique<std::condition_variable>())
            , buffer_raw_(nullptr)
        {
        }

        std::string output_name_;
        std::string stream_;
        nvsipl::NvSIPLPipelineQueues queues_;
        std::vector<NvSciBufObj> sci_bufs_icp_;

        std::thread acquire_thread_;
        std::unique_ptr<std::atomic<bool>> stop_thread_;
        std::unique_ptr<std::mutex> buffer_mutex_;
        std::unique_ptr<std::condition_variable> buffer_available_;
        nvsipl::INvSIPLClient::INvSIPLBuffer* buffer_raw_;

        // R39.2: the SIPL pipeline index / sensor id this pipeline drives, and the resolved
        // per-sensor virtual-channel info (resolution / inputFormat / embeddedTopLines / cfa).
        uint32_t sensor_id_ = 0;
        uint32_t link_ = 0;
        nvsipl::sensorconfig::CommonSensorConfig::VirtualChannelInfo vc_;
    };

    void init_cameras();
    void init_nvsipl();
    void init_nvsci();
    void fill_camera_info();
    void allocate_buffers(uint32_t camera_index, nvsipl::INvSIPLClient::ConsumerDesc::OutputType output_type, std::vector<NvSciBufObj>& bufs);
    void free_buffers(std::vector<NvSciBufObj>& bufs);
    void acquire_buffer_thread_func(PerCameraState* camera_state);
    std::string stream_for_sensor(uint32_t total_cameras, uint32_t ordinal_in_module) const;
    void apply_link_offsets();

    std::string camera_config_;
    std::string json_config_;
    std::string stream_;

    holoscan::Parameter<uint32_t> capture_queue_depth_;
    holoscan::Parameter<uint32_t> timeout_;
    // Default (false): a per-camera buffer-wait timeout logs a WARN and skips emitting this tick
    // (every camera's tensor together, so no convert op downstream ever sees a partial/missing
    // input) rather than throwing -- one stalled stream no longer kills the whole multi-camera
    // view. Set true to restore the original fail-fast behavior.
    holoscan::Parameter<bool> strict_;

    std::unique_ptr<nvsipl::INvSIPLCameraQuery> sipl_query_;
    nvsipl::sensorconfig::SensorSystemConfig sipl_config_;
    std::unique_ptr<nvsipl::INvSIPLCamera> sipl_camera_;

    // GMSL deserializer link-enable mask (R39.2 query->ApplyMask), one nibble per link (e.g. 0x0011
    // = links 0+1). Defaults to link 0 only (single D457); set via the constructor for multi-camera
    // configs (see apply_link_offsets() -- the query replicates the module per enabled link with
    // identical VCs/i2c, so every link beyond 0 needs its pipelines offset before Init()).
    uint32_t link_mask_ = 0x0001;

    NvSciBufModule sci_buf_module_;
    NvSciSyncModule sci_sync_module_;
    NvSciSyncCpuWaitContext cpu_wait_context_;

    std::vector<PerCameraState> per_camera_state_;
    std::vector<CameraInfo> camera_info_;

    struct CudaBufferMapping {
        cudaExternalMemory_t mem_;
        void* ptr_;
    };
    std::map<NvSciBufObj, CudaBufferMapping> cuda_mappings_;

    bool initialized_ = false;
    bool streaming_ = false;

    // Pending buffer map is static since the release callback only provides the buffer pointer.
    static std::map<void*, nvsipl::INvSIPLClient::INvSIPLBuffer*> pending_buffers_;
    static std::mutex pending_buffers_mutex_;
    static std::condition_variable pending_buffer_released_;
};

} // namespace hololink::operators
