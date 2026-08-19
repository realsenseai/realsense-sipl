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

#include "image_decoder.hpp"

#include <hololink/core/logging_internal.hpp>
#include <hololink/common/cuda_helper.hpp>
#include <holoscan/holoscan.hpp>
#include <holoscan/utils/cuda_stream_handler.hpp>

namespace {
const char* source = R"(
extern "C" {

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;


// Field order and types MUST match ImageDecoder::rs2_intrinsics in image_decoder.hpp: this struct
// is filled by a straight cudaMemcpy of the host one, so a mismatch is silent (both are the same
// size) and only harmless while the kernels ignore coeffs/model.
typedef struct {
    int width;
    int height;
    float ppx;
    float ppy;
    float fx;
    float fy;
    float coeffs[5]; // not used
    int model;       // not used
} rs2_intrinsics;

typedef struct {
    float rotation[9];
    float translation[3];
} rs2_extrinsics;


__global__ void frameReconstructionZ16(unsigned short* out,
                                       const unsigned char* in,
                                       int per_line_size,
                                       int width,
                                       int height)
{
    int idx_x = blockIdx.x * blockDim.x + threadIdx.x;
    int idx_y = blockIdx.y * blockDim.y + threadIdx.y;
    if ((idx_x >= width) || (idx_y >= height)) return;
    int out_index = idx_y * width + idx_x;
    int in_index = (per_line_size * idx_y) + idx_x * 2;
    unsigned short val = static_cast<unsigned short>(in[in_index]) |
                         (static_cast<unsigned short>(in[in_index + 1]) << 8);
    out[out_index] = val;
}

__global__ void frameReconstructionYUYV(uint8_t* out_rgb,
                                        const uint8_t* in_yuyv,
                                        int per_line_size,
                                        int width,
                                        int height) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if ((x >= width) || (y >= height)) return;
    int pixel_pair_idx = x / 2;
    int in_idx = y * per_line_size + pixel_pair_idx * 4;
    uint8_t Y0 = in_yuyv[in_idx + 0];
    uint8_t U  = in_yuyv[in_idx + 1];
    uint8_t Y1 = in_yuyv[in_idx + 2];
    uint8_t V  = in_yuyv[in_idx + 3];
    int c = x % 2;
    uint8_t Y = (c == 0) ? Y0 : Y1;
    int C = Y - 16, D = U - 128, E = V - 128;
    int R = (298 * C + 409 * E + 128) >> 8;
    int G = (298 * C - 100 * D - 208 * E + 128) >> 8;
    int B = (298 * C + 516 * D + 128) >> 8;
    R = R < 0 ? 0 : (R > 255 ? 255 : R);
    G = G < 0 ? 0 : (G > 255 ? 255 : G);
    B = B < 0 ? 0 : (B > 255 ? 255 : B);
    int out_idx = (y * width + x) * 3;
    out_rgb[out_idx + 0] = R;
    out_rgb[out_idx + 1] = G;
    out_rgb[out_idx + 2] = B;

}

__global__ void compute_histogram(const uint16_t* depth, int* hist, int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;
    uint16_t d = depth[idx];
    if (d > 0 && d < 65536) atomicAdd(&hist[d], 1);
}

__global__ void prefix_sum_histogram(int* hist, int size) {
    for (int i = 1; i < size; ++i) {
        hist[i] += hist[i - 1];
    }
}

__device__ inline float3 interpolate_colormap(float value, const float3* colormap, int colormap_size) {
    float t = fminf(fmaxf(value, 0.f), 1.f) * (colormap_size - 1);
    int idx = (int)t;
    float frac = t - idx;
    float3 lo = colormap[idx];
    float3 hi = colormap[min(idx + 1, colormap_size - 1)];
    return make_float3(lo.x * (1.f - frac) + hi.x * frac,
                       lo.y * (1.f - frac) + hi.y * frac,
                       lo.z * (1.f - frac) + hi.z * frac);
}

__global__ void colorizeDepth(uint8_t* out_rgb,
                              const void* depth_data,
                              bool is_aligned,
                              const int* hist,
                              int width,
                              int height,
                              float depth_units,
                              float min_m,
                              float max_m,
                              bool equalize,
                              const float3* colormap,
                              int colormap_size) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;

    int idx = y * width + x;
    
