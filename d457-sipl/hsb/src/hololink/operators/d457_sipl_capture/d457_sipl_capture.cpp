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

#include "d457_sipl_capture.hpp"
#include "d457_sipl_fmt.hpp"

#include <cstdlib>
#include <variant>

#include <cuda.h>

namespace hololink::operators {

std::map<void*, nvsipl::INvSIPLClient::INvSIPLBuffer*> D457SIPLCaptureOp::pending_buffers_;
std::mutex D457SIPLCaptureOp::pending_buffers_mutex_;
std::condition_variable D457SIPLCaptureOp::pending_buffer_released_;

void D457SIPLCaptureOp::setup(holoscan::OperatorSpec& spec)
{
    spec.output<holoscan::gxf::Entity>("output");

    spec.param(capture_queue_depth_, "capture_queue_depth", "Capture Queue Depth",
        "Depth of the NvSIPL capture queue", 4U);
    spec.param(timeout_, "timeout", "Timeout",
        "Timeout for capture requests, in microseconds", 1000000U);
    spec.param(strict_, "strict", "Strict",
        "Throw on a per-camera buffer-wait timeout instead of skipping the tick", false);
}

nvidia::gxf::Expected<void> D457SIPLCaptureOp::buffer_release_callback(void* pointer)
{
    std::lock_guard<std::mutex> lock(pending_buffers_mutex_);
    auto status = pending_buffers_[pointer]->Release();
    if (status != nvsipl::NVSIPL_STATUS_OK) {
        HSB_LOG_ERROR("Failed to release buffer {}", (void*)pending_buffers_[pointer]);
    } else {
        HSB_LOG_TRACE("Released buffer {}", (void*)pending_buffers_[pointer]);
    }
    pending_buffers_.erase(pointer);
    pending_buffer_released_.notify_all();
    return nvidia::gxf::Expected<void>();
}

void D457SIPLCaptureOp::list_available_configs(const std::string& json_config)
{
    auto sipl_query = nvsipl::INvSIPLCameraQuery::GetInstance();
    if (!sipl_query) {
        throw std::runtime_error("Failed to get NvSIPLCameraQuery instance");
    }

    if (json_config != "") {
        if (sipl_query->ParseJsonFile(json_config) != nvsipl::NVSIPL_STATUS_OK) {
            throw std::runtime_error("Failed to parse NvSIPLCameraQuery database");
        }
    } else {
        if (sipl_query->ParseDatabase() != nvsipl::NVSIPL_STATUS_OK) {
            throw std::runtime_error("Failed to parse NvSIPLCameraQuery database");
        }
    }

    const auto& config_names = sipl_query->GetCameraConfigNames();
    if (config_names.empty()) {
        throw std::runtime_error("No camera configurations available");
    }

    std::set<std::string> unique_configs(config_names.begin(), config_names.end());
    if (config_names.size() != unique_configs.size()) {
        HSB_LOG_WARN("Duplicate camera configs found: {}", config_names);
    }

    std::cout << unique_configs.size() << " Available Camera Configurations:\n\n";
    for (const auto& config_name : unique_configs) {
        nvsipl::sensorconfig::SensorSystemConfig camera_config;
        auto status = sipl_query->GetSensorSystemConfig(config_name, camera_config);
        if (status != nvsipl::NVSIPL_STATUS_OK) {
            throw std::runtime_error("Failed to get sensor system config");
        }
        std::cout << fmt::format("{}: {} module(s), {} transport(s)\n", config_name,
            camera_config.modules.size(), camera_config.transports.size());
    }
}

std::string D457SIPLCaptureOp::stream_for_sensor(uint32_t total_cameras, uint32_t ordinal_in_module) const
{
    // Single-camera config: the driver selects the stream from D457_STREAM (or the `stream` param,
    // which we propagate to that env before Init). Label the one pipeline accordingly.
    if (total_cameras <= 1) {
        if (!stream_.empty()) {
            return stream_;
        }
        const char* s = std::getenv("D457_STREAM");
        return (s != nullptr && *s != '\0') ? std::string(s) : std::string("depth");
    }
    // Multi-camera config: convention 0=depth, 1=rgb, 2=ir, by ORDINAL WITHIN THE OWNING MODULE
    // (not a flat index across all modules/links -- see query/d457_query.cpp,
    // query/d457_query_4cam.cpp and uddf_driver/D457Sensor.cpp BuildStreamList).
    switch (ordinal_in_module) {
    case 0:
        return "depth";
    case 1:
        return "rgb";
    case 2:
        return "ir";
    default:
        return "depth";
    }
}

void D457SIPLCaptureOp::apply_link_offsets()
{
    // The query engine replicates the D457 module once per enabled GMSL link with IDENTICAL VCs and
    // i2c address; every link beyond 0 must be offset before SetPlatformCfg/Init so its pipelines
    // land on distinct Tegra VI virtual channels and reach that link's own DS5 over i2c. This is an
    // exact port of the nvsipl_camera app patch (sdk-patches/multicam-patches/
    // repatch2_nvsipl_main.sh) -- the formula MUST stay identical to that script's, since it has to
    // match the deser HSL pixel map (MAX967XXHsl.py) placement byte-for-byte, or specific streams
    // capture 0 frames.
    // Sensor ids are left untouched: nvsipl_camera's identical patch doesn't touch them either, and
    // Stage 2/3 both captured correct distinct per-stream frame counts through it, so SIPL's
    // internal per-sensor id allocation already differentiates replicated modules.
    for (auto& module : sipl_config_.modules) {
        if (!std::holds_alternative<nvsipl::sensorconfig::GmslModule>(module.moduleType)) {
            continue;
        }
        auto& gmsl = std::get<nvsipl::sensorconfig::GmslModule>(module.moduleType);
        const uint32_t link = gmsl.linkIndex;
        if (link == 0U || link == UINT32_MAX) {
            continue;
        }
        // Output-VC placement must match the deser HSL: keep each link's streams inside ONE 4-VC
        // extended-VC msb group and <= VC7 (Tegra VI ignores VC>=8). streams_per_link (SPL) is
        // derived from this module's own sensor count; links_per_group (LPG) = 4/SPL.
        const uint32_t streams_per_link = static_cast<uint32_t>(gmsl.sensorConfigs.size());
        const uint32_t links_per_group = (streams_per_link >= 1U && (4U / streams_per_link) >= 1U)
            ? (4U / streams_per_link)
            : 1U;
        const uint32_t voff = (link / links_per_group) * 4U + (link % links_per_group) * streams_per_link;
        for (auto& sensor_variant : gmsl.sensorConfigs) {
            std::visit([&](auto& sensor) {
                // 7-bit I2C address space: refuse rather than silently wrapping onto another device
                // on the shared bus.
                auto offset_addr = [link](uint16_t base, const char* what) -> uint16_t {
                    const uint32_t shifted = static_cast<uint32_t>(base) + link * 0x10U;
                    if (shifted > 0x7FU) {
                        throw std::runtime_error(fmt::format(
                            "link {} would push the {} i2c address to {:#x}, outside the 7-bit space",
                            link, what, shifted));
                    }
                    return static_cast<uint16_t>(shifted);
                };
                sensor.address.i2cAddress = offset_addr(sensor.address.i2cAddress, "sensor");
                if (sensor.address.virtualI2CAddress.has_value()) {
                    sensor.address.virtualI2CAddress =
                        offset_addr(sensor.address.virtualI2CAddress.value(), "virtual");
                }
                for (auto& vc : sensor.vcInfoList) {
                    if (vc.vcIdSrc != UINT32_MAX) {
                        vc.vcIdSrc += voff;
                    }
                    if (vc.vcIdDst != UINT32_MAX) {
                        vc.vcIdDst += voff;
                    }
                }
            },
                sensor_variant);
        }
    }
}

void D457SIPLCaptureOp::init_cameras()
{
    if (!initialized_) {
        init_nvsipl();
        init_nvsci();
        fill_camera_info();
        initialized_ = true;
    }
}

void D457SIPLCaptureOp::init_nvsipl()
{
    // D457_STREAM is the only channel to the UDDF camera driver: that is a separately dlopen'd .so
    // which reads it during Init(), so the `stream` parameter cannot simply be handed over in-process.
    // It is therefore genuinely shared process state, with the usual hazards -- it is inherited by
    // child processes, and two operator instances in one process would otherwise silently disagree
    // (last to init wins). Set it once and fail loudly on a conflict rather than picking a winner.
    if (!stream_.empty()) {
        const char* existing = std::getenv("D457_STREAM");
        if (existing != nullptr && *existing != '\0' && stream_ != existing) {
            throw std::runtime_error(fmt::format(
                "D457_STREAM is already '{}' but this operator wants '{}'; two D457SIPLCaptureOp "
                "instances cannot select different streams in one process",
                existing, stream_));
        }
        setenv("D457_STREAM", stream_.c_str(), 1);
    }

    sipl_query_ = nvsipl::INvSIPLCameraQuery::GetInstance();
    if (!sipl_query_) {
        throw std::runtime_error("Failed to get NvSIPLCameraQuery instance");
    }

    if (json_config_ != "") {
        HSB_LOG_DEBUG("Using JSON config: {}", json_config_);
        if (sipl_query_->ParseJsonFile(json_config_) != nvsipl::NVSIPL_STATUS_OK) {
            throw std::runtime_error("Failed to parse NvSIPLCameraQuery database");
        }
    } else {
        if (sipl_query_->ParseDatabase() != nvsipl::NVSIPL_STATUS_OK) {
            throw std::runtime_error("Failed to parse NvSIPLCameraQuery database");
        }
    }

    if (camera_config_ == "") {
        throw std::runtime_error(
            "No camera config specified; pass camera_config (e.g. \"D457_Camera\") or json_config. "
            "The D457 is a GMSL config and is not picked up by the CoE auto-select default.");
    }

    HSB_LOG_DEBUG("Using camera config: {}", camera_config_);
    auto status = sipl_query_->GetSensorSystemConfig(camera_config_, sipl_config_);
    if (status != nvsipl::NVSIPL_STATUS_OK) {
        throw std::runtime_error("Failed to get sensor system config");
    }

    // R39.2: GetSensorSystemConfig returns every GMSL link slot of the deserializer; ApplyMask filters
    // to the enabled link(s) -- equivalent to `nvsipl_camera -m <mask>`. The D457 is a single camera on
    // link 0, so link_mask_ (default 0x0001) selects it; one mask entry per transport/deserializer.
    std::vector<uint32_t> masks(sipl_config_.transports.size(), link_mask_);
    status = sipl_query_->ApplyMask(sipl_config_, masks);
    if (status != nvsipl::NVSIPL_STATUS_OK) {
        throw std::runtime_error("Failed to apply link mask to sensor system config");
    }
    HSB_LOG_DEBUG("After ApplyMask(0x{:x}): {} module(s)", link_mask_, sipl_config_.modules.size());

    // Multi-camera de-replication -- no-op for the single-link default (see apply_link_offsets()).
    apply_link_offsets();

    sipl_camera_ = nvsipl::INvSIPLCamera::GetInstance();
    if (!sipl_camera_) {
        throw std::runtime_error("Failed to get NvSIPLCamera instance");
    }

    status = sipl_camera_->SetPlatformCfg(sipl_config_);
    if (status != nvsipl::NVSIPL_STATUS_OK) {
        throw std::runtime_error("Failed to set NvSIPLCamera platform config");
    }

    // Flatten the (masked) GMSL modules into one capture pipeline per sensor. SIPL keys pipelines by
    // the sensor's id (CommonSensorConfig::id), which we also use for GetImageAttributes/RegisterImages.
    // Count CAMERA sensors only: the loop below skips non-camera entries (an IMU, say) without
    // incrementing camera_index, so counting every sensorConfigs entry here would both oversize
    // per_camera_state_ -- leaving trailing entries with sensor_id_ 0 and a default vc_, which
    // start() and fill_camera_info() would still walk -- and inflate the count handed to
    // stream_for_sensor(), skipping its single-camera branch on a module that has one camera plus
    // one other sensor.
    uint32_t total_cameras = 0;
    for (const auto& module : sipl_config_.modules) {
        if (!std::holds_alternative<nvsipl::sensorconfig::GmslModule>(module.moduleType)) {
            continue;
        }
        for (const auto& sensor_variant :
             std::get<nvsipl::sensorconfig::GmslModule>(module.moduleType).sensorConfigs) {
            if (std::holds_alternative<nvsipl::sensorconfig::GmslCameraSensorConfig>(sensor_variant)) {
                ++total_cameras;
            }
        }
    }
    per_camera_state_.resize(total_cameras);

    // RAW (ICP) capture only -- the D457 path has no Tegra ISP.
    nvsipl::NvSIPLPipelineConfiguration sipl_pipeline_config = {
        .captureOutputRequested = true,
        .isp0OutputRequested = false,
        .isp1OutputRequested = false,
        .isp2OutputRequested = false,
        .disableSubframe = true
    };
    uint32_t camera_index = 0;
    for (const auto& module : sipl_config_.modules) {
        if (!std::holds_alternative<nvsipl::sensorconfig::GmslModule>(module.moduleType)) {
            continue;
        }
        const auto& gmsl = std::get<nvsipl::sensorconfig::GmslModule>(module.moduleType);
        const uint32_t link = gmsl.linkIndex;
        uint32_t ordinal_in_module = 0;
        for (const auto& sensor_variant : gmsl.sensorConfigs) {
            if (!std::holds_alternative<nvsipl::sensorconfig::GmslCameraSensorConfig>(sensor_variant)) {
                continue; // non-camera sensor (e.g. IMU) in this module; not ours to configure
            }
            const auto& sensor = std::get<nvsipl::sensorconfig::GmslCameraSensorConfig>(sensor_variant);
            if (sensor.vcInfoList.empty()) {
                throw std::runtime_error("Sensor has no virtual-channel info");
            }
            auto& camera_state = per_camera_state_[camera_index];
            status = sipl_camera_->SetPipelineCfg(sensor.id, sipl_pipeline_config, camera_state.queues_);
            if (status != nvsipl::NVSIPL_STATUS_OK) {
                throw std::runtime_error("Failed to set NvSIPLCamera pipeline config");
            }
            const auto& vc = sensor.vcInfoList[0];
            camera_state.sensor_id_ = sensor.id;
            camera_state.link_ = link;
            camera_state.vc_ = vc;
            camera_state.stream_ = stream_for_sensor(total_cameras, ordinal_in_module);
            camera_state.output_name_ = fmt::format("cam{}_{}", link, camera_state.stream_);
            HSB_LOG_INFO(
                "pipeline[{}] link={} stream={} sensor_id={} vc(src={},dst={}) i2c=0x{:x}",
                camera_index, link, camera_state.stream_, sensor.id, vc.vcIdSrc, vc.vcIdDst,
                sensor.address.i2cAddress);
            ++camera_index;
            ++ordinal_in_module;
        }
    }

    // The two passes must agree, or per_camera_state_ has entries no pipeline was configured for.
    if (camera_index != total_cameras) {
        throw std::runtime_error(fmt::format(
            "configured {} camera pipelines but counted {}", camera_index, total_cameras));
    }

    status = sipl_camera_->Init();
    if (status != nvsipl::NVSIPL_STATUS_OK) {
        throw std::runtime_error(fmt::format(
            "Failed to initialize NvSIPLCamera (status = {})", static_cast<int>(status)));
    }
}

void D457SIPLCaptureOp::init_nvsci()
{
    NvSciError err;

    err = NvSciSyncModuleOpen(&sci_sync_module_);
    if (err != NvSciError_Success) {
        throw std::runtime_error("Failed to initialize NvSciSyncModule");
    }

    err = NvSciBufModuleOpen(&sci_buf_module_);
    if (err != NvSciError_Success) {
        throw std::runtime_error("Failed to initialize NvSciBufModule");
    }

    err = NvSciSyncCpuWaitContextAlloc(sci_sync_module_, &cpu_wait_context_);
    if (err != NvSciError_Success) {
        throw std::runtime_error("Failed to allocate NvSciCpuWaitContext");
    }
}

void D457SIPLCaptureOp::fill_camera_info()
{
    camera_info_.resize(per_camera_state_.size());
    for (uint32_t camera_index = 0; camera_index < camera_info_.size(); ++camera_index) {
        std::unique_ptr<NvSciBufAttrList> attr_list(new NvSciBufAttrList());
        auto err = NvSciBufAttrListCreate(sci_buf_module_, attr_list.get());
        if (err != NvSciError_Success) {
            throw std::runtime_error("Failed to create NvSciBufAttrList");
        }

        const auto& camera = per_camera_state_[camera_index];
        const auto& vc = camera.vc_;
        auto status = sipl_camera_->GetImageAttributes(camera.sensor_id_,
            nvsipl::INvSIPLClient::ConsumerDesc::OutputType::ICP, *(attr_list.get()));
        if (status != nvsipl::NVSIPL_STATUS_OK) {
            throw std::runtime_error(fmt::format("Failed to get image attributes ({})", static_cast<int>(status)));
        }

        std::unique_ptr<NvSciBufAttrList> reconciled_attr_list(new NvSciBufAttrList());
        std::unique_ptr<NvSciBufAttrList> conflict_attr_list(new NvSciBufAttrList());
        err = NvSciBufAttrListReconcile(attr_list.get(), 1U,
            reconciled_attr_list.get(), conflict_attr_list.get());
        if (err != NvSciError_Success) {
            throw std::runtime_error("Failed to reconcile NvSciBuf attributes");
        }

        NvSciBufAttrKeyValuePair img_attrs[] = { { NvSciBufImageAttrKey_PlanePitch, NULL, 0 } };
        err = NvSciBufAttrListGetAttrs(*reconciled_attr_list, img_attrs, 1);
        if (err != NvSciError_Success) {
            throw std::runtime_error("Failed to get buffer attributes");
        }
        const uint32_t plane_pitch = *(static_cast<const uint32_t*>(img_attrs[0].value));

        auto& info = camera_info_[camera_index];
        info.output_name = camera.output_name_;
        info.stream = camera.stream_;
        info.link = camera.link_;
        info.offset = 0;
        info.width = vc.resolution.width;
        info.height = vc.resolution.height;
        info.bytes_per_line = plane_pitch;

        // Pixel format -- the D457 delivers every stream as unpacked RAW16 (DT 0x2E).
        switch (vc.inputFormat) {
        case NVSIPL_CAP_INPUT_FORMAT_TYPE_RAW16:
            info.pixel_format = hololink::csi::PixelFormat::RAW_16;
            info.offset = (info.width * 2) * vc.embeddedTopLines;
            break;
        case NVSIPL_CAP_INPUT_FORMAT_TYPE_RAW10:
            info.pixel_format = hololink::csi::PixelFormat::RAW_10;
            info.offset = ((info.width * 10) / 8) * vc.embeddedTopLines;
            break;
        case NVSIPL_CAP_INPUT_FORMAT_TYPE_RAW12:
            info.pixel_format = hololink::csi::PixelFormat::RAW_12;
            info.offset = ((info.width * 12) / 8) * vc.embeddedTopLines;
            break;
        default:
            throw std::runtime_error("Unsupported input format");
        }

        // Bayer order (load-bearing: the D457 query reports rggb; the field must be set even though
        // the D457 path does not demosaic).
        switch (vc.cfa) {
        case NVSIPL_PIXEL_ORDER_RGGB:
            info.bayer_format = hololink::csi::BayerFormat::RGGB;
            break;
        case NVSIPL_PIXEL_ORDER_BGGR:
            info.bayer_format = hololink::csi::BayerFormat::BGGR;
            break;
        case NVSIPL_PIXEL_ORDER_GRBG:
            info.bayer_format = hololink::csi::BayerFormat::GRBG;
            break;
        case NVSIPL_PIXEL_ORDER_GBRG:
            info.bayer_format = hololink::csi::BayerFormat::GBRG;
            break;
        default:
            throw std::runtime_error("Unsupported color filter array");
        }

        HSB_LOG_DEBUG("camera[{}] output={} stream={} {}x{} pitch={} offset={}",
            camera_index, info.output_name, info.stream, info.width, info.height,
            info.bytes_per_line, info.offset);
    }
}

void D457SIPLCaptureOp::allocate_buffers(uint32_t camera_index, nvsipl::INvSIPLClient::ConsumerDesc::OutputType output_type, std::vector<NvSciBufObj>& bufs)
{
    struct CloseNvSciBufAttrList {
        void operator()(NvSciBufAttrList* attr_list) const
        {
            if (attr_list != nullptr) {
                if ((*attr_list) != nullptr) {
                    NvSciBufAttrListFree(*attr_list);
                }
                delete attr_list;
            }
        }
    };

    std::unique_ptr<NvSciBufAttrList, CloseNvSciBufAttrList> attr_list;
    attr_list.reset(new NvSciBufAttrList());
    NvSciError err = NvSciBufAttrListCreate(sci_buf_module_, attr_list.get());
    if (err != NvSciError_Success) {
        throw std::runtime_error("Failed to create NvSciBufAttrList");
    }

    constexpr NvSciBufType buf_type = NvSciBufType_Image;
    constexpr NvSciBufAttrValAccessPerm access_perm = NvSciBufAccessPerm_Readonly;

    // Map buffers into the GPU.
    CUuuid uuid;
    cuDeviceGetUuid_v2(&uuid, 0);
    NvSciRmGpuId gpu_id = { 0 };
    memcpy(&gpu_id.bytes, uuid.bytes, sizeof(uuid.bytes));

    NvSciBufAttrKeyValuePair attr_kvp[] = {
        { NvSciBufGeneralAttrKey_Types, &buf_type, sizeof(buf_type) },
        { NvSciBufGeneralAttrKey_RequiredPerm, &access_perm, sizeof(access_perm) },
        { NvSciBufGeneralAttrKey_GpuId, &gpu_id, sizeof(gpu_id) },
    };

    err = NvSciBufAttrListSetAttrs(*(attr_list.get()), attr_kvp,
        sizeof(attr_kvp) / sizeof(attr_kvp[0]));
    if (err != NvSciError_Success) {
        throw std::runtime_error("Failed to set NvSciBufAttrList values");
    }

    const auto sensor_id = per_camera_state_[camera_index].sensor_id_;
    auto status = sipl_camera_->GetImageAttributes(sensor_id, output_type, *(attr_list.get()));
    if (status != nvsipl::NVSIPL_STATUS_OK) {
        throw std::runtime_error("Failed to get image attributes");
    }

    std::unique_ptr<NvSciBufAttrList, CloseNvSciBufAttrList> reconciled_attr_list;
    std::unique_ptr<NvSciBufAttrList, CloseNvSciBufAttrList> conflict_attr_list;
    reconciled_attr_list.reset(new NvSciBufAttrList());
    conflict_attr_list.reset(new NvSciBufAttrList());
    err = NvSciBufAttrListReconcile(attr_list.get(), 1U,
        reconciled_attr_list.get(), conflict_attr_list.get());
    if (err != NvSciError_Success) {
        throw std::runtime_error("Failed to reconcile NvSciBuf attributes");
    }

    for (size_t i = 0; i < capture_queue_depth_.get(); i++) {
        NvSciBufObj buf_obj;
        err = NvSciBufObjAlloc(*(reconciled_attr_list.get()), &buf_obj);
        if (err != NvSciError_Success || !buf_obj) {
            throw std::runtime_error("Failed to allocate NvSciBufObj");
        }
        bufs.push_back(buf_obj);
    }

    status = sipl_camera_->RegisterImages(sensor_id, output_type, bufs);
    if (status != nvsipl::NVSIPL_STATUS_OK) {
        throw std::runtime_error(fmt::format("Failed to register images with NvSIPLCamera (status = {})",
            static_cast<int>(status)));
    }

    HSB_LOG_DEBUG("Allocated and registered {} buffers for output type {}",
        bufs.size(), static_cast<uint32_t>(output_type));
}

void D457SIPLCaptureOp::free_buffers(std::vector<NvSciBufObj>& bufs)
{
    for (auto buf : bufs) {
        NvSciBufObjFree(buf);
    }
    bufs.clear();
}

void D457SIPLCaptureOp::start()
{
    init_cameras();

    for (uint32_t camera_index = 0; camera_index < per_camera_state_.size(); ++camera_index) {
        auto& camera_state = per_camera_state_[camera_index];
        allocate_buffers(camera_index, nvsipl::INvSIPLClient::ConsumerDesc::OutputType::ICP, camera_state.sci_bufs_icp_);
    }
}

void D457SIPLCaptureOp::stop()
{
    sipl_camera_->Stop();

    for (auto& camera_state : per_camera_state_) {
        if (camera_state.acquire_thread_.joinable()) {
            camera_state.stop_thread_->store(true);
            camera_state.acquire_thread_.join();
        }
    }

    // Wait for any pending buffers to be released.
    uint32_t wait_limit = 30;
    std::unique_lock<std::mutex> pending_buffer_lock(pending_buffers_mutex_);
    while (pending_buffers_.size() > 0) {
        auto status = pending_buffer_released_.wait_for(pending_buffer_lock,
            std::chrono::milliseconds(100));
        if (status == std::cv_status::timeout && (--wait_limit == 0)) {
            HSB_LOG_ERROR("Failed to wait for pending buffers to be released.");
            break;
        }
    }
    pending_buffer_lock.unlock();

    for (auto mapping : cuda_mappings_) {
        cudaFree(mapping.second.ptr_);
        cudaDestroyExternalMemory(mapping.second.mem_);
    }
    cuda_mappings_.clear();

    for (auto& camera_state : per_camera_state_) {
        free_buffers(camera_state.sci_bufs_icp_);
    }
    per_camera_state_.clear();

    if (cpu_wait_context_) {
        NvSciSyncCpuWaitContextFree(cpu_wait_context_);
        cpu_wait_context_ = nullptr;
    }
    if (sci_sync_module_) {
        NvSciSyncModuleClose(sci_sync_module_);
        sci_sync_module_ = nullptr;
    }
    if (sci_buf_module_) {
        NvSciBufModuleClose(sci_buf_module_);
        sci_buf_module_ = nullptr;
    }

    sipl_camera_.reset();
    sipl_query_.reset();
}

void D457SIPLCaptureOp::compute(holoscan::InputContext& op_input,
    holoscan::OutputContext& op_output,
    holoscan::ExecutionContext& context)
{
    // Streaming is started on the first compute() so the capture queue doesn't fill while other
    // operators are still initializing.
    if (!streaming_) {
        HSB_LOG_DEBUG("Starting streaming");
        for (auto& camera_state : per_camera_state_) {
            camera_state.acquire_thread_ = std::thread(&D457SIPLCaptureOp::acquire_buffer_thread_func,
                this, &camera_state);
        }
        auto status = sipl_camera_->Start();
        if (status != nvsipl::NVSIPL_STATUS_OK) {
            // A transient failure on the very first Start() (post-boot NvSci/GPU-context settling
            // -- a known, rig-documented gotcha*.md) is worth one
            // automatic retry rather than surfacing a hard failure for a condition that clears
            // itself a moment later.
            HSB_LOG_WARN("sipl_camera_->Start() failed (status={}); retrying once", static_cast<int>(status));
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            status = sipl_camera_->Start();
            if (status != nvsipl::NVSIPL_STATUS_OK) {
                throw std::runtime_error(
                    fmt::format("Failed to start streaming (status = {}) after retry", static_cast<int>(status)));
            }
        }
        streaming_ = true;
    }

    auto entity = holoscan::gxf::Entity::New(&context);
    for (auto& camera_state : per_camera_state_) {
        // Wait for and take ownership of the next buffer from the acquire thread.
        std::unique_lock<std::mutex> buffer_state_lock(*camera_state.buffer_mutex_.get());
        while (camera_state.buffer_raw_ == nullptr) {
            auto status = camera_state.buffer_available_->wait_for(buffer_state_lock,
                std::chrono::microseconds(timeout_.get()));
            if (status == std::cv_status::timeout) {
                if (strict_.get()) {
                    throw std::runtime_error(fmt::format("Failed to get buffer for {}", camera_state.output_name_));
                }
                // Skip the WHOLE tick (every camera, not just the stalled one) rather than emitting
                // a partial entity -- every convert op downstream subscribes to the same "output"
                // port and expects its own named tensor to be present every message. Any earlier
                // cameras in this loop already got fully wrapped into `entity`'s scope; letting it
                // fall out of scope here still runs their buffer_release_callback (registered by
                // wrapMemory), so nothing leaks -- this tick's frames for those cameras are just
                // dropped, not lost track of.
                HSB_LOG_WARN("Timed out waiting for buffer for {}; skipping this tick",
                    camera_state.output_name_);
                return;
            }
        }
        auto buffer = camera_state.buffer_raw_;
        camera_state.buffer_raw_ = nullptr;
        buffer_state_lock.unlock();

        auto nvm_buffer = dynamic_cast<nvsipl::INvSIPLClient::INvSIPLNvMBuffer*>(buffer);
        if (nvm_buffer == nullptr) {
            throw std::runtime_error("Failed to get INvSIPLNvMBuffer");
        }

        // Get and wait for the EOF fence (if there is one).
        NvSciSyncFence fence = NvSciSyncFenceInitializer;
        auto status = nvm_buffer->GetEOFNvSciSyncFence(&fence);
        if (status == nvsipl::NVSIPL_STATUS_OK) {
            auto err = NvSciSyncFenceWait(&fence, cpu_wait_context_, timeout_.get());
            if (err != NvSciError_Success) {
                NvSciSyncFenceClear(&fence);
                throw std::runtime_error("Failed to wait for EOF fence");
            }
        }
        NvSciSyncFenceClear(&fence);

        NvSciBufObj buf_obj = nvm_buffer->GetNvSciBufImage();
        if (buf_obj == nullptr) {
            throw std::runtime_error("Failed to get NvSciBufObj");
        }

        NvSciBufAttrList buf_attr_list;
        auto err = NvSciBufObjGetAttrList(buf_obj, &buf_attr_list);
        if (err != NvSciError_Success) {
            throw std::runtime_error("Failed to get buffer attribute list");
        }

        NvSciBufAttrKeyValuePair img_attrs[] = {
            { NvSciBufImageAttrKey_Size, NULL, 0 }, // 0
            { NvSciBufImageAttrKey_PlaneCount, NULL, 0 }, // 1
            { NvSciBufImageAttrKey_PlanePitch, NULL, 0 }, // 2
            { NvSciBufImageAttrKey_PlaneWidth, NULL, 0 }, // 3
            { NvSciBufImageAttrKey_PlaneHeight, NULL, 0 }, // 4
            { NvSciBufImageAttrKey_PlaneColorFormat, NULL, 0 }, // 5
        };
        size_t num_attrs = sizeof(img_attrs) / sizeof(img_attrs[0]);
        err = NvSciBufAttrListGetAttrs(buf_attr_list, img_attrs, num_attrs);
        if (err != NvSciError_Success) {
            throw std::runtime_error("Failed to get buffer attributes");
        }

        const uint64_t size = *(static_cast<const uint64_t*>(img_attrs[0].value));
        const uint32_t plane_count = *(static_cast<const uint32_t*>(img_attrs[1].value));
        const uint32_t* plane_pitch = static_cast<const uint32_t*>(img_attrs[2].value);
        const uint32_t* plane_width = static_cast<const uint32_t*>(img_attrs[3].value);
        const uint32_t* plane_height = static_cast<const uint32_t*>(img_attrs[4].value);
        const NvSciBufAttrValColorFmt* plane_color_format = static_cast<const NvSciBufAttrValColorFmt*>(img_attrs[5].value);

        HSB_LOG_TRACE("Got buffer; output={} stream={} planes={} size={} plane0 {}x{} pitch={} fmt={}",
            camera_state.output_name_, camera_state.stream_, plane_count, size,
            plane_width[0], plane_height[0], plane_pitch[0],
            fmt::format("{}", plane_color_format[0]));

        // The D457 delivers unpacked RAW16 (DT 0x2E). Accept the Bayer16 formats (cfa=rggb -> RGGB) and
        // Y16 defensively. Anything else is unexpected for this operator.
        const bool is_raw16 = plane_color_format[0] == NvSciColor_Bayer16RGGB
            || plane_color_format[0] == NvSciColor_Bayer16BGGR
            || plane_color_format[0] == NvSciColor_Bayer16GRBG
            || plane_color_format[0] == NvSciColor_Bayer16GBRG
            || plane_color_format[0] == NvSciColor_Y16;
        if (!is_raw16) {
            throw std::runtime_error(fmt::format("Buffer has unsupported color format: {}",
                fmt::format("{}", plane_color_format[0])));
        }

        // Map the buffer into CUDA (once per buffer object).
        if (cuda_mappings_.find(buf_obj) == cuda_mappings_.end()) {
            CudaBufferMapping mapping;

            cudaExternalMemoryHandleDesc ext_mem_handle_desc;
            memset(&ext_mem_handle_desc, 0, sizeof(ext_mem_handle_desc));
            ext_mem_handle_desc.type = cudaExternalMemoryHandleTypeNvSciBuf;
            ext_mem_handle_desc.handle.nvSciBufObject = buf_obj;
            ext_mem_handle_desc.size = size;
            auto cuda_err = cudaImportExternalMemory(&mapping.mem_, &ext_mem_handle_desc);
            if (cuda_err != cudaSuccess) {
                throw std::runtime_error("Failed to import NvSciBuf into CUDA");
            }

            cudaExternalMemoryBufferDesc buffer_desc;
            memset(&buffer_desc, 0, sizeof(buffer_desc));
            buffer_desc.offset = 0;
            buffer_desc.size = size;
            cuda_err = cudaExternalMemoryGetMappedBuffer(&mapping.ptr_, mapping.mem_, &buffer_desc);
            if (cuda_err != cudaSuccess) {
                throw std::runtime_error("Failed to map NvSciBuf into CUDA");
            }

            cuda_mappings_[buf_obj] = mapping;
            HSB_LOG_DEBUG("Mapped buffer ({}) into CUDA (mem={} / ptr={})",
                (void*)buf_obj, (void*)mapping.mem_, (void*)mapping.ptr_);
        }

        const auto name = camera_state.output_name_.c_str();

        // Wrap the raw buffer as a flat uint8 tensor (the proven RAW10 pattern; avoids non-contiguous
        // stride pitfalls). The consumer reshapes to [height, bytes_per_line] using CameraInfo.
        auto tensor = static_cast<nvidia::gxf::Entity&>(entity).add<nvidia::gxf::Tensor>(name);
        if (!tensor) {
            throw std::runtime_error("Failed to add GXF Tensor");
        }
        nvidia::gxf::Shape shape { static_cast<int>(size) };
        const auto element_type = nvidia::gxf::PrimitiveType::kUnsigned8;
        const auto element_size = nvidia::gxf::PrimitiveTypeSize(element_type);
        if (!tensor.value()->wrapMemory(shape, element_type, element_size,
                nvidia::gxf::ComputeTrivialStrides(shape, element_size),
                nvidia::gxf::MemoryStorageType::kDevice,
                cuda_mappings_[buf_obj].ptr_, buffer_release_callback)) {
            throw std::runtime_error("Failed to add wrapped memory");
        }

        std::lock_guard<std::mutex> lock(pending_buffers_mutex_);
        pending_buffers_[cuda_mappings_[buf_obj].ptr_] = buffer;
        HSB_LOG_TRACE("Output buffer {} ({} pending)", static_cast<void*>(buffer), pending_buffers_.size());
    }

    op_output.emit(entity, "output");
}

const std::vector<D457SIPLCaptureOp::CameraInfo>& D457SIPLCaptureOp::get_camera_info()
{
    init_cameras();
    return camera_info_;
}

void D457SIPLCaptureOp::acquire_buffer_thread_func(PerCameraState* state)
{
    HSB_LOG_DEBUG("Starting acquire buffer thread for camera: {}", state->output_name_);

    while (!state->stop_thread_->load()) {
        constexpr uint32_t timeout = 100000; // 100ms

        nvsipl::INvSIPLClient::INvSIPLBuffer* buffer_raw = nullptr;
        auto status = state->queues_.captureCompletionQueue->Get(buffer_raw, timeout);
        if (status != nvsipl::NVSIPL_STATUS_OK) {
            if (status == nvsipl::NVSIPL_STATUS_TIMED_OUT) {
                HSB_LOG_WARN("Timeout getting RAW buffer for {}", state->output_name_);
                continue;
            }
            HSB_LOG_ERROR("Failed to get RAW buffer for {} (status = {})",
                state->output_name_, static_cast<int>(status));
            continue;
        }

        std::lock_guard<std::mutex> lock(*state->buffer_mutex_.get());
        if (state->buffer_raw_) {
            state->buffer_raw_->Release();
        }
        state->buffer_raw_ = buffer_raw;
        state->buffer_available_->notify_all();
    }

    std::lock_guard<std::mutex> lock(*state->buffer_mutex_.get());
    if (state->buffer_raw_) {
        state->buffer_raw_->Release();
        state->buffer_raw_ = nullptr;
    }

    HSB_LOG_DEBUG("Stopped acquire buffer thread for camera: {}", state->output_name_);
}

} // namespace hololink::operators
