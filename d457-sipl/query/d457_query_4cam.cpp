/*
 * d457_query.cpp — SIPL query database plugin for the D457 GMSL camera.
 *
 * Built into libnvsipl_qry_d457.so, installed in /usr/lib/nvsipl_drv. SIPL's
 * libnvsipl_query.so (CNvMQuery::ParseDatabase) dlopen()s every libnvsipl_qry_*.so there and
 * dlsym()s `const char* CNvMQuery_GetJsonData(void)` (ABI confirmed by disassembly) to get the
 * camera-config JSON. This registers D457 so `nvsipl_camera -c D457_Camera` can find it.
 *
 * Schema replicated EXACTLY from the stock libnvsipl_qry_nova0_hawk.so (dumped via its own
 * CNvMQuery_GetJsonData): the top level is a JSON ARRAY of two objects —
 *   [0] { "cameraConfigs": [ <module defs> ] }
 *   [1] { "platformTransportSettings": [ { board: name/boardIdPrefix/enableMasks,
 *                                          "transportSettings": [ <transport> ] } ] }
 * Boards are matched to the running unit by `boardIdPrefix` (vs GetBoardModel, which on this
 * rig = "NVIDIA Jetson AGX Thor Developer Kit"). Values resolved from the live d4xx DT:
 * MAX9295@0x40, MAX96712@0x29, sensor(depth)@0x1a, CSI-A, i2c bus 9, 2 lanes / 16bpp.
 * moduleDriverName "D457" matches DriverInfo.name in D457Library.cpp.
 *
 * ── SIMULTANEOUS depth + RGB (two sensors / two pipelines) ──
 * SIPL maps ONE sensorInfo -> ONE virtualChannel -> ONE capture pipeline (NvSIPLDeviceBlockInfo.hpp:
 * CameraModuleInfo.sensorInfo and SensorInfo.vcInfo are singular; SensorInfo.id IS the pipeline index).
 * So depth+RGB is expressed as TWO sensorInfo entries on the SAME module/mux (i2c 0x1a), distinguished
 * only by id/deviceIndex and their virtual channel:
 *   id 0 / deviceIndex 0  ->  DEPTH, vcIdSrc=vcIdDst=0   (DS5 emits VC0/DT 0x2E)
 *   id 1 / deviceIndex 1  ->  RGB,   vcIdSrc=vcIdDst=1   (DS5 emits VC1/DT 0x1E, deser remaps ->0x2E)
 * The D457 module driver creates one D457Sensor per entry and keys the stream off deviceIndex
 * (D457Sensor::Configure). The consumer reads two INvSIPLClient queues (pipeline 0 = depth, 1 = RGB).
 *
 * CONFIRMED shape (dumped libnvsipl_qry_nova0_hawk.so, 2026-06-24): a 2-sensor module = TWO
 * sensorInfo entries (id 0/1, deviceIndex 0/1), each with ONE virtualChannels entry (vcIdSrc/Dst 0
 * and 1). The NON-stereo HAWK variant (no "sensorGroup", isTriggerModeEnabled=false) is the template
 * for INDEPENDENT (non-synchronized) streams — exactly depth+RGB — which is what this mirrors.
 *
 * ⚠ ONE DIFFERENCE FROM HAWK (verify on-rig): HAWK's two sensors have DISTINCT i2c addresses (0x18,
 * 0x10) because they are two physical AR0234 chips. The D457's two streams are the SAME physical DS5
 * mux (0x1a), so both entries below use 0x1a. If SIPL rejects two sensors at one address (or assigns
 * them two virtual addresses needing a 2nd MAX9295 translator slot ARRAY2 0x0046/0x0047->0x10), give
 * the RGB entry a distinct virtualI2CAddress and add that translator slot in patch_max9295_d457.py.
 *
 * ── PoC POWER CONTROL (transportSettings.powerControlInfo) ──
 * Wires the MAX20087 PoC controller (@ i2c 0x28, bus 9) into the device block so SIPL OWNS the
 * camera power: the framework calls the MAX20087 driver's Init() (drives all outputs OFF) then
 * EnableModulePower at device-block bring-up — i.e. a clean off→on cold-start each run. This is
 * the supported replacement for the manual `i2cset -y 9 0x28 0x01 0x00/0x1f` DS5 power-cycle the
 * DS5 wedge (RSDSO-21558) otherwise needs. The driver name "MAX20087" must match DriverInfo.name
 * in libnvuddf_max20087_library.so.
 *   - Only `moduleInfo` (the PoC switch) is wired. The HAWK template also has a `deserializerInfo`
 *     half (TegraDeserPowerDriver, a GPIO-gated deser rail); omitted here because this board's deser
 *     rail is always-on with no power GPIO (see dt/ overlay [V4] notes). Add it only if needed.
 *   - VERIFY on-rig: (a) the framework's Init→Enable gives enough off-dwell to clear the wedge — if
 *     not, power is at least OFF between runs (Deinit on exit), which itself supplies the discharge;
 *     (b) the channel the camera is wired to gets enabled (manual cmd used 0x1f = all 4 channels).
 */

