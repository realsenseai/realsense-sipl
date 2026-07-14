/*
 * Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
 *
 * D457Sensor.hpp — D457 (DS5 ASIC) sensor UBB for the SIPL GMSL UDDF stack.
 *
 * Authored for the RealSense D457-over-GMSL bring-up on Jetson Thor (L4T r39.2),
 * against the public "Camera SIPL" package (Jetson_SIPL_API_R39.2.0). Modeled on
 * uddf/drivers/sensors/AR0234 + uddf/drivers/common/SensorUbb.
 *
 * The D457 is NOT a raw Bayer sensor: the DS5 ASIC emits already-processed depth(Z16)/
 * IR(Y8)/color(YUYV) over a single CSI source (UYVY8_1X16, 16 bpp), controlled via the
 * DS5 mux at I2C 0x1A. This UBB is a thin control path (mode select + stream start/stop +
 * status poll) using the DS5 register tables in d457_ds5_registers.h, issued as dynamic
 * HSL sequences (II2CBuilder). Exposure/gain/embedded-data are minimal/no-ops.
 */
#ifndef UDDF_CDD_D457_SENSOR_HPP
#define UDDF_CDD_D457_SENSOR_HPP

#include <cstdint>
#include <vector>

#include "uddf/cdi/IHardwareAccess.hpp"
#include "uddf/cdi/IDriverServices.hpp"
#include "uddf/cdi/hal/II2CBuilder.hpp"
#include "uddf/cdi/HSLResult.hpp"
#include "uddf/ddi/interfaces/IGmslModuleControl.hpp"
#include "uddf/ddi/interfaces/ICameraSensorControl.hpp"
#include "uddf/ddi/interfaces/ICameraSensorInfo.hpp"
#include "common/SensorUbb.hpp"

#include "d457_ds5_registers.h"   // ported DS5 mode/stream/status tables (this repo)

namespace uddf::cdd::d457 {

using namespace uddf::ddi::interfaces;
using uddf::cdd::gmslubb::SensorUbb;

enum class D457Stream : uint8_t { DEPTH_Z16, COLOR_YUYV, IR_Y8I };

// One stream's programmed mode: which stream + the resolution to write into the DS5 config struct.
// Built per-module in Configure() from the query sensorInfo resolutions, so resolution is data-driven
// (D457_WIDTH/HEIGHT/FPS env -> query JSON -> here). See d457_ds5_registers.h "Resolution is PARAMETRIC".
struct D457ModeReq {
    D457Stream stream;
    uint16_t   width;
    uint16_t   height;
    uint16_t   fps;
};

/**
 * @brief UBB object for the D457 (DS5 ASIC) on a GMSL link (MAX9295 serializer).
 * One instance == one logical stream == one SIPL capture pipeline (= one sensorInfo entry).
 *
 * Stream/VC selection (see d457_ds5_registers.h "VIRTUAL-CHANNEL ASSIGNMENT"):
 *   - Two-sensor config (depth+RGB simultaneous): deviceIndex picks the stream and VC —
 *     deviceIndex 0 = DEPTH on VC0, deviceIndex 1 = RGB on VC1.
 *   - Single-sensor config (legacy/debug): the D457_STREAM env var picks the stream, VC0.
 */
class D457Sensor final : public SensorUbb
{
public:
    explicit D457Sensor(const GmslModuleContext::Config& config, uint8_t deviceIndex);
    ~D457Sensor() = default;

    // --- DeviceUbb ---
    const char* GetName() const override { return "D457"; }
    bool Configure(const GmslModuleContext::Config& config) override;
    uddf::ddi::DeviceTable GetDeviceTable() const override;
    uddf::ddi::GpioPinTable GetGpioPinTable() const override;
    bool ProbeHardware(const GmslModuleContext& context, bool alreadyInitialized) override;
    bool Init(const GmslModuleContext& context) override;

    // --- SensorUbb ---
    bool StartStreaming(const GmslModuleContext& context) override;
    bool StopStreaming(const GmslModuleContext& context) override;

    // --- Runtime direct-register access (backs the module's IReadWriteI2C interface) ---
    // The IReadWriteI2C call from the SIPL client carries no GmslModuleContext, so these operate on
    // the IHardwareAccess handle cached at Init()/StartStreaming(). They reuse the exact swap16 +
    // II2CBuilder mechanism proven by RunRegTable (see d457_ds5_registers.h header / FINDINGS §5j).
    bool HasHwAccess() const { return m_hwAccess != nullptr; }
    bool WriteReg16(uint16_t reg, uint16_t val);          // write one DS5 16-bit register (caller LE)
    bool ReadReg16(uint16_t reg, uint16_t& valOut);       // read  one DS5 16-bit register (caller LE)

    // --- ICameraSensorControl ---
    bool SetSensorControls(uddf::cdi::IHardwareAccess& hwAccess,
                           uddf::cdi::IDriverServices& driverServices,
                           const SensorControls& controls) override;

    // --- ICameraSensorInfo ---
    bool GetSensorAttributes(uddf::cdi::IDriverServices& driverServices,
                             SensorAttributes& attributes) const override;
    bool ParseTopEmbeddedData(uddf::cdi::IDriverServices& driverServices,
                              const EmbeddedDataChunk& chunk,
                              EmbeddedDataInfo& info) override;
    bool ParseBottomEmbeddedData(uddf::cdi::IDriverServices& driverServices,
                                 const EmbeddedDataChunk& chunk,
                                 EmbeddedDataInfo& info) override;

private:
    // Build+submit a dynamic HSL sequence that writes a DS5 register table to the mux.
    // If pollStatus, append poll() ops on the config/stream status registers.
    bool RunRegTable(const GmslModuleContext& context,
                     const d457_reg_t* table, size_t count, bool pollStatus);

    uint8_t  m_deviceIndex {0U};
    uint8_t  m_numSensors {1U};   // total D457 sensors in this config (2 = simultaneous depth+RGB)
    uint8_t  m_link {0U};         // GMSL link (0..3) from GmslModuleContext::Config.linkIndex — multi-camera
    uint8_t  m_i2cAddr {D457_MUX_I2C_ADDR};
    uint32_t m_width {1280U};
    uint32_t m_height {720U};
    float    m_fps {30.0f};
    D457Stream m_stream {D457Stream::DEPTH_Z16};
    uint8_t  m_vc {0U};   // CSI virtual channel for this stream (depth VC0, RGB VC1 when simultaneous)
    uint16_t m_cfgStatusReg {D457_DEPTH_CONFIG_STATUS};
    uint16_t m_streamStatusReg {D457_DEPTH_STREAM_STATUS};

    // Module-wide (stream, resolution) list that sensor 0 programs in StartStreaming, built in
    // Configure() from the query sensorInfo resolutions. Resolution is data-driven; see D457ModeReq.
    std::vector<D457ModeReq> m_streamCfgs;

    // HSL handles cached at Init()/StartStreaming() for the runtime control path (non-owning;
    // lifetime is the device-block lifetime). See WriteReg16()/ReadReg16().
    uddf::cdi::IHardwareAccess* m_hwAccess {nullptr};
    uddf::cdi::IDriverServices* m_driverServices {nullptr};
};

} // namespace uddf::cdd::d457

#endif // UDDF_CDD_D457_SENSOR_HPP
