/*
 * Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
 *
 * D457Module.cpp — wires the D457 sensor + stock MAX9295 serializer into the ModuleUbb.
 * See D457Module.hpp. Modeled on coSerDes/modules/R0SIM623/R0SIM623Module.cpp.
 */
#include "D457Module.hpp"
#include "D457Sensor.hpp"

// Stock serializer driver: uddf/drivers/serializers/MAX9295/ (target uddf-cdd::ser_max9295_driver).
#include "serializers/MAX9295/MAX9295.hpp"

#include <cstdlib>   // getenv, strtoll
#include <cstring>   // strcmp
#include <cstdio>    // fprintf (env-apply harness logging)
#include <sstream>
#include <string>
#include <thread>    // settle delay between env set and read-back
#include <chrono>

namespace uddf::cdd::d457 {

bool D457Module::doCreateUbbObjects(const GmslModuleContext::Config& config)
{
    const size_t numSensors = config.sensorInfoList.empty() ? 1U : config.sensorInfoList.size();

    // Serializer: stock MAX9295, reused unchanged.
    // NB: numSensors here means GMSL *links* to the serializer, NOT capture streams. The D457 is a
    // single DS5 ASIC on ONE MAX9295A link, even when it emits several streams on distinct CSI virtual
    // channels (depth VC0 + RGB VC1). So the serializer always uses the D457 single-link init path
    // (patch_max9295_d457.py: the numSensors==1 branch). Passing the sensor-UBB count (==2 for
    // depth+RGB) would wrongly select the HAWK 2-link path. The per-VC pipe routing is configured by
    // that single-link sequence, not by this count.
    auto ser = std::make_unique<uddf::cdd::max9295::MAX9295>(config);
    ser->SetConfig(uddf::cdd::max9295::MAX9295Config{ .enableEmbDataType = false,
                                                      .numSensors = 1U });
    if (!addSerializerUbb(std::move(ser))) {
        return false;
    }

    // Sensor(s): one D457 (DS5) UBB per configured sensorInfo entry == one capture pipeline.
    // Single sensor = depth (or D457_STREAM-selected); two sensors = depth (idx 0) + RGB (idx 1).
    for (uint8_t i = 0; i < static_cast<uint8_t>(numSensors); ++i) {
        auto sensor = std::make_unique<D457Sensor>(config, i);
        if (i == 0U) {
            m_sensor0 = sensor.get();   // non-owning; backs WriteI2C/ReadI2C (DS5 mux owner)
        }
        if (!addSensorUbb(std::move(sensor))) {
            return false;
        }
    }

    // EEPROM: optional; not required for first-light. Add a stock eeprom UBB later if needed.
    return true;
}

bool D457Module::SetDeviceOffsetWidth(uint16_t address, uint8_t offsetWidth)
{
    // v1 targets only the DS5 mux with 16-bit (2-byte) register offsets — the offset width
    // GetDeviceTable already fixes for that mapping. WriteReg16/ReadReg16 always use 2-byte offsets.
    return (address == D457_MUX_I2C_ADDR) && (offsetWidth == 2U);
}

uddf::ddi::IInterface* D457Module::doGetExtendedInterface(const uddf::ddi::UUID& uuid)
{
    // Called by ModuleUbb::GetInterface() for any UUID it doesn't handle itself. Return our extra
    // interfaces; nullptr for anything else (the base already tried the standard module interfaces).
    if (uuid == uddf::ddi::interfaces::IReadWriteI2C::id) {
        return static_cast<uddf::ddi::interfaces::IReadWriteI2C*>(this);
    }
    if (uuid == ID457CameraControl::id) {
        return static_cast<ID457CameraControl*>(this);
    }
    return nullptr;
}

// ── ID457CameraControl: the published control registry ──────────────────────────────────────────
// One row per control. `d` is the public descriptor (what QueryControl hands the client); reg/bits/
// scale are the private DS5 mapping. value↔register: regVal = value / scale ; value = regVal * scale
// (scale=100 converts the RGB exposure µs API to the DS5's 100 µs register unit; 1 elsewhere).
// Registers: depth/IR cam0 block 0x4100, RGB cam1 block 0x4200 (see camera-controls-design.md §4).
namespace {
struct CtrlEntry {
    ControlDesc d;
    uint16_t    reg;    // DS5 control register (the LSW address for 32-bit controls)
    uint8_t     bits;   // 16 or 32
    int32_t     scale;  // value/register unit conversion (1, or 100 for RGB exposure)
};

// clang-format off
const CtrlEntry kRegistry[] = {
    // id                          name                    cam            type               min     max   step    def   rd   wr  menu                                 reg     bits scale
    { { CtrlId::DepthExposureUs,   "depth_exposure_us",    Cam::DepthIR,  CtrlType::Integer,    1, 165000,    1, 33000, true, true, nullptr },                          0x4100U, 32,  1 },
    { { CtrlId::DepthGain,         "depth_gain",           Cam::DepthIR,  CtrlType::Integer,   16,    248,    1,    16, true, true, nullptr },                          0x4104U, 16,  1 },
    { { CtrlId::DepthAutoExposure, "depth_auto_exposure",  Cam::DepthIR,  CtrlType::Boolean,    0,      1,    1,     1, true, true, nullptr },                          0x410CU, 16,  1 },
    { { CtrlId::EmitterMode,       "emitter_mode",         Cam::DepthIR,  CtrlType::Menu,       0,      3,    1,     1, true, true, "off|laser|auto|led" },             0x4108U, 16,  1 },
    { { CtrlId::LaserPowerMw,      "laser_power_mw",       Cam::DepthIR,  CtrlType::Integer,    0,    360,   30,   150, true, true, nullptr },                          0x4124U, 16,  1 },
    { { CtrlId::SyncMode,          "sync_mode",            Cam::DepthIR,  CtrlType::Menu,       0,      2,    1,     0, true, true, "default|master|external" },        0x412CU, 16,  1 },
    { { CtrlId::ReadoutShaping,    "readout_shaping",      Cam::DepthIR,  CtrlType::Integer,    0,    100,    1,     0, true, true, nullptr },                          0x4130U, 16,  1 },

    { { CtrlId::RgbExposureUs,     "rgb_exposure_us",      Cam::Color,    CtrlType::Integer,  100,1000000,  100,166000, true, true, nullptr },                          0x4200U, 32,100 },
    { { CtrlId::RgbGain,           "rgb_gain",             Cam::Color,    CtrlType::Integer,    0,    128,    1,    64, true, true, nullptr },                          0x4204U, 16,  1 },
    { { CtrlId::RgbAutoExposure,   "rgb_auto_exposure",    Cam::Color,    CtrlType::Menu,       1,      8,    1,     8, true, true, "1=manual|2=auto|4=shutter|8=aperture" }, 0x420CU, 16, 1 },
    { { CtrlId::RgbAePriority,     "rgb_ae_priority",      Cam::Color,    CtrlType::Boolean,    0,      1,    1,     0, true, true, nullptr },                          0x4208U, 16,  1 },
    { { CtrlId::RgbSaturation,     "rgb_saturation",       Cam::Color,    CtrlType::Integer,    0,    100,    1,    64, true, true, nullptr },                          0x4210U, 16,  1 },
    { { CtrlId::RgbSharpness,      "rgb_sharpness",        Cam::Color,    CtrlType::Integer,    0,    100,    1,    50, true, true, nullptr },                          0x4214U, 16,  1 },
    { { CtrlId::RgbWhiteBalanceTempK,"rgb_wb_temp_k",      Cam::Color,    CtrlType::Integer, 2800,   6500,   10,  4600, true, true, nullptr },                          0x4218U, 16,  1 },
    { { CtrlId::RgbAutoWhiteBalance,"rgb_auto_wb",         Cam::Color,    CtrlType::Boolean,    0,      1,    1,     1, true, true, nullptr },                          0x421CU, 16,  1 },
    { { CtrlId::RgbPowerLineFreq,  "rgb_power_line_freq",  Cam::Color,    CtrlType::Menu,       0,      2,    1,     0, true, true, "off|50hz|60hz" },                  0x4220U, 16,  1 },
};
// clang-format on
constexpr uint32_t kRegistryCount = static_cast<uint32_t>(sizeof(kRegistry) / sizeof(kRegistry[0]));

const CtrlEntry* FindEntry(CtrlId cid)
{
    for (uint32_t i = 0; i < kRegistryCount; ++i) {
        if (kRegistry[i].d.id == cid) { return &kRegistry[i]; }
    }
    return nullptr;
}

const CtrlEntry* FindByName(const char* name)
{
    for (uint32_t i = 0; i < kRegistryCount; ++i) {
        if (std::strcmp(kRegistry[i].d.name, name) == 0) { return &kRegistry[i]; }
    }
    return nullptr;
}

// Encode a control value and write it to the DS5 via the given sensor's register path. 32-bit
// controls stage the MSW (+0x02) then commit the LSW (base). Shared by SetControl and the env harness.
bool WriteCtrl(D457Sensor& s, const CtrlEntry& e, int64_t value)
{
    const int32_t  scale  = (e.scale != 0) ? e.scale : 1;
    const uint32_t regVal = static_cast<uint32_t>(value / scale);
    if (e.bits == 32U) {
        if (!s.WriteReg16(static_cast<uint16_t>(e.reg + 2U),
                          static_cast<uint16_t>((regVal >> 16) & 0xFFFFU))) { return false; }
        return s.WriteReg16(e.reg, static_cast<uint16_t>(regVal & 0xFFFFU));
    }
    return s.WriteReg16(e.reg, static_cast<uint16_t>(regVal & 0xFFFFU));
}

// Read a control value back from the DS5 via the given sensor's register path.
bool ReadCtrl(D457Sensor& s, const CtrlEntry& e, int64_t& value)
{
    uint16_t lo = 0U;
    if (!s.ReadReg16(e.reg, lo)) { return false; }
    uint32_t regVal = lo;
    if (e.bits == 32U) {
        uint16_t hi = 0U;
        if (!s.ReadReg16(static_cast<uint16_t>(e.reg + 2U), hi)) { return false; }
        regVal |= (static_cast<uint32_t>(hi) << 16);
    }
    value = static_cast<int64_t>(regVal) * ((e.scale != 0) ? e.scale : 1);
    return true;
}
} // namespace

uint32_t D457Module::GetControlCount() const noexcept { return kRegistryCount; }

CtrlResult D457Module::QueryControl(uint32_t index, ControlDesc& out) const noexcept
{
    if (index >= kRegistryCount) { return CtrlResult::UnknownId; }
    out = kRegistry[index].d;
    return CtrlResult::Ok;
}

CtrlResult D457Module::QueryControlById(CtrlId cid, ControlDesc& out) const noexcept
{
    const CtrlEntry* e = FindEntry(cid);
    if (e == nullptr) { return CtrlResult::UnknownId; }
    out = e->d;
    return CtrlResult::Ok;
}

CtrlResult D457Module::SetControl(CtrlId cid, int64_t value) noexcept
{
    const CtrlEntry* e = FindEntry(cid);
    if (e == nullptr)        { return CtrlResult::UnknownId; }
    if (!e->d.writable)      { return CtrlResult::NotSupported; }
    if (value < e->d.min || value > e->d.max) { return CtrlResult::OutOfRange; }

    std::lock_guard<std::mutex> lk(m_ctrlMtx);
    if (m_sensor0 == nullptr || !m_sensor0->HasHwAccess()) { return CtrlResult::NotReady; }
    return WriteCtrl(*m_sensor0, *e, value) ? CtrlResult::Ok : CtrlResult::HwError;
}

CtrlResult D457Module::GetControl(CtrlId cid, int64_t& value) noexcept
{
    const CtrlEntry* e = FindEntry(cid);
    if (e == nullptr)   { return CtrlResult::UnknownId; }
    if (!e->d.readable) { return CtrlResult::NotSupported; }

    std::lock_guard<std::mutex> lk(m_ctrlMtx);
    if (m_sensor0 == nullptr || !m_sensor0->HasHwAccess()) { return CtrlResult::NotReady; }
    return ReadCtrl(*m_sensor0, *e, value) ? CtrlResult::Ok : CtrlResult::HwError;
}

// ── Env-driven control harness ───────────────────────────────────────────────────────────────────
// ApplyEnvControls() is invoked by sensor 0 from D457Sensor::StartStreaming() (after the stream is up
// and HW access is live). It is one of TWO ways controls reach the DS5 today; both share this file's
// control registry + encoding (WriteCtrl/ReadCtrl, same as SetControl/GetControl):
//   1. This env harness (D457_CTRL / D457_CTRL_GET) -- applied in-driver from StartStreaming.
//   2. The ID457CameraControl SIPL interface, driven from the nvsipl_camera main.cpp hook injected by
//      sdk-patches/patch_nvsipl_camera_d457ctrl.py after upMaster->Start() (used because the
//      client-side GetModuleInterfaceProvider path is rejected INVALID_STATE on the current SDK).
//   D457_CTRL="name=val;name=val"   -> apply each
//   D457_CTRL_GET="name,name"       -> read each back
void ApplyEnvControls(D457Sensor& sensor0)
{
    const char* setEnv = std::getenv("D457_CTRL");
    const char* getEnv = std::getenv("D457_CTRL_GET");
    if (setEnv == nullptr && getEnv == nullptr) { return; }

    if (setEnv != nullptr) {
        std::stringstream ss{std::string(setEnv)};
        std::string tok;
        while (std::getline(ss, tok, ';')) {
            const auto eq = tok.find('=');
            if (eq == std::string::npos) { continue; }
            const std::string nm = tok.substr(0, eq);
            const std::string vs = tok.substr(eq + 1);
            const CtrlEntry* e = FindByName(nm.c_str());
            if (e == nullptr) { std::fprintf(stderr, "[D457CTRL-DRV] set: unknown control '%s'\n", nm.c_str()); continue; }
            // Parse strictly: strtoll returns 0 on non-numeric input, and 0 can be a valid in-range
            // control value -- silently writing 0 to the DS5 would be a real bug. Reject if the string
            // isn't fully consumed (empty / trailing junk / non-numeric).
            char* end = nullptr;
            const int64_t v = static_cast<int64_t>(std::strtoll(vs.c_str(), &end, 0));
            if (end == vs.c_str() || *end != '\0') {
                std::fprintf(stderr, "[D457CTRL-DRV] set %s: bad value '%s'\n", nm.c_str(), vs.c_str());
                continue;
            }
            if (v < e->d.min || v > e->d.max) {
                std::fprintf(stderr, "[D457CTRL-DRV] set %s=%s -> OUT_OF_RANGE [%lld..%lld]\n",
                             nm.c_str(), vs.c_str(), (long long)e->d.min, (long long)e->d.max);
                continue;
            }
            const bool ok = WriteCtrl(sensor0, *e, v);
            std::fprintf(stderr, "[D457CTRL-DRV] set %s=%s (reg 0x%04x) -> %s\n",
                         nm.c_str(), vs.c_str(), e->reg, ok ? "ok" : "FAIL");
        }
    }
    if (getEnv != nullptr) {
        // Controls apply asynchronously (FW sets control_status=WIP, then OK). Let pending writes
        // settle before reading back, else the value register can still read the WIP sentinel.
        if (setEnv != nullptr) { std::this_thread::sleep_for(std::chrono::milliseconds(400)); }
        std::stringstream ss{std::string(getEnv)};
        std::string tok;
        while (std::getline(ss, tok, ',')) {
            if (tok.empty()) { continue; }
            const CtrlEntry* e = FindByName(tok.c_str());
            if (e == nullptr) { std::fprintf(stderr, "[D457CTRL-DRV] get: unknown control '%s'\n", tok.c_str()); continue; }
            int64_t v = 0;
            if (ReadCtrl(sensor0, *e, v)) {
                std::fprintf(stderr, "[D457CTRL-DRV] get %s (reg 0x%04x) -> %lld\n", tok.c_str(), e->reg, (long long)v);
            } else {
                std::fprintf(stderr, "[D457CTRL-DRV] get %s -> FAIL\n", tok.c_str());
            }
        }
    }
}

IReadWriteI2C::I2CResult
D457Module::WriteI2C(uddf::cdi::IDriverServices& /*driverServices*/, uint8_t /*sensorIndex*/,
                     uint16_t address, uint16_t offset, uint8_t const* data, uint16_t length)
{
    if (m_sensor0 == nullptr || data == nullptr) { return RWI2C_INTERNAL_ERROR; }
    // v1 targets the DS5 mux only (the DS5 ASIC camera-control register file at Physical 0x1A). The
    // MAX9295/MAX96712 SerDes are owned by their own stock drivers, not this interface.
    if (address != D457_MUX_I2C_ADDR) { return RWI2C_OUT_OF_RANGE; }
    // DS5 registers are 16-bit; require whole little-endian words. The caller composes higher-level
    // controls (e.g. 32-bit exposure = MSW then LSW-triggers) by ordering successive WriteI2C calls.
    if (length == 0U || (length % 2U) != 0U) { return RWI2C_OUT_OF_RANGE; }
    if (!m_sensor0->HasHwAccess()) { return RWI2C_INTERNAL_ERROR; }   // not initialized yet

    for (uint16_t i = 0; i < length; i += 2U) {
        const uint16_t reg = static_cast<uint16_t>(offset + i);
        const uint16_t val = static_cast<uint16_t>(data[i] | (static_cast<uint16_t>(data[i + 1U]) << 8));
        if (!m_sensor0->WriteReg16(reg, val)) { return RWI2C_ERROR_UNKNOWN; }
    }
    return RWI2C_SUCCESS;
}

IReadWriteI2C::I2CResult
D457Module::ReadI2C(uddf::cdi::IDriverServices& /*driverServices*/, uint8_t /*sensorIndex*/,
                    uint16_t address, uint16_t offset, uint8_t* data, uint16_t length)
{
    if (m_sensor0 == nullptr || data == nullptr) { return RWI2C_INTERNAL_ERROR; }
    if (address != D457_MUX_I2C_ADDR) { return RWI2C_OUT_OF_RANGE; }
    if (length == 0U || (length % 2U) != 0U) { return RWI2C_OUT_OF_RANGE; }
    if (!m_sensor0->HasHwAccess()) { return RWI2C_INTERNAL_ERROR; }

    for (uint16_t i = 0; i < length; i += 2U) {
        uint16_t val = 0U;
        if (!m_sensor0->ReadReg16(static_cast<uint16_t>(offset + i), val)) { return RWI2C_ERROR_UNKNOWN; }
        data[i]      = static_cast<uint8_t>(val & 0xFFU);          // little-endian to the caller
        data[i + 1U] = static_cast<uint8_t>((val >> 8) & 0xFFU);
    }
    return RWI2C_SUCCESS;
}

} // namespace uddf::cdd::d457
