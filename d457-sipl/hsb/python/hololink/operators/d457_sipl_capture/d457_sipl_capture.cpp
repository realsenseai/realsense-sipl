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

#include <hololink/operators/d457_sipl_capture/d457_sipl_capture.hpp>

#include "../operator_util.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstdint>
#include <memory>
#include <string>

#include <holoscan/core/fragment.hpp>
#include <holoscan/core/operator.hpp>
#include <holoscan/core/operator_spec.hpp>

using std::string_literals::operator""s;
using pybind11::literals::operator""_a;

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;

namespace hololink::operators {

class PyD457SIPLCaptureOp : public D457SIPLCaptureOp {
public:
    using D457SIPLCaptureOp::D457SIPLCaptureOp;

    PyD457SIPLCaptureOp(holoscan::Fragment* fragment, const py::args& args,
        const std::string& camera_config,
        const std::string& json_config,
        const std::string& stream,
        uint32_t link_mask,
        uint32_t capture_queue_depth,
        uint32_t timeout,
        bool strict,
        const std::string& name = "d457_sipl_capture")
        : D457SIPLCaptureOp(camera_config, json_config, stream, link_mask,
            holoscan::ArgList {
                holoscan::Arg { "capture_queue_depth", capture_queue_depth },
                holoscan::Arg { "timeout", timeout },
                holoscan::Arg { "strict", strict } })
    {
        add_positional_condition_and_resource_args(this, args);
        name_ = name;
        fragment_ = fragment;
        spec_ = std::make_shared<holoscan::OperatorSpec>(fragment);
        setup(*spec_.get());
    }
};

PYBIND11_MODULE(_d457_sipl_capture, m)
{
#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif

    py::class_<D457SIPLCaptureOp::CameraInfo>(m, "CameraInfo")
        .def_readwrite("output_name", &D457SIPLCaptureOp::CameraInfo::output_name)
        .def_readwrite("stream", &D457SIPLCaptureOp::CameraInfo::stream)
        .def_readwrite("link", &D457SIPLCaptureOp::CameraInfo::link)
        .def_readwrite("offset", &D457SIPLCaptureOp::CameraInfo::offset)
        .def_readwrite("width", &D457SIPLCaptureOp::CameraInfo::width)
        .def_readwrite("height", &D457SIPLCaptureOp::CameraInfo::height)
        .def_readwrite("bytes_per_line", &D457SIPLCaptureOp::CameraInfo::bytes_per_line)
        .def_readwrite("pixel_format", &D457SIPLCaptureOp::CameraInfo::pixel_format)
        .def_readwrite("bayer_format", &D457SIPLCaptureOp::CameraInfo::bayer_format);

    py::class_<D457SIPLCaptureOp, PyD457SIPLCaptureOp, holoscan::Operator,
        std::shared_ptr<D457SIPLCaptureOp>>(m, "D457SIPLCaptureOp")
        .def(py::init<holoscan::Fragment*, const py::args&,
                 const std::string&,
                 const std::string&,
                 const std::string&,
                 uint32_t,
                 uint32_t,
                 uint32_t,
                 bool,
                 const std::string&>(),
            "fragment"_a,
            "camera_config"_a = "",
            "json_config"_a = "",
            "stream"_a = "",
            "link_mask"_a = 0x0001u,
            "capture_queue_depth"_a = 4u,
            "timeout"_a = 1000000u,
            "strict"_a = false,
            "name"_a = "d457_sipl_capture"s)
        .def_static("list_available_configs", &D457SIPLCaptureOp::list_available_configs,
            "json_config"_a = ""s)
        .def("get_camera_info", &D457SIPLCaptureOp::get_camera_info);
} // PYBIND11_MODULE

} // namespace hololink::operators
