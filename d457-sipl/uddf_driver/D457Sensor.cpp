/*
 * Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
 *
 * D457Sensor.cpp — D457 (DS5 ASIC) sensor UBB implementation.
 *
 * Register writes + status polling are issued as a dynamic HSL sequence via II2CBuilder
 * (write/poll) — the same HAL the AR0234 driver uses, but built at runtime from the
 * C register tables in d457_ds5_registers.h (no PyHSL codegen needed for the DS5 side).
 */
#include "D457Sensor.hpp"
#include "D457Module.hpp"   // ApplyEnvControls (D457_CTRL / D457_CTRL_GET harness)

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace uddf::cdd::d457 {

using namespace std::chrono;

namespace {
// HSLResult provides operator bool() (true == success). See uddf/cdi/HSLResult.hpp.
inline bool IsOk(const uddf::cdi::HSLResult& r) { return static_cast<bool>(r); }
inline uint16_t swap16(uint16_t x) { return static_cast<uint16_t>((x << 8) | (x >> 8)); }

// ── Stream selection ──────────────────────────────────────────────────────────────────────────
// Each stream rides its CANONICAL CSI virtual channel, fixed by the SerDes pipe DT filters after the
// pipe-per-VC fix: serializer pipe X(VC0)=RAW16 0x2E, pipe Y(VC1)=YUV422 0x1E, pipe Z(VC2)=RAW16 0x2E
// (deser pipe0/1/2 likewise). So depth(0x2E)->VC0, RGB(0x1E)->VC1, IR(0x2E)->VC2 — each on its own
// pipe. A stream can ONLY run on its canonical VC (e.g. RGB only on VC1, the lone 0x1E pipe).
inline uint8_t CanonicalVc(D457Stream s)
{
    return (s == D457Stream::COLOR_YUYV) ? 1U : (s == D457Stream::IR_Y8I) ? 2U : 0U;
}

D457Stream StreamFromName(const char* tok)
{
    if (strcasecmp(tok, "rgb") == 0 || strcasecmp(tok, "color") == 0)    { return D457Stream::COLOR_YUYV; }
    if (strcasecmp(tok, "ir") == 0  || strcasecmp(tok, "infrared") == 0) { return D457Stream::IR_Y8I; }
    return D457Stream::DEPTH_Z16;   // "depth" / default
}

// Single-stream legacy selector: D457_STREAM=depth|rgb|color|ir (used only when neither D457_STREAMS
// nor a multi-sensor query is present).
D457Stream StreamFromEnv()
{
    const char* s = std::getenv("D457_STREAM");
    return (s == nullptr) ? D457Stream::DEPTH_Z16 : StreamFromName(s);
}

// Ordered list of streams the module runs, by deviceIndex. Resolution order:
//   1. D457_STREAMS env (comma-separated names, e.g. "depth,rgb,ir" or "rgb,ir") — the test harness
//      knob; lets ANY subset/permutation run. Each name -> its canonical VC.
//   2. else multi-sensor query: 3 sensors -> depth,rgb,ir; 2 sensors -> depth,rgb.
//   3. else single sensor -> D457_STREAM (default depth).
// The query's sensorInfo order + vcIdDst MUST match this list (deviceIndex i -> list[i] on its
// canonical VC) — the tests' query generator emits exactly that.
std::vector<D457Stream> BuildStreamList(size_t numSensors)
{
    std::vector<D457Stream> out;
    const char* env = std::getenv("D457_STREAMS");
    if (env != nullptr && *env != '\0') {
        std::string s(env);
        size_t pos = 0U;
        while (pos <= s.size()) {
            const size_t comma = s.find(',', pos);
            const size_t len = (comma == std::string::npos) ? std::string::npos : (comma - pos);
            std::string tok = s.substr(pos, len);
            const size_t a = tok.find_first_not_of(" \t");
            if (a != std::string::npos) {
                const size_t b = tok.find_last_not_of(" \t");
                out.push_back(StreamFromName(tok.substr(a, b - a + 1U).c_str()));
            }
            if (comma == std::string::npos) { break; }
            pos = comma + 1U;
        }
        if (!out.empty()) { return out; }
    }
    if (numSensors >= 3U)      { out = { D457Stream::DEPTH_Z16, D457Stream::COLOR_YUYV, D457Stream::IR_Y8I }; }
    else if (numSensors == 2U) { out = { D457Stream::DEPTH_Z16, D457Stream::COLOR_YUYV }; }
    else                       { out = { StreamFromEnv() }; }
    return out;
}

// Build the DS5 mode-config register sequence for stream `s` at w x h @ fps, on its canonical VC.
// Replaces the old per-resolution static tables: the ONLY thing that varies with resolution is the
// width/height/fps config-struct words (dt/dtOut/VC are fixed per stream type), so we compute them.
// ANY DS5-supported mode works with no new table, and the resolution flows straight from the query
// (sensorInfoList[i].resolution -> m_streamCfgs -> here). out[] must hold >= D457_MODE_TABLE_MAX_ENTRIES;
// returns the entry count. Register ORDER matches the previously-validated static tables exactly.
size_t BuildModeTable(D457Stream s, uint16_t w, uint16_t h, uint16_t fps, d457_reg_t* out)
{
    const uint16_t mdvc = static_cast<uint16_t>(CanonicalVc(s) << 8);   // (vc<<8)|md_fmt(0)
    uint16_t base, dt, startCmd;
    bool hasDtOut;
    switch (s) {
        case D457Stream::COLOR_YUYV:
            base = D457_CFG_BASE_RGB; dt = D457_DT_RGB_YUV422; startCmd = D457_STREAM_START_RGB;
            hasDtOut = false;  break;   // RGB ignores dtOut; deser remaps 0x1E->0x2E
        case D457Stream::IR_Y8I:
            base = D457_CFG_BASE_IR;  dt = D457_DT_IR_R8L8;    startCmd = D457_STREAM_START_IR;
            hasDtOut = true;   break;
        case D457Stream::DEPTH_Z16:
        default:
            base = D457_CFG_BASE_DEPTH; dt = D457_DT_DEPTH_Z16; startCmd = D457_STREAM_START_DEPTH;
            hasDtOut = true;   break;
    }
    auto reg = [base](uint16_t off) { return static_cast<uint16_t>(base + off); };
    size_t n = 0;
    out[n++] = { D457_REG_MIPI_LANES, D457_VAL_MIPI_2LANE };
    out[n++] = { D457_REG_MIPI_RATE,  D457_VAL_MIPI_1000M };
    out[n++] = { reg(D457_CFG_OFF_DT),   dt };
    out[n++] = { reg(D457_CFG_OFF_MDVC), mdvc };
    if (hasDtOut) { out[n++] = { reg(D457_CFG_OFF_DTOUT), D457_DT_OUT_RAW16 }; }
    out[n++] = { reg(D457_CFG_OFF_FPS),    fps };
    out[n++] = { reg(D457_CFG_OFF_WIDTH),  w };
    out[n++] = { reg(D457_CFG_OFF_HEIGHT), h };
    out[n++] = { D457_REG_STREAM_CTRL, startCmd };
    out[n++] = { D457_REG_WAIT_MS, D457_REG_WAIT_MS };
    return n;
}

const d457_reg_t* StopTableForStream(D457Stream s, size_t& n)
{
    auto pick = [&](const d457_reg_t* t, size_t c) { n = c; return t; };
    if (s == D457Stream::COLOR_YUYV) { return pick(d457_stop_rgb, sizeof(d457_stop_rgb) / sizeof(d457_reg_t)); }
    if (s == D457Stream::IR_Y8I)     { return pick(d457_stop_ir,  sizeof(d457_stop_ir)  / sizeof(d457_reg_t)); }
    return pick(d457_stop_depth, sizeof(d457_stop_depth) / sizeof(d457_reg_t));
}
} // namespace

D457Sensor::D457Sensor(const GmslModuleContext::Config& config, uint8_t deviceIndex)
    : SensorUbb(config), m_deviceIndex(deviceIndex)
{
    if (deviceIndex < config.sensorInfoList.size()) {
        const auto& s = config.sensorInfoList[deviceIndex];
        if (s.i2cAddress != 0U)  { m_i2cAddr = s.i2cAddress; }
        if (s.resolution.width)  { m_width   = s.resolution.width; }
        if (s.resolution.height) { m_height  = s.resolution.height; }
        if (s.frameRate > 0.0f)  { m_fps     = s.frameRate; }
    }
}

bool D457Sensor::Configure(const GmslModuleContext::Config& config)
{
    if (m_deviceIndex < config.sensorInfoList.size()) {
        const auto& s = config.sensorInfoList[m_deviceIndex];
        m_width   = s.resolution.width  ? s.resolution.width  : m_width;
        m_height  = s.resolution.height ? s.resolution.height : m_height;
        m_fps     = (s.frameRate > 0.0f) ? s.frameRate : m_fps;
    }
    const size_t numSensors = config.sensorInfoList.size();
    m_numSensors = (numSensors == 0U) ? 1U : static_cast<uint8_t>(numSensors);
    // MULTI-CAMERA: with mask 0x00X1 the framework replicates this module once PER enabled link and
    // sets config.linkIndex per instance. Each link's DS5 answers at 0x1a + link*0x10 (link0=0x1a,
    // link1=0x2a; deser-translated, confirmed via d4xx enumeration). The per-link OUTPUT CSI VC
    // (link0=0/1/2, link1=4/5/6) is applied by the nvsipl_camera VC-offset patch (VI side) + the deser
    // HSL (deser output); the driver's m_vc stays the NATIVE canonical VC (0/1/2) for the DS5 mdvc tag.
    m_link    = (config.linkIndex <= 3U) ? static_cast<uint8_t>(config.linkIndex) : 0U;
    m_i2cAddr = static_cast<uint8_t>(D457_MUX_I2C_ADDR + m_link * 0x10U);
    const auto streams = BuildStreamList(m_numSensors);
    m_stream = (m_deviceIndex < streams.size()) ? streams[m_deviceIndex] : D457Stream::DEPTH_Z16;
    m_vc     = CanonicalVc(m_stream);
    switch (m_stream) {
        case D457Stream::COLOR_YUYV:
            m_cfgStatusReg = D457_RGB_CONFIG_STATUS; m_streamStatusReg = D457_RGB_STREAM_STATUS; break;
        case D457Stream::IR_Y8I:
            m_cfgStatusReg = D457_IR_CONFIG_STATUS;  m_streamStatusReg = D457_IR_STREAM_STATUS;  break;
        case D457Stream::DEPTH_Z16:
        default:
            m_cfgStatusReg = D457_DEPTH_CONFIG_STATUS; m_streamStatusReg = D457_DEPTH_STREAM_STATUS; break;
    }
    // Streams this module's owner (deviceIndex 0) programs+plays on ITS link's DS5 in StartStreaming.
    m_streamCfgs.clear();
    const uint16_t w = static_cast<uint16_t>(m_width);
    const uint16_t h = static_cast<uint16_t>(m_height);
    for (size_t i = 0; i < streams.size(); ++i) {
        uint16_t ww = w, hh = h; float ff = m_fps;
        if (i < config.sensorInfoList.size()) {
            const auto& si = config.sensorInfoList[i];
            if (si.resolution.width)  { ww = static_cast<uint16_t>(si.resolution.width); }
            if (si.resolution.height) { hh = static_cast<uint16_t>(si.resolution.height); }
            if (si.frameRate > 0.0f)  { ff = si.frameRate; }
        }
        m_streamCfgs.push_back(D457ModeReq{ streams[i], ww, hh, static_cast<uint16_t>(ff + 0.5f) });
    }
    return true;
}

uddf::ddi::DeviceTable D457Sensor::GetDeviceTable() const
{
    uddf::ddi::DeviceTable t;
    // Each link's DS5 mux is registered ONCE, by that link's owner (streamIdx 0). The HSL I2C encoder
    // rejects a duplicate mapping, so the non-owner streams on a link register nothing (their writes to
    // the mux route through the owner's mapping). Multi-cam: link0 owner (deviceIndex 0) registers
    // 0x1a, link1 owner (deviceIndex 3) registers 0x2a — distinct virtual addresses, so no duplicate.
    if (m_deviceIndex != 0U) {
        return t;  // empty: this link's DS5 mux already registered by this module's deviceIndex 0
    }
    t.push_back(uddf::ddi::DeviceTableEntry{
        .i2cAddress    = m_i2cAddr,          // per-link virtual: link0=0x1a, link1=0x2a
        .offsetWidth   = 2U,                 // DS5 registers use 16-bit offsets
        .dataWidth     = 2U,                 // 16-bit values (LE)
        .flags         = 0U,
        .hslI2cAddress = D457_MUX_PHYS_ADDR,  // physical def-addr (0x10): framework maps virtual->0x10
        .deviceIndex   = m_deviceIndex,
    });
    return t;
}

uddf::ddi::GpioPinTable D457Sensor::GetGpioPinTable() const
{
    return {};  // no module GPIOs needed on the GMSL link for basic bring-up
}

bool D457Sensor::ProbeHardware(const GmslModuleContext& context, bool alreadyInitialized)
{
    (void)alreadyInitialized;
    if (context.hwAccess == nullptr || context.driverServices == nullptr) { return false; }
    UDDF_LOG_INFO(*context.driverServices, "D457[%u] ProbeHardware (mux 0x%02x)", m_deviceIndex, m_i2cAddr);
    // TODO: read a stable DS5 identity/version register and validate. For first bring-up,
    // succeed if the reverse channel is up (a follow-up adds a ReadI2C identity check).
    return true;
}

bool D457Sensor::Init(const GmslModuleContext& context)
{
    if (context.hwAccess == nullptr) { return false; }
    UDDF_LOG_INFO(*context.driverServices, "D457[%u] Init", m_deviceIndex);
    // Cache the HSL handles for the runtime direct-register control path (the module's IReadWriteI2C
    // interface call carries no GmslModuleContext). The handle SIPL gives us here is valid for the
    // device-block lifetime; refreshed again at StartStreaming. See WriteReg16()/ReadReg16().
    m_hwAccess = context.hwAccess;
    m_driverServices = context.driverServices;
    // No persistent DS5 init beyond per-stream mode programming (done in StartStreaming).
    return true;
}

bool D457Sensor::RunRegTable(const GmslModuleContext& context,
                             const d457_reg_t* table, size_t count, bool pollStatus)
{
    auto& hw = *context.hwAccess;
    uddf::cdi::IHSLDynamicSequence& seq = hw.GetDynamicSequence();
    // The DS5 mux answers at Physical I2C address 0x1A over the GMSL tunnel (confirmed by
    // probe: Virtual-mode resolution and Physical 0x10 do not reach it). Use Physical 0x1A.
    uddf::cdi::II2CBuilder* b = seq.i2cBuilder(m_i2cAddr, uddf::cdi::I2CAddressMode::Physical);
    if (b == nullptr) { return false; }

    for (size_t i = 0; i < count; ++i) {
        if (table[i].reg == D457_REG_WAIT_MS) {
            seq.delay(milliseconds(500));
        } else {
            // DS5 mode/stream regs are not safely read-back-verifiable mid-config.
            // BOTH the register offset AND the 16-bit data word must be byte-swapped: the SIPL HSL
            // II2CBuilder serializes 16-bit words big-endian, but the DS5 register interface is
            // little-endian (matches the realsense/Hololink append_uint16_le convention). Proven on
            // the rig over raw i2c: data written little-endian makes i2cHostIfStreamControl read the
            // correct config (dt=0x31, 1280x720) AND treat 0x1000 as the byte array {streamId=0,
            // cmd=0x02 play}. Without the data swap, 0x0200 lands as {streamId=2, cmd=0=ignore} and
            // the depth-play dispatch is silently skipped (no EVT_I2C_HOST_STREAM_CONFIG), so the
            // stream never starts. See FINDINGS §5j.
            b->write(swap16(table[i].reg), swap16(table[i].val), uddf::cdi::I2CWriteFlags::NO_READ_VERIFY);
            seq.delay(milliseconds(100));   // DS5 needs settle time between writes (realsense uses 0.1s)
        }
    }

    if (pollStatus) {
        // On-device poll: config-status == 0x1 and stream-status == 0x2.
        b->poll(swap16(m_cfgStatusReg),    D457_STATUS_STREAMING, 0xFFFFU,
                milliseconds(D457_START_POLL_TIME_MS), D457_START_MAX_COUNT);
        b->poll(swap16(m_streamStatusReg), D457_STREAM_STREAMING, 0xFFFFU,
                milliseconds(D457_START_POLL_TIME_MS), D457_START_MAX_COUNT);
    }

    hw.SubmitSequence(seq);
    return IsOk(hw.GetErrorState());
}

bool D457Sensor::WriteReg16(uint16_t reg, uint16_t val)
{
    // Runtime control path (module IReadWriteI2C). Uses the handle cached at Init()/StartStreaming(),
    // not a GmslModuleContext. Mirrors RunRegTable's single-write op exactly: a fresh one-shot HSL
    // sequence to the DS5 mux at Physical 0x1A, with BOTH the register offset and the 16-bit data
    // word byte-swapped (HSL serializes big-endian; DS5 regs are little-endian — FINDINGS §5j).
    if (m_hwAccess == nullptr) { return false; }
    auto& hw = *m_hwAccess;
    uddf::cdi::IHSLDynamicSequence& seq = hw.GetDynamicSequence();
    uddf::cdi::II2CBuilder* b = seq.i2cBuilder(m_i2cAddr, uddf::cdi::I2CAddressMode::Physical);
    if (b == nullptr) { return false; }
    b->write(swap16(reg), swap16(val), uddf::cdi::I2CWriteFlags::NO_READ_VERIFY);
    hw.SubmitSequence(seq);
    return IsOk(hw.GetErrorState());
}

bool D457Sensor::ReadReg16(uint16_t reg, uint16_t& valOut)
{
    if (m_hwAccess == nullptr) { return false; }
    // Mirror WriteReg16's addressing. IHardwareAccess::ReadI2C's first arg is the I2C *address*, not a
    // device index: target the DS5 mux at Physical 0x1A (the earlier /*deviceIndex=*/0U read address 0
    // and always failed). The register offset is byte-swapped on the wire like the write path (HSL
    // serializes big-endian; DS5 regs are little-endian), and the returned bytes come back in DS5
    // little-endian order (raw[0]=low, raw[1]=high).
    // ⚠ Never call while RGB streams: an I2C read on the shared DS5 mux stalls the RealTek RGB ISP
    //   (see FINDINGS gotchas) — reads are safe only for the depth/IR single-stream case.
    uint8_t raw[2] = {0U, 0U};
    const uddf::cdi::HSLResult r =
        m_hwAccess->ReadI2C(m_i2cAddr, swap16(reg), sizeof(raw), raw,
                            uddf::cdi::I2CAddressMode::Physical);
    if (!IsOk(r)) { return false; }
    valOut = static_cast<uint16_t>(raw[0] | (static_cast<uint16_t>(raw[1]) << 8));
    return true;
}

bool D457Sensor::StartStreaming(const GmslModuleContext& context)
{
    if (context.hwAccess == nullptr) { return false; }
    // Refresh the cached HSL handles for the runtime control path (see Init()/WriteReg16()).
    m_hwAccess = context.hwAccess;
    m_driverServices = context.driverServices;

    // In the simultaneous depth+RGB config, sensor 0 owns ALL DS5 programming (it also owns the shared
    // I2C device-table mapping). Configuring/playing RGB from sensor 1 WHILE depth is already streaming
    // disrupts the RGB stream (it stops after ~3-4 frames → VI PIX_SHORT → both pipelines torn down).
    // So sensor 0 configures and plays BOTH streams in ONE submitted sequence, and the higher-index
    // sensor is capture-only (its SIPL pipeline still captures its VC; it just issues no mux I2C).
    // Owner = deviceIndex 0 of this (per-link) module: configures+plays ITS link's DS5 (0x1a+link*0x10).
    // The other streams on the link are capture-only (their SIPL pipeline still captures its VC).
    if (m_deviceIndex != 0U) {
        UDDF_LOG_INFO(*context.driverServices,
                      "D457[%u] StartStreaming: capture-only VC%u link%u (DS5 driven by deviceIndex 0)",
                      m_deviceIndex, m_vc, m_link);
        return true;
    }

    // Sensor 0 programs the DS5 ONCE for the whole module: stop every stream in the list, then play
    // every stream (each on its canonical VC). Doing it in one submitted sequence avoids reconfiguring
    // the shared mux while a stream already runs (which stalls the RealTek RGB ISP). The list is the
    // same BuildStreamList() every instance computes, so it matches the query's sensorInfo order.
    UDDF_LOG_INFO(*context.driverServices,
                  "D457[%u] StartStreaming: link%u DS5@0x%02x programming %zu stream(s) (numSensors=%u)",
                  m_deviceIndex, m_link, m_i2cAddr, m_streamCfgs.size(), m_numSensors);
    // Stop any prior streams first (realsense start() does this), then play each.
    for (const auto& cfg : m_streamCfgs) {
        size_t sn = 0;
        const d457_reg_t* stop = StopTableForStream(cfg.stream, sn);
        RunRegTable(context, stop, sn, /*pollStatus=*/false);
    }
    d457_reg_t modebuf[D457_MODE_TABLE_MAX_ENTRIES];
    bool ok = true;
    for (const auto& cfg : m_streamCfgs) {
        const char* sname = (cfg.stream == D457Stream::COLOR_YUYV) ? "RGB"
                          : (cfg.stream == D457Stream::IR_Y8I)     ? "IR"  : "DEPTH";
        // Resolution comes from the query (D457_WIDTH/HEIGHT/FPS env -> JSON -> sensorInfo). Reject
        // a clearly-invalid mode here; an unsupported-but-plausible mode just won't stream (the DS5
        // firmware validates it) and the test's drop/frame check catches that.
        if (cfg.width == 0U || cfg.height == 0U || cfg.fps == 0U ||
            cfg.width > 4096U || cfg.height > 4096U || cfg.fps > 120U) {
            UDDF_LOG_ERROR(*context.driverServices,
                "D457[%u] StartStreaming: %s invalid mode %ux%u@%u",
                m_deviceIndex, sname, cfg.width, cfg.height, cfg.fps);
            return false;
        }
        const size_t mn = BuildModeTable(cfg.stream, cfg.width, cfg.height, cfg.fps, modebuf);
        UDDF_LOG_INFO(*context.driverServices, "D457[%u]   play %s %ux%u@%u on VC%u",
                      m_deviceIndex, sname, cfg.width, cfg.height, cfg.fps, CanonicalVc(cfg.stream));
        ok = RunRegTable(context, modebuf, mn, /*pollStatus=*/false);
        if (!ok) {
            UDDF_LOG_ERROR(*context.driverServices, "D457[%u] StartStreaming: %s mode write failed", m_deviceIndex, sname);
            return false;
        }
    }
    // ⚠ Do NOT read the DS5 status back here. An I2C read on the shared DS5 mux WHILE the RGB stream
    // is running DISRUPTS it (RealTek RGB ISP stalls after ~3-4 frames; see FINDINGS gotchas).

    // Apply any env-requested camera controls (D457_CTRL / D457_CTRL_GET) now that the stream is up
    // and HW access is live. No-op unless those env vars are set. Sensor 0 owns the DS5 mux, so this
    // is the safe place to issue control register writes. (D457_CTRL_GET reads are depth/IR-safe;
    // avoid reading while RGB streams.)
    ApplyEnvControls(*this);

    return true;
}

bool D457Sensor::StopStreaming(const GmslModuleContext& context)
{
    if (context.hwAccess == nullptr) { return false; }
    // Mirror StartStreaming: sensor 0 owns all DS5 I/O. The capture-only higher-index sensor issues no
    // mux I2C (avoids perturbing the shared mux while the other stream may still run).
    if (m_deviceIndex != 0U) {
        UDDF_LOG_INFO(*context.driverServices, "D457[%u] StopStreaming: capture-only (no-op)", m_deviceIndex);
        return true;
    }
    UDDF_LOG_INFO(*context.driverServices, "D457[%u] StopStreaming", m_deviceIndex);
    // Stop every stream in the module's list (mirrors StartStreaming).
    bool ok = true;
    for (const auto& cfg : m_streamCfgs) {
        size_t n = 0;
        const d457_reg_t* stop = StopTableForStream(cfg.stream, n);
        ok = RunRegTable(context, stop, n, /*pollStatus=*/false) && ok;
    }
    return ok;
}

bool D457Sensor::SetSensorControls(uddf::cdi::IHardwareAccess&,
                                   uddf::cdi::IDriverServices&,
                                   const SensorControls&)
{
    // D457 emits processed frames; host AE/AWB does not drive the DS5. Accept as no-op.
    return true;
}

bool D457Sensor::GetSensorAttributes(uddf::cdi::IDriverServices&,
                                     SensorAttributes& attributes) const
{
    attributes = SensorAttributes{};
    attributes.numActiveExposures = 0U;   // not host-controlled
    return true;
}

bool D457Sensor::ParseTopEmbeddedData(uddf::cdi::IDriverServices&,
                                      const EmbeddedDataChunk&, EmbeddedDataInfo&)
{
    return false;  // TODO: parse the ~68B DS5 metadata line if embedded data is enabled
}

bool D457Sensor::ParseBottomEmbeddedData(uddf::cdi::IDriverServices&,
                                         const EmbeddedDataChunk&, EmbeddedDataInfo&)
{
    return false;
}

} // namespace uddf::cdd::d457