#include <cstdlib>
#include <string>

extern "C" const char* CNvMQuery_GetJsonData(void)
{
    // ── Resolution is DATA-DRIVEN (no recompile to change it) ──
    // D457_WIDTH/D457_HEIGHT/D457_FPS env vars replace the __W__/__H__/__F__ tokens below. getenv()
    // works because SIPL dlopen's this plugin INTO the nvsipl_camera process, so the command-line env
    // is visible here. The substituted JSON sizes the SIPL pipeline; the driver reads the SAME parsed
    // resolution (sensorInfoList -> D457Sensor) to program the DS5 — one source of truth. Default 720p30.
    static std::string json = R"JSON(
[
  {
    "cameraConfigs": [
      {
        "name": "D457_Camera",
        "moduleDriverName": "D457",
        "type": "GMSL",
        "description": "RealSense D457 GMSL (depth VC0 + RGB VC1 + IR VC2)",
        "serInfo": { "name": "MAX9295", "i2cAddress": "0x40" },
        "linkMode": "LINK_MODE_GMSL2_6GBPS",
        "fsyncMode": "osc_manual",
        "mipiSettings": { "dphyRate": 594000, "phyMode": "dphy", "lanes": 2 },
        "sensorInfo": [
          {
            "name": "D457",
            "id": 0,
            "description": "RealSense D457 DS5 ASIC depth (Z16) via DS5 mux @ 0x1A, VC0",
            "i2cAddress": "0x1a",
            "numContext": 1,
            "isTriggerModeEnabled": false,
            "deviceIndex": 0,
            "virtualChannels": [
              {
                "vcIdSrc": 0,
                "vcIdDst": 0,
                "cfa": "rggb",
                "embeddedTopLines": 0,
                "embeddedBottomLines": 0,
                "inputFormat": "raw16",
                "width": __W__,
                "height": __H__,
                "fps": __F__,
                "isEmbeddedDataTypeEnabled": false
              }
            ]
          },
          {
            "name": "D457",
            "id": 1,
            "description": "RealSense D457 DS5 ASIC color (YUYV->RAW16) via DS5 mux @ 0x1A, VC1",
            "i2cAddress": "0x1a",
            "numContext": 1,
            "isTriggerModeEnabled": false,
            "deviceIndex": 1,
            "virtualChannels": [
              {
                "vcIdSrc": 1,
                "vcIdDst": 1,
                "cfa": "rggb",
                "embeddedTopLines": 0,
                "embeddedBottomLines": 0,
                "inputFormat": "raw16",
                "width": __W__,
                "height": __H__,
                "fps": __F__,
                "isEmbeddedDataTypeEnabled": false
              }
            ]
          }
        ],
        "cryptoConfigName": ""
      }
    ]
  },
  {
    "platformTransportSettings": [
      {
        "name": "advantech_mic742_thor",
        "description": "Advantech MIC-742 Jetson Thor (D457 GMSL on CSI-A)",
        "boardIdPrefix": "NVIDIA Jetson AGX Thor",
        "enableMasks": [ "0x1111" ],
        "transportSettings": [
          {
            "name": "transportSettings_d457_AB",
            "type": "GMSL",
            "description": "GMSL transport for D457 - CSI-AB (2x4 capture descriptor). Tegra lane count is forced to 2 by the libnvsipl.so BuildSensorProperty patch (see FINDINGS top); deser PHY is patched to d4xx 2x4-on-PHY1/2-lane.",
            "csiPort": "csi-ab",
            "deserInfo": { "name": "Max96712GmslDeserializer", "i2cAddress": "0x29" },
            "powerControlInfo": {
              "moduleInfo": { "name": "MAX20087", "i2cAddress": "0x28" }
            },
            "i2cDevice": 9,
            "desI2CPort": 0,
            "phyMode": "dphy",
            "groupInitProg": true
          }
        ]
      }
    ]
  }
]
)JSON";
    auto envOr = [](const char* k, const char* d) {
        const char* v = std::getenv(k);
        return std::string((v != nullptr && *v != '\0') ? v : d);
    };
    auto replaceAll = [](std::string& s, const char* from, const std::string& to) {
        const std::string f(from);
        for (size_t p = s.find(f); p != std::string::npos; p = s.find(f, p + to.size())) {
            s.replace(p, f.size(), to);
        }
    };
    replaceAll(json, "__W__", envOr("D457_WIDTH",  "1280"));
    replaceAll(json, "__H__", envOr("D457_HEIGHT", "720"));
    replaceAll(json, "__F__", envOr("D457_FPS",    "30.0"));
    return json.c_str();
}
