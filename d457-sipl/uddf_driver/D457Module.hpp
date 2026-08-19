/*
 * Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
 *
 * D457Module.hpp — D457 GMSL camera-module driver (UBB-based).
 *
 * Composes the D457 (DS5) sensor UBB with the stock MAX9295 serializer UBB, using the
 * UDDF Building Blocks (ModuleUbb) convenience layer documented in the r39.2 Developer
 * Guide ("Guide to Writing GMSL UDDF Drivers"). ModuleUbb implements IDriver +
 * IGmslModuleControl + IModuleComponentAccess and drives the per-component lifecycle
 * (Configure/Probe/Init/Start/Stop) in the correct order — we only declare the parts.
 *
 * Pairs at runtime with the stock Max96712GmslDeserializer driver (selected by the JSON
 * transportSettings.deserInfo.name); the deserializer is a separate top-level UDDF driver.
 */
#ifndef UDDF_CDD_D457_MODULE_HPP
#define UDDF_CDD_D457_MODULE_HPP

#include "uddf/ddi/interfaces/IGmslModuleControl.hpp"
#include "uddf/ddi/interfaces/IReadWriteI2C.hpp"
#include "common/ModuleUbb.hpp"

#include "ID457CameraControl.hpp"   // structured camera-control interface (this repo)

#include <mutex>

namespace uddf::cdd::d457 {

using namespace uddf::ddi::interfaces;

class D457Sensor;   // forward decl — the module delegates register I/O to sensor 0 (it owns the
                    // cached HSL handle + the DS5 swap16/II2CBuilder mechanism). Defined in D457Sensor.hpp.

// Env-driven control harness: apply D457_CTRL / read D457_CTRL_GET via the shared control registry.
// Called by sensor 0 from D457Sensor::StartStreaming(). One of two control paths (the other is the
// ID457CameraControl SIPL interface via the injected nvsipl_camera main.cpp hook); the client-side
// GetModuleInterfaceProvider path is rejected INVALID_STATE on the current SDK. See D457Module.cpp.
void ApplyEnvControls(D457Sensor& sensor0);

/**
 * @brief D457 GMSL module = D457(DS5) sensor + MAX9295 serializer (+ optional EEPROM).
 *
 * DriverInfo.name registered by D457Library.cpp must equal the JSON `moduleDriverName`
 * ("D457" in d457_gmsl.json).
 */
class D457Module final : public uddf::cdd::gmslubb::ModuleUbb,
                         public uddf::ddi::interfaces::IReadWriteI2C,
                         public ID457CameraControl
{
public:
    explicit D457Module() = default;
    ~D457Module() override = default;

    // --- IReadWriteI2C: runtime direct register access from the SIPL client ---
    // The client retrieves this interface via INvSIPLCamera::GetModuleInterfaceProvider(idx) ->
    // IInterfaceProvider::GetInterface(IReadWriteI2C::id) at init, then calls Write/ReadI2C at
    // runtime. Both delegate to sensor 0, which holds the cached HSL handle and the DS5
    // swap16/II2CBuilder mechanism. v1 targets the DS5 mux (camera-control register file) only.
    I2CResult WriteI2C(uddf::cdi::IDriverServices& driverServices, uint8_t sensorIndex,
                       uint16_t address, uint16_t offset, uint8_t const* data, uint16_t length) override;
    I2CResult ReadI2C(uddf::cdi::IDriverServices& driverServices, uint8_t sensorIndex,
                      uint16_t address, uint16_t offset, uint8_t* data, uint16_t length) override;
    // Set register-offset width for a device. The DS5 mux uses 16-bit offsets (fixed at 2 by
    // GetDeviceTable); v1 accepts only that case.
    bool SetDeviceOffsetWidth(uint16_t address, uint8_t offsetWidth) override;

    // --- ID457CameraControl: structured camera controls for SIPL clients ---
    // A registry-backed control surface (the d4xx V4L2 control model on SIPL). Each control id maps
    // to a DS5 host control register (depth/IR block 0x4100, RGB block 0x4200); SetControl/GetControl
    // delegate to sensor 0's WriteReg16/ReadReg16 (the swap16 path), under m_ctrlMtx. The published
    // list (QueryControl) lets a client discover controls without knowing any register.
    uint32_t   GetControlCount() const noexcept override;
    CtrlResult QueryControl(uint32_t index, ControlDesc& out) const noexcept override;
    CtrlResult QueryControlById(CtrlId cid, ControlDesc& out) const noexcept override;
    CtrlResult GetControl(CtrlId cid, int64_t& value) noexcept override;
    CtrlResult SetControl(CtrlId cid, int64_t value) noexcept override;

protected:
    // GMSL ModuleUbb extension hook. ModuleUbb::GetInterface() handles the standard module
    // interfaces (IGmslModuleControl / IModuleComponentAccess / IConfigurationObject) and delegates
    // any other UUID here — where we expose IReadWriteI2C (raw debug) and ID457CameraControl.
    uddf::ddi::IInterface* doGetExtendedInterface(const uddf::ddi::UUID& uuid) override;
    /**
     * @brief Declare the module's components. Called by ModuleUbb during ConfigureDriver.
     * Order: serializer first, then sensor(s) — ModuleUbb sequences init accordingly.
     *
     * VERIFY against installed common/ModuleUbb.hpp: exact names of
     * doCreateUbbObjects / addSensorUbb / addSerializerUbb / addEepromUbb and whether the
     * MAX9295 serializer UBB ctor takes (config) or (config, linkIndex).
     */
    bool doCreateUbbObjects(const GmslModuleContext::Config& config) override;

    /** AR0234-HAWK overrides FSYNC TX ID to 0x02 for MAX9295; D457 also uses MAX9295. */
    uint8_t GetModuleFsyncTxId() const override { return 0x02U; }  // TODO: confirm for D457

private:
    // Non-owning handle to sensor 0 (the DS5 mux owner), stashed in doCreateUbbObjects. ModuleUbb
    // owns the D457Sensor instances; this just lets WriteI2C/ReadI2C reach sensor 0's register I/O.
    D457Sensor* m_sensor0 {nullptr};

    // Serializes the runtime control path (SetControl/GetControl). All DS5 mux I/O funnels through
    // sensor 0; this keeps control writes from interleaving with each other or a status read.
    std::mutex m_ctrlMtx;
};

} // namespace uddf::cdd::d457

#endif // UDDF_CDD_D457_MODULE_HPP
