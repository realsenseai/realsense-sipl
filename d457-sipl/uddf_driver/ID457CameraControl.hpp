/*
 * Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
 *
 * ID457CameraControl.hpp — structured D457 camera-control interface (SIPL custom interface).
 *
 * This is the "use the SIPL interface, not raw register pokes" control surface for the D457.
 * A SIPL client retrieves it the standard way — after Init(), before Start():
 *
 *   INvSIPLCamera::GetModuleInterfaceProvider(sensorIdx, provider);   // nvsipl::IInterfaceProvider
 *   auto* ctrl = reinterpret_cast<ID457CameraControl*>(
 *                    provider->GetInterface(<ID457_CAMERA_CONTROL_ID>));
 *   ctrl->SetControl(CtrlId::DepthExposureUs, 8000);
 *
 * The framework forwards GetInterface() down to D457Module::doGetExtendedInterface() (the GMSL
 * ModuleUbb extension hook). nvsipl::UUID and uddf::ddi::UUID are layout-identical (16 bytes, same
 * fields), so the client builds an nvsipl::UUID with the same field values as `id` below.
 *
 * Model = the V4L2 control model d4xx exposes, re-expressed on SIPL:
 *   - QueryControl()  ≈ VIDIOC_QUERYCTRL  — discover the published control list (name/range/type).
 *   - GetControl()/SetControl() ≈ G_CTRL/S_CTRL — read/write a control by stable id, value-based.
 * The driver maps each control id to its DS5 host control register internally (block 0x4100 depth/IR,
 * 0x4200 RGB); the client never sees a register offset or a byte-swap. This header is shared verbatim
 * between the driver and the client so both agree on the id, the CtrlId enum, and ControlDesc.
 */
#ifndef UDDF_CDD_ID457_CAMERA_CONTROL_HPP
#define UDDF_CDD_ID457_CAMERA_CONTROL_HPP

#include <cstdint>

#include "uddf/ddi/IInterface.hpp"
#include "uddf/ddi/uuid.hpp"

namespace uddf::cdd::d457 {

namespace uuid {
// Unique id for ID457CameraControl. The client passes the SAME field values as an nvsipl::UUID.
// AUTHORITATIVE SOURCE: this constant is duplicated as raw field values (nvsipl::UUID cannot be
// constructed from uddf::ddi::UUID across the SIPL/UDDF boundary) in
// sdk-patches/patch_nvsipl_camera_d457ctrl.py (two injected `kD457CtrlId` locals in main.cpp).
// If this value ever changes, update both copies there too.
inline constexpr uddf::ddi::UUID ID457_CAMERA_CONTROL_ID =
    uddf::ddi::UUID(0xd457c711U, 0x9a2eU, 0x4f6bU, 0xb3d1U, 0x52U, 0x6fU, 0xa4U, 0x18U, 0x0cU, 0x37U);
} // namespace uuid

/** Which DS5 control block a control targets (depth + left-IR share cam0; RGB is cam1). */
enum class Cam : uint8_t { DepthIR = 0, Color = 1 };

/** Control value semantics (for clients rendering/validating the published list). */
enum class CtrlType : uint8_t { Integer = 0, Boolean = 1, Menu = 2 };

/** Result of a control operation. */
enum class CtrlResult : int32_t {
    Ok = 0,
    UnknownId,      ///< no published control with that id/index
    NotSupported,   ///< control exists but is not writable/readable (or SKU-gated off)
    OutOfRange,     ///< value outside [min,max]
    NotReady,       ///< driver has no live HW handle yet (call after Init())
    HwError,        ///< the underlying DS5 register access failed
};

/**
 * Stable id for every published control. Values are part of the driver↔client contract — only
 * append; never renumber. Depth/IR controls hit DS5 cam0 (0x4100); RGB controls hit cam1 (0x4200).
 */
enum class CtrlId : uint32_t {
    // ── Depth + IR (DS5 cam0 control block, 0x4100) ──
    DepthExposureUs = 0,    ///< manual exposure, microseconds
    DepthGain,              ///< imager gain code
    DepthAutoExposure,      ///< 0/1; when on, the DS5 runs its own AE
    EmitterMode,            ///< projector mode: 0 off, 1 laser, 2 auto, 3 led
    LaserPowerMw,           ///< manual laser power, milliwatts (eyesafety-clamped at runtime)
    SyncMode,               ///< inter-cam sync: 0 default, 1 master, 2 external
    ReadoutShaping,         ///< % HTS readout shaping
    // ── RGB (DS5 cam1 control block, 0x4200) ──
    RgbExposureUs,          ///< manual exposure, microseconds (DS5 reg is 100 µs units)
    RgbGain,                ///< gain
    RgbAutoExposure,        ///< UVC AE mode: 1 manual, 2 auto, 4 shutter-pri, 8 aperture-pri
    RgbAePriority,          ///< 0/1
    RgbSaturation,          ///< 0..100
    RgbSharpness,           ///< 0..100
    RgbWhiteBalanceTempK,   ///< white-balance temperature, Kelvin
    RgbAutoWhiteBalance,    ///< 0/1
    RgbPowerLineFreq,       ///< 0 off, 1 50 Hz, 2 60 Hz
};

/**
 * Published descriptor for one control — the discoverable list (V4L2 QUERYCTRL analog).
 * `name` is a stable machine token (e.g. "depth_exposure_us") usable on a CLI. `menu`, when set,
 * is a pipe-separated label list for Menu-type controls (nullptr otherwise). No register/encoding
 * detail is exposed here — that stays inside the driver.
 */
struct ControlDesc {
    CtrlId      id;
    const char* name;
    Cam         cam;
    CtrlType    type;
    int64_t     min;
    int64_t     max;
    int64_t     step;
    int64_t     def;
    bool        readable;
    bool        writable;
    const char* menu;   ///< Menu labels "a|b|c", else nullptr
};

/**
 * @brief Structured D457 camera-control interface (retrieved via the SIPL interface provider).
 */
class ID457CameraControl : public uddf::ddi::IInterface {
public:
    static constexpr uddf::ddi::UUID id { uuid::ID457_CAMERA_CONTROL_ID };

    // ── Discovery — walk/look up the published control list ──
    virtual uint32_t   GetControlCount() const noexcept = 0;
    virtual CtrlResult QueryControl(uint32_t index, ControlDesc& out) const noexcept = 0;
    virtual CtrlResult QueryControlById(CtrlId cid, ControlDesc& out) const noexcept = 0;

    // ── Read / write a control by id (value-based; driver maps to DS5 register internally) ──
    // NB: GetControl reads the DS5 mux over I2C — do NOT call while the RGB stream runs (it stalls
    // the RealTek RGB ISP). Reads are safe for depth/IR-only sessions. Sets are fire-safe.
    virtual CtrlResult GetControl(CtrlId cid, int64_t& value) noexcept = 0;
    virtual CtrlResult SetControl(CtrlId cid, int64_t value) noexcept = 0;

protected:
    ~ID457CameraControl() override = default;
};

} // namespace uddf::cdd::d457

#endif // UDDF_CDD_ID457_CAMERA_CONTROL_HPP