    // Read depth value based on format
    uint16_t d;
    if (is_aligned) {
        uint32_t d32 = ((const uint32_t*)depth_data)[idx];
        d = (d32 == 0xFFFFFFFF) ? 0 : static_cast<uint16_t>(d32);
    } else {
        d = ((const uint16_t*)depth_data)[idx];
    }

    // Handle zero depth
    if (d == 0) {
        out_rgb[3 * idx + 0] = 0;
        out_rgb[3 * idx + 1] = 0;
        out_rgb[3 * idx + 2] = 0;
        return;
    }

    // Compute normalized value
    float norm;
    if (equalize && hist != nullptr && !is_aligned) {
        int total_hist = hist[65535];
        norm = (total_hist > 0) ? (float)(hist[d]) / total_hist : 0.f;
    } else {
        float depth_m = d * depth_units;
        norm = (depth_m - min_m) / (max_m - min_m);
        norm = fminf(fmaxf(norm, 0.f), 1.f);
    }

    // Apply colormap
    float3 c = interpolate_colormap(norm, colormap, colormap_size);
    out_rgb[3 * idx + 0] = (uint8_t)(c.x);
    out_rgb[3 * idx + 1] = (uint8_t)(c.y);
    out_rgb[3 * idx + 2] = (uint8_t)(c.z);
}

__global__ void projectDepthToRGB(uint32_t* aligned_depth,
                                  const uint16_t* raw_depth,
                                  const rs2_intrinsics* depth_intr,
                                  const rs2_intrinsics* rgb_intr,
                                  const rs2_extrinsics* depth_to_rgb,
                                  int width,
                                  int height,
                                  float depth_scale) {
    int dx = blockIdx.x * blockDim.x + threadIdx.x;
    int dy = blockIdx.y * blockDim.y + threadIdx.y;
    if (dx >= width || dy >= height) return;

    int depth_idx = dy * width + dx;
    uint16_t d = raw_depth[depth_idx];
    if (d == 0) return;

    float z = d * depth_scale;
    float x = (dx - depth_intr->ppx) / depth_intr->fx * z;
    float y = (dy - depth_intr->ppy) / depth_intr->fy * z;

    float rx = depth_to_rgb->rotation[0] * x + depth_to_rgb->rotation[3] * y + depth_to_rgb->rotation[6] * z + depth_to_rgb->translation[0];
    float ry = depth_to_rgb->rotation[1] * x + depth_to_rgb->rotation[4] * y + depth_to_rgb->rotation[7] * z + depth_to_rgb->translation[1];
    float rz = depth_to_rgb->rotation[2] * x + depth_to_rgb->rotation[5] * y + depth_to_rgb->rotation[8] * z + depth_to_rgb->translation[2];

    if (rz <= 0.f) return;

    int rx_pixel = static_cast<int>((rx / rz) * rgb_intr->fx + rgb_intr->ppx);
    int ry_pixel = static_cast<int>((ry / rz) * rgb_intr->fy + rgb_intr->ppy);

    if (rx_pixel < 0 || rx_pixel >= rgb_intr->width || ry_pixel < 0 || ry_pixel >= rgb_intr->height) return;

    int rgb_idx = ry_pixel * rgb_intr->width + rx_pixel;

    atomicMin(&aligned_depth[rgb_idx], static_cast<uint32_t>(d));
}

}
)";
} // namespace

namespace hololink::operators {

// hololink/common/cuda_helper.hpp's CudaCheck is for the driver API: it assigns into a CUresult,
// and cudaError_t will not convert to one. The runtime-API calls below need their own check -- an
// unchecked cudaMalloc hands the kernels a null pointer and fails somewhere else entirely.
#define CudaRtCheck(FUNC, WHAT)                                                       \
    {                                                                                 \
        const cudaError_t rt_result = FUNC;                                           \
        if (rt_result != cudaSuccess) {                                               \
            throw std::runtime_error(fmt::format("[{}:{}] {} failed: {}",             \
                __FILE__, __LINE__, WHAT, cudaGetErrorString(rt_result)));            \
        }                                                                             \
    }

void ImageDecoder::setup(holoscan::OperatorSpec& spec) {
    spec.input<holoscan::gxf::Entity>("input");
    spec.output<holoscan::gxf::Entity>("output");
    spec.param(allocator_, "allocator", "Allocator", "Memory allocator");
    spec.param(cuda_device_ordinal_, "cuda_device_ordinal", "CudaDeviceOrdinal", "CUDA device");
    spec.param(out_tensor_name_, "out_tensor_name", "OutputTensorName", "Name of output tensor");
    spec.param(align_depth_to_rgb, "align_depth_to_rgb", "AlignDepthToRGB", "Align Depth to RGB");
    spec.param(equalize_depth, "equalize_depth", "EqualizeDepth",
        "Histogram-equalize the colorized depth (unaligned path only)", false);
    cuda_stream_handler_.define_params(spec);
}

void ImageDecoder::start() {
    if (pixel_format_ == hololink::csi::PixelFormat::INVALID) throw std::runtime_error("Decoder not configured");
    CudaCheck(cuInit(0));
    CudaCheck(cuDeviceGet(&cuda_device_, cuda_device_ordinal_.get()));
    CudaCheck(cuDevicePrimaryCtxRetain(&cuda_context_, cuda_device_));
    hololink::common::CudaContextScopedPush cur_cuda_context(cuda_context_);
    cuda_function_launcher_.reset(new hololink::common::CudaFunctionLauncher(
        source, {"frameReconstructionZ16", "frameReconstructionYUYV","colorizeDepth",
             "compute_histogram", "prefix_sum_histogram", "projectDepthToRGB"}));
    
    // Allocate d_hist_ (256KB). Checked: a failed alloc otherwise reaches cudaMemsetAsync and
    // compute_histogram as a null pointer.
    CudaRtCheck(cudaMalloc(&d_hist_, sizeof(int) * 0x10000), "cudaMalloc d_hist_");

    // Allocate and upload colormap
    std::vector<float3> colormap = {
        {0.f, 0.f, 255.f},    // Blue
        {0.f, 255.f, 255.f},  // Cyan
        {255.f, 255.f, 0.f},  // Yellow
        {255.f, 0.f, 0.f},    // Red
        {50.f, 0.f, 0.f}      // Dark red
    };
    colormap_size_ = colormap.size();
    CudaRtCheck(cudaMalloc(&d_colormap_, colormap_size_ * sizeof(float3)), "cudaMalloc d_colormap_");
    CudaRtCheck(cudaMemcpy(d_colormap_, colormap.data(), colormap_size_ * sizeof(float3),
                           cudaMemcpyHostToDevice), "cudaMemcpy of the colormap");
    
    // Initialize alignment resources if alignment is enabled
    if (align_depth_to_rgb.get()) {
        initialize_alignment_resources();
    }
}

void ImageDecoder::set_depth_intrinsics(const rs2_intrinsics& intrinsics) {
    h_depth_intrin_ = intrinsics;
    alignment_calibration_dirty_ = true;
}

void ImageDecoder::set_rgb_intrinsics(const rs2_intrinsics& intrinsics) {
    h_rgb_intrin_ = intrinsics;
    alignment_calibration_dirty_ = true;
}

void ImageDecoder::set_extrinsics(const rs2_extrinsics& extrinsics) {
    h_extrinsics_ = extrinsics;
    alignment_calibration_dirty_ = true;
}

void ImageDecoder::set_depth_intrinsics(int width, int height, float ppx, float ppy, float fx, float fy) {
    h_depth_intrin_.width = width;
    h_depth_intrin_.height = height;
    h_depth_intrin_.ppx = ppx;
    h_depth_intrin_.ppy = ppy;
    h_depth_intrin_.fx = fx;
    h_depth_intrin_.fy = fy;
    alignment_calibration_dirty_ = true;
}

void ImageDecoder::set_rgb_intrinsics(int width, int height, float ppx, float ppy, float fx, float fy) {
    h_rgb_intrin_.width = width;
    h_rgb_intrin_.height = height;
    h_rgb_intrin_.ppx = ppx;
    h_rgb_intrin_.ppy = ppy;
    h_rgb_intrin_.fx = fx;
    h_rgb_intrin_.fy = fy;
    alignment_calibration_dirty_ = true;
}

void ImageDecoder::set_extrinsics(const float rotation[9], const float translation[3]) {
    memcpy(h_extrinsics_.rotation, rotation, 9 * sizeof(float));
    memcpy(h_extrinsics_.translation, translation, 3 * sizeof(float));
    alignment_calibration_dirty_ = true;
}

void ImageDecoder::initialize_alignment_resources() {
    // Allocate the device copies. The upload itself is left to
    // upload_alignment_calibration_if_dirty(), which also runs on every later change.
    CudaRtCheck(cudaMalloc(&d_depth_intrinsics_, sizeof(rs2_intrinsics)),
                "cudaMalloc d_depth_intrinsics_");
    CudaRtCheck(cudaMalloc(&d_rgb_intrinsics_, sizeof(rs2_intrinsics)),
                "cudaMalloc d_rgb_intrinsics_");
    CudaRtCheck(cudaMalloc(&d_depth_to_rgb_extrinsics_, sizeof(rs2_extrinsics)),
                "cudaMalloc d_depth_to_rgb_extrinsics_");

    upload_alignment_calibration_if_dirty();
}

// The setters only write the host copies, so the device copies have to be refreshed before the
// kernels read them. Without this, an app that calls set_rgb_intrinsics() after start() but before
// the first compute() passes ensure_aligned_depth_buffer()'s check -- which reads the host copy --
// and then reprojects against zeroed device intrinsics: fx is 0 and rgb width is 0, so the bounds
// test rejects every pixel and the aligned output is silently all black.
void ImageDecoder::upload_alignment_calibration_if_dirty() {
    if (!alignment_calibration_dirty_) {
        return;
    }
    CudaRtCheck(cudaMemcpy(d_depth_intrinsics_, &h_depth_intrin_, sizeof(rs2_intrinsics),
                           cudaMemcpyHostToDevice), "cudaMemcpy of the depth intrinsics");
    CudaRtCheck(cudaMemcpy(d_rgb_intrinsics_, &h_rgb_intrin_, sizeof(rs2_intrinsics),
                           cudaMemcpyHostToDevice), "cudaMemcpy of the RGB intrinsics");
    CudaRtCheck(cudaMemcpy(d_depth_to_rgb_extrinsics_, &h_extrinsics_, sizeof(rs2_extrinsics),
                           cudaMemcpyHostToDevice), "cudaMemcpy of the depth-to-RGB extrinsics");
    alignment_calibration_dirty_ = false;
}

// d_aligned_depth_ holds depth reprojected into the RGB frame. Which dims size it is subtler than it
// looks, and an earlier round of review got this wrong in BOTH directions:
//
//   - projectDepthToRGB() is LAUNCHED over the depth dims and READS raw_depth[dy * width + dx] in
//     depth coordinates -- but it WRITES aligned_depth[ry_pixel * rgb_intr->width + rx_pixel], i.e.
//     in RGB coordinates, bounded by rgb_intr->width/height. So the allocation must come from the
//     RGB intrinsics. Sizing it from the depth dims (as the previous round did, reasoning from the
//     launch dims) writes past the allocation whenever the RGB frame is the larger of the two.
//   - colorizeDepth() then READS this buffer at y * width_ + x, with the DEPTH width, and writes a
//     depth-dimensioned output tensor. So the two kernels only agree when the RGB and depth frames
//     have the same dimensions.
//
// Until the aligned path treats the RGB frame as the target throughout (issue #5), require the two
// to match and say so loudly, rather than silently reading the wrong grid. The validated D555
// configuration runs both at 1280x720, so it is unaffected.
void ImageDecoder::ensure_aligned_depth_buffer() {
    if (h_rgb_intrin_.width <= 0 || h_rgb_intrin_.height <= 0) {
        throw std::runtime_error(
            "align_depth_to_rgb is enabled but the RGB intrinsics are unset; "
            "call set_rgb_intrinsics() before starting the pipeline");
    }
    if (width_ == 0 || height_ == 0) {
        throw std::runtime_error("align_depth_to_rgb is enabled but configure() has not run");
    }
    if (h_rgb_intrin_.width != static_cast<int>(width_)
        || h_rgb_intrin_.height != static_cast<int>(height_)) {
        throw std::runtime_error(fmt::format(
            "align_depth_to_rgb currently requires matching depth and RGB dimensions (issue #5): "
            "depth is {}x{}, the RGB intrinsics say {}x{}",
            width_, height_, h_rgb_intrin_.width, h_rgb_intrin_.height));
    }
    // Sized from the RGB intrinsics: those are the coordinates projectDepthToRGB() writes.
    const size_t elems = static_cast<size_t>(h_rgb_intrin_.width) * static_cast<size_t>(h_rgb_intrin_.height);
    if (d_aligned_depth_ != nullptr && aligned_depth_elems_ == elems) {
        return;
    }
    if (d_aligned_depth_ != nullptr) {
        cudaFree(d_aligned_depth_);
        d_aligned_depth_ = nullptr;
    }
    cudaError_t err = cudaMalloc(&d_aligned_depth_, sizeof(uint32_t) * elems);
    if (err != cudaSuccess || d_aligned_depth_ == nullptr) {
        throw std::runtime_error(
            fmt::format("cudaMalloc for the aligned-depth buffer failed: {}", cudaGetErrorString(err)));
    }
    aligned_depth_elems_ = elems;
}

void ImageDecoder::stop() {
    hololink::common::CudaContextScopedPush cur_cuda_context(cuda_context_);
    cuda_function_launcher_.reset();

    if (d_hist_) {
        cudaFree(d_hist_);
        d_hist_ = nullptr;
    }

    if (d_colormap_) {
        cudaFree(d_colormap_);
        d_colormap_ = nullptr;
        colormap_size_ = 0;
    }
    if (d_depth_intrinsics_) {
        cudaFree(d_depth_intrinsics_);
        d_depth_intrinsics_ = nullptr;
    }
    if (d_rgb_intrinsics_) {
        cudaFree(d_rgb_intrinsics_);
        d_rgb_intrinsics_ = nullptr;
    }
    if (d_depth_to_rgb_extrinsics_) {
        cudaFree(d_depth_to_rgb_extrinsics_);
        d_depth_to_rgb_extrinsics_ = nullptr;
    }
    if (d_aligned_depth_) {
        cudaFree(d_aligned_depth_);
        d_aligned_depth_ = nullptr;
        aligned_depth_elems_ = 0;
    }

    CudaCheck(cuDevicePrimaryCtxRelease(cuda_device_));
    cuda_context_ = nullptr;
}

void ImageDecoder::compute(holoscan::InputContext& input, holoscan::OutputContext& output,
                           holoscan::ExecutionContext& context) {
    auto maybe_entity = input.receive<holoscan::gxf::Entity>("input");
    if (!maybe_entity) throw std::runtime_error("No input entity");
    auto& entity = static_cast<nvidia::gxf::Entity&>(maybe_entity.value());
    gxf_result_t stream_handler_result = cuda_stream_handler_.from_message(context.context(), entity);
    if (stream_handler_result != GXF_SUCCESS) throw std::runtime_error("Failed to get stream");
    auto input_tensor = entity.get<nvidia::gxf::Tensor>().value();

    if (input_tensor->storage_type() == nvidia::gxf::MemoryStorageType::kHost) {
        if (!is_integrated_ && !host_memory_warning_) {
            host_memory_warning_ = true;
            HSB_LOG_WARN(
                "The input tensor is stored in host memory, this will reduce performance of this "
                "operator. For best performance store the input tensor in device memory.");
        }
    } else if (input_tensor->storage_type() != nvidia::gxf::MemoryStorageType::kDevice) {
        throw std::runtime_error(
            fmt::format("Unsupported storage type {}", (int)input_tensor->storage_type()));
    }

    if (input_tensor->rank() != 1) throw std::runtime_error("Tensor must be 1D");

    const int32_t size = input_tensor->shape().dimension(0);
    // The kernels below read frame_start_size_ + per_line_size * height_ bytes out of this tensor,
    // which is network-derived. Check it actually holds that much before we index into it.
    const uint32_t per_line_size_check = line_start_size_ + bytes_per_line_ + line_end_size_;
    const size_t required = static_cast<size_t>(frame_start_size_)
                          + static_cast<size_t>(per_line_size_check) * static_cast<size_t>(height_);
    if (size < 0 || static_cast<size_t>(size) < required) {
        throw std::runtime_error(fmt::format(
            "CSI tensor too small: have {} bytes, need {} ({}x{}, frame_start={}, per_line={})",
            size, required, width_, height_, frame_start_size_, per_line_size_check));
    }
    auto allocator = nvidia::gxf::Handle<nvidia::gxf::Allocator>::Create(
        fragment()->executor().context(), allocator_->gxf_cid());
    const uint32_t per_line_size = line_start_size_ + bytes_per_line_ + line_end_size_;

    switch (pixel_format_) {
    case hololink::csi::PixelFormat::RAW_16: {
        // llocate depth tensor (device)
        nvidia::gxf::Shape depth_shape{int(height_), int(width_), 1};
        auto depth_message = CreateTensorMap(context.context(), allocator.value(), {{
            "depth", nvidia::gxf::MemoryStorageType::kDevice, depth_shape,
            nvidia::gxf::PrimitiveType::kUnsigned16, 0,
            nvidia::gxf::ComputeTrivialStrides(depth_shape, 2)}}, false);
        auto depth_tensor = depth_message.value().get<nvidia::gxf::Tensor>("depth");

        // Reconstruct depth frame from CSI
        cuda_function_launcher_->launch("frameReconstructionZ16", {width_, height_, 1},
            cuda_stream_handler_.get_cuda_stream(context.context()),
            depth_tensor.value()->pointer(),
            input_tensor->pointer() + frame_start_size_ + line_start_size_,
            per_line_size, width_, height_);

        // Allocate RGB tensor (device)
        nvidia::gxf::Shape rgb_shape{int(height_), int(width_), 3};
        auto out_message = align_depth_to_rgb.get()
            ? CreateTensorMap(context.context(), allocator.value(), {
                {
                    out_tensor_name_.get(), nvidia::gxf::MemoryStorageType::kDevice, rgb_shape,
                    nvidia::gxf::PrimitiveType::kUnsigned8, 0,
                    nvidia::gxf::ComputeTrivialStrides(rgb_shape, 1)
                },
                {
                    "depth_raw", nvidia::gxf::MemoryStorageType::kDevice, depth_shape,
                    nvidia::gxf::PrimitiveType::kUnsigned16, 0,
                    nvidia::gxf::ComputeTrivialStrides(depth_shape,
                    nvidia::gxf::PrimitiveTypeSize(nvidia::gxf::PrimitiveType::kUnsigned16))
                },
            }, false)
            : CreateTensorMap(context.context(), allocator.value(), {
                {
                    out_tensor_name_.get(), nvidia::gxf::MemoryStorageType::kDevice, rgb_shape,
                    nvidia::gxf::PrimitiveType::kUnsigned8, 0,
                    nvidia::gxf::ComputeTrivialStrides(rgb_shape, 1)
                },
            }, false);
        auto rgb_tensor = out_message.value().get<nvidia::gxf::Tensor>(out_tensor_name_.get().c_str());

        // fill depth_raw sensor
        if (align_depth_to_rgb.get()) {
            auto raw_tensor = out_message.value().get<nvidia::gxf::Tensor>("depth_raw");
            cudaMemcpyAsync(raw_tensor.value()->pointer(), depth_tensor.value()->pointer(),
                            sizeof(uint16_t) * width_ * height_,
                            cudaMemcpyDeviceToDevice,
                            cuda_stream_handler_.get_cuda_stream(context.context()));
        }

        // GPU histogram
        const bool do_equalize = equalize_depth.get() && !align_depth_to_rgb.get();
        if (do_equalize) {
        cudaMemsetAsync(d_hist_, 0, sizeof(int) * 0x10000, cuda_stream_handler_.get_cuda_stream(context.context()));

        cuda_function_launcher_->launch("compute_histogram", {width_, height_, 1},
            cuda_stream_handler_.get_cuda_stream(context.context()),
            depth_tensor.value()->pointer(), d_hist_, width_ * height_);

        cuda_function_launcher_->launch("prefix_sum_histogram", {1, 1, 1},
            cuda_stream_handler_.get_cuda_stream(context.context()),
            d_hist_,
            0x10000);
        }

        // Run GPU depth → RGB colorizer
        if (align_depth_to_rgb.get()) {
            ensure_aligned_depth_buffer();
            upload_alignment_calibration_if_dirty();
            cudaMemsetAsync(d_aligned_depth_, 0xFF, sizeof(uint32_t) * aligned_depth_elems_,
                cuda_stream_handler_.get_cuda_stream(context.context()));

            cuda_function_launcher_->launch("projectDepthToRGB", {width_, height_, 1},
                cuda_stream_handler_.get_cuda_stream(context.context()),
                d_aligned_depth_,
                depth_tensor.value()->pointer(),
                d_depth_intrinsics_,
                d_rgb_intrinsics_,
                d_depth_to_rgb_extrinsics_,
                width_, height_, 0.001f);

            cuda_function_launcher_->launch("colorizeDepth", {width_, height_, 1},
                cuda_stream_handler_.get_cuda_stream(context.context()),
                rgb_tensor.value()->pointer(),
                d_aligned_depth_,
                true,  // is_aligned
                (int*)nullptr,  // hist (not used for aligned)
                width_, height_,
                0.001f, 0.3f, 4.0f,
                false,  // equalize (not used for aligned)
                d_colormap_, colormap_size_);
        } else {
            cuda_function_launcher_->launch("colorizeDepth", {width_, height_, 1},
                cuda_stream_handler_.get_cuda_stream(context.context()),
                rgb_tensor.value()->pointer(),
                depth_tensor.value()->pointer(),
                false,  // is_aligned
                d_hist_,
                width_, height_,
                0.001f,
                0.3f, 4.0f,
                do_equalize,
                d_colormap_,
                static_cast<int>(colormap_size_));
        }


        // Emit output
        stream_handler_result = cuda_stream_handler_.to_message(out_message);
        if (stream_handler_result != GXF_SUCCESS) {
            throw std::runtime_error("Failed to emit RGB image");
        }
        // auto& out_entity = out_message.value();
        auto out_entity = holoscan::gxf::Entity(std::move(out_message.value()));
        output.emit(out_entity);
        return;
    }
    case hololink::csi::PixelFormat::YUYV_422_8: {
        nvidia::gxf::Shape shape{int(height_), int(width_), 3};
        auto out_message = CreateTensorMap(context.context(), allocator.value(), {{
            out_tensor_name_.get(), nvidia::gxf::MemoryStorageType::kDevice, shape,
            nvidia::gxf::PrimitiveType::kUnsigned8, 0,
            nvidia::gxf::ComputeTrivialStrides(shape, 1)}}, false);
        auto tensor = out_message.value().get<nvidia::gxf::Tensor>(out_tensor_name_.get().c_str());
        cuda_function_launcher_->launch("frameReconstructionYUYV", {width_, height_, 1},
            cuda_stream_handler_.get_cuda_stream(context.context()),
            tensor.value()->pointer(),
            input_tensor->pointer() + frame_start_size_ + line_start_size_,
            per_line_size, width_, height_);
        stream_handler_result = cuda_stream_handler_.to_message(out_message);
        auto out_entity = holoscan::gxf::Entity(std::move(out_message.value()));
        output.emit(out_entity);
        return;
    }
    default:
        throw std::runtime_error("Unsupported pixel format");
    }
}

void ImageDecoder::configure(uint32_t width, uint32_t height, hololink::csi::PixelFormat pixel_format,
                             uint32_t frame_start_size, uint32_t frame_end_size,
                             uint32_t line_start_size, uint32_t line_end_size,
                             uint32_t margin_left, uint32_t margin_top,
                             uint32_t margin_right, uint32_t margin_bottom) {
    width_ = width;
    height_ = height;
    pixel_format_ = pixel_format;
    frame_start_size_ = frame_start_size;
    frame_end_size_ = frame_end_size;
    line_start_size_ = line_start_size;
    line_end_size_ = line_end_size;
    switch (pixel_format_) {
    case hololink::csi::PixelFormat::RAW_16:
        bytes_per_line_ = width * 2;
        line_start_size_ += margin_left * 2;
        line_end_size_ += margin_right * 2;
        break;
    case hololink::csi::PixelFormat::YUYV_422_8:
        bytes_per_line_ = width * 2;
        line_start_size_ += margin_left * 2;
        line_end_size_ += margin_right * 2;
        break;
    default:
        throw std::runtime_error("Unsupported pixel format");
    }
    const uint32_t line_size = line_start_size_ + bytes_per_line_ + line_end_size_;
    frame_start_size_ += margin_top * line_size;
    frame_end_size_ += margin_bottom * line_size;
    csi_length_ = (frame_start_size_ + line_size * height_ + frame_end_size_ + 7) & ~7;
}

size_t ImageDecoder::get_csi_length() {
    if (pixel_format_ == hololink::csi::PixelFormat::INVALID) {
        throw std::runtime_error("ImageDecoder is not configured.");
    }
    return csi_length_;
}

} // namespace hololink::operators
