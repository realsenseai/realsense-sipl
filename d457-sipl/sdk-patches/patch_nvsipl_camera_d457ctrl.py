#!/usr/bin/env python3
"""patch_nvsipl_camera_d457ctrl.py — add the D457 camera-control hook to the GMSL nvsipl_camera sample.

The stock nvsipl_camera captures frames but never touches a custom module interface, so it can't
exercise the D457 ID457CameraControl interface. This patch adds a small, env-driven, opt-in hook
(applied after Init(), before Start()) that:

  D457_CTRL_LIST=1            -> print the published control list (QueryControl)
  D457_CTRL="name=val;..."    -> SetControl each (e.g. "emitter_mode=0;depth_exposure_us=8000")
  D457_CTRL_GET="name,..."    -> GetControl each and print the value

It is idempotent (re-running is a no-op) and touches three files of the sample dir:
  - CMakeLists.txt        : add uddf/include + uddf/cdd_d457 to the include path
  - main.cpp              : include the interface header + the env hook before upMaster->Start()
  - CNvSIPLMaster.hpp     : add a GetModuleInterfaceProvider() passthrough (mirrors the pipeline one)

USAGE
  python3 patch_nvsipl_camera_d457ctrl.py [SAMPLE_DIR]
    SAMPLE_DIR defaults to the rig's full-SIPL camera sample:
    /home/mic-742/sipl_full/usr/src/jetson_sipl_api/sipl/samples/camera

The ID457CameraControl.hpp / uddf headers must already be reachable at SAMPLE_DIR/../../uddf/...
(true on the rig once the driver sources are synced into uddf/cdd_d457).
"""
import os
import sys

DEFAULT_DIR = "/home/mic-742/sipl_full/usr/src/jetson_sipl_api/sipl/samples/camera"


def patch_file(path, anchor, replacement, skip_if):
    with open(path, "r", encoding="utf-8") as f:
        text = f.read()
    if skip_if in text:
        print(f"  [skip] {os.path.basename(path)}: already patched")
        return False
    if anchor not in text:
        raise SystemExit(f"  [FAIL] {os.path.basename(path)}: anchor not found:\n{anchor!r}")
    if text.count(anchor) != 1:
        raise SystemExit(f"  [FAIL] {os.path.basename(path)}: anchor not unique ({text.count(anchor)}x)")
    with open(path, "w", encoding="utf-8") as f:
        f.write(text.replace(anchor, replacement, 1))
    print(f"  [ok]   {os.path.basename(path)}: patched")
    return True


# ── CMakeLists.txt: add the uddf include dirs ─────────────────────────────────────────────────────
CMAKE_ANCHOR = """    ${HOLOLINK_INCLUDE_DIR}
)"""
CMAKE_REPL = """    ${HOLOLINK_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../uddf/include       # [D457CTRL] uddf::ddi::IInterface / UUID
    ${CMAKE_CURRENT_SOURCE_DIR}/../../uddf/cdd_d457       # [D457CTRL] ID457CameraControl.hpp
)"""

# ── main.cpp: include the interface header + a file-scope handle for the resolved control iface ────
INC_ANCHOR = '#include "CNvSIPLMaster.hpp"'
INC_REPL = r'''#include "CNvSIPLMaster.hpp"
#include "ID457CameraControl.hpp"   // [D457CTRL] structured D457 camera-control interface
#include <sstream>                  // [D457CTRL]
#include <cstdlib>                  // [D457CTRL]
#include <cstdio>                   // [D457CTRL]
static uddf::cdd::d457::ID457CameraControl* g_d457Ctrl = nullptr;  // [D457CTRL] resolved in config phase'''

# ── main.cpp: resolve the module control interface in the config phase (pre-Init), where the SDK
#    also fetches the pipeline interface provider. The interface pointer stays valid for the device
#    object's lifetime, so we use it later (before Start) once HW access is up. ──────────────────────
CFG_ANCHOR = r'''                if (pISPStatCustomInterface != nullptr) {
                    upCons->SetISPStatCustomInterface(pISPStatCustomInterface);
                }
            }'''
CFG_REPL = r'''                if (pISPStatCustomInterface != nullptr) {
                    upCons->SetISPStatCustomInterface(pISPStatCustomInterface);
                }
            }
            // [D457CTRL] resolve the D457 module control interface here (config phase, pre-Init) —
            // same window the pipeline provider above uses. Stored for use just before Start().
            if (g_d457Ctrl == nullptr) {
                nvsipl::IInterfaceProvider* d457Prov = nullptr;
                SIPLStatus d457St = upMaster->GetModuleInterfaceProvider(uSensor, d457Prov);
                if (d457St == NVSIPL_STATUS_OK && d457Prov != nullptr) {
                    // Must match uddf::cdd::d457::uuid::ID457_CAMERA_CONTROL_ID (the authoritative
                    // definition) in ID457CameraControl.hpp -- update BOTH copies in this file together.
                    static const nvsipl::UUID kD457CtrlId(0xd457c711U, 0x9a2eU, 0x4f6bU, 0xb3d1U,
                                                          0x52U, 0x6fU, 0xa4U, 0x18U, 0x0cU, 0x37U);
                    g_d457Ctrl = reinterpret_cast<uddf::cdd::d457::ID457CameraControl*>(
                        d457Prov->GetInterface(kD457CtrlId));
                }
                std::cout << "[D457CTRL] cfg-phase GetModuleInterfaceProvider(" << uSensor
                          << ") status=" << (int)d457St << " ctrl=" << (void*)g_d457Ctrl << "\n";
            }'''

# ── main.cpp: the env-driven control hook, just AFTER the stream starts (Operational state — the
#    module interface provider is rejected with INVALID_STATE before Start on this SDK build). ──────
START_ANCHOR = '''    status = upMaster->Start();
    CHK_STATUS_AND_RETURN(status, "Master start");'''
START_REPL = r'''    status = upMaster->Start();
    CHK_STATUS_AND_RETURN(status, "Master start");

    // ── [D457CTRL] env-driven D457 camera-control hook (opt-in; see camera-controls-design.md) ──
    {
        const char* d457List = std::getenv("D457_CTRL_LIST");
        const char* d457Set  = std::getenv("D457_CTRL");
        const char* d457Get  = std::getenv("D457_CTRL_GET");
        if (d457List || d457Set || d457Get) {
            using namespace uddf::cdd::d457;
            // Prefer the pointer resolved in the config phase; fall back to resolving here (post-Init,
            // pre-Start) so one run reports which phase the framework actually allows.
            if (g_d457Ctrl == nullptr) {
                nvsipl::IInterfaceProvider* d457Prov = nullptr;
                SIPLStatus d457St = upMaster->GetModuleInterfaceProvider(0U, d457Prov);
                if (d457St == NVSIPL_STATUS_OK && d457Prov != nullptr) {
                    // Must match uddf::cdd::d457::uuid::ID457_CAMERA_CONTROL_ID (the authoritative
                    // definition) in ID457CameraControl.hpp -- update BOTH copies in this file together.
                    static const nvsipl::UUID kD457CtrlId(0xd457c711U, 0x9a2eU, 0x4f6bU, 0xb3d1U,
                                                          0x52U, 0x6fU, 0xa4U, 0x18U, 0x0cU, 0x37U);
                    g_d457Ctrl = reinterpret_cast<ID457CameraControl*>(d457Prov->GetInterface(kD457CtrlId));
                }
                std::cout << "[D457CTRL] post-start GetModuleInterfaceProvider(0) status=" << (int)d457St
                          << " ctrl=" << (void*)g_d457Ctrl << "\n";
            }
            ID457CameraControl* d457Ctrl = g_d457Ctrl;
            if (d457Ctrl == nullptr) {
                std::cout << "[D457CTRL] control interface unavailable\n";
            } else {
                auto d457Find = [&](const std::string& nm, CtrlId& out) -> bool {
                    ControlDesc d;
                    for (uint32_t i = 0; i < d457Ctrl->GetControlCount(); ++i) {
                        if (d457Ctrl->QueryControl(i, d) == CtrlResult::Ok && nm == d.name) { out = d.id; return true; }
                    }
                    return false;
                };
                if (d457List) {
                    ControlDesc d;
                    std::cout << "[D457CTRL] published controls (" << d457Ctrl->GetControlCount() << "):\n";
                    for (uint32_t i = 0; i < d457Ctrl->GetControlCount(); ++i) {
                        if (d457Ctrl->QueryControl(i, d) != CtrlResult::Ok) continue;
                        std::printf("  %-22s %-7s min=%-7lld max=%-9lld step=%-4lld def=%-8lld%s%s\n",
                            d.name, d.cam == Cam::Color ? "color" : "depthir",
                            (long long)d.min, (long long)d.max, (long long)d.step, (long long)d.def,
                            d.menu ? "  menu=" : "", d.menu ? d.menu : "");
                    }
                }
                if (d457Set) {
                    std::stringstream ss{std::string(d457Set)};
                    std::string tok;
                    while (std::getline(ss, tok, ';')) {
                        auto eq = tok.find('=');
                        if (eq == std::string::npos) continue;
                        std::string nm = tok.substr(0, eq), vs = tok.substr(eq + 1);
                        CtrlId cid;
                        if (!d457Find(nm, cid)) { std::cout << "[D457CTRL] set: unknown control '" << nm << "'\n"; continue; }
                        char* end = nullptr;
                        int64_t sv = (int64_t)std::strtoll(vs.c_str(), &end, 0);
                        if (end == vs.c_str() || *end != '\0') { std::cout << "[D457CTRL] set " << nm << ": bad value '" << vs << "'\n"; continue; }
                        CtrlResult r = d457Ctrl->SetControl(cid, sv);
                        std::cout << "[D457CTRL] set " << nm << "=" << vs << " -> rc=" << (int)r
                                  << (r == CtrlResult::Ok ? " (ok)" : " (FAIL)") << "\n";
                    }
                }
                if (d457Get) {
                    std::stringstream ss{std::string(d457Get)};
                    std::string tok;
                    while (std::getline(ss, tok, ',')) {
                        if (tok.empty()) continue;
                        CtrlId cid;
                        if (!d457Find(tok, cid)) { std::cout << "[D457CTRL] get: unknown control '" << tok << "'\n"; continue; }
                        int64_t v = 0;
                        CtrlResult r = d457Ctrl->GetControl(cid, v);
                        std::cout << "[D457CTRL] get " << tok << " -> "
                                  << (r == CtrlResult::Ok ? std::to_string(v) : ("rc=" + std::to_string((int)r))) << "\n";
                    }
                }
            }
        }
    }
    // ── [D457CTRL] end ──'''

# ── CNvSIPLMaster.hpp: GetModuleInterfaceProvider passthrough (mirrors GetPipelineInterfaceProvider) ─
MASTER_ANCHOR = '''        return m_pCamera->GetPipelineInterfaceProvider(uSensor, pipelineInterfaceProvider);
    }'''
MASTER_REPL = '''        return m_pCamera->GetPipelineInterfaceProvider(uSensor, pipelineInterfaceProvider);
    }

    // [D457CTRL] Public method to get the module interface provider (D457 camera-control hook).
    SIPLStatus GetModuleInterfaceProvider(uint32_t uSensor, IInterfaceProvider*& moduleInterfaceProvider) {
        if (m_pCamera == nullptr) {
            return NVSIPL_STATUS_NOT_INITIALIZED;
        }
        return m_pCamera->GetModuleInterfaceProvider(uSensor, moduleInterfaceProvider);
    }'''


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_DIR
    if not os.path.isdir(d):
        raise SystemExit(f"sample dir not found: {d}")
    print(f"patching nvsipl_camera D457 control hook in: {d}")
    patch_file(os.path.join(d, "CMakeLists.txt"), CMAKE_ANCHOR, CMAKE_REPL, skip_if="cdd_d457")
    patch_file(os.path.join(d, "CNvSIPLMaster.hpp"), MASTER_ANCHOR, MASTER_REPL, skip_if="GetModuleInterfaceProvider")
    patch_file(os.path.join(d, "main.cpp"), INC_ANCHOR, INC_REPL, skip_if="ID457CameraControl.hpp")
    patch_file(os.path.join(d, "main.cpp"), CFG_ANCHOR, CFG_REPL, skip_if="cfg-phase GetModuleInterfaceProvider")
    patch_file(os.path.join(d, "main.cpp"), START_ANCHOR, START_REPL, skip_if="[D457CTRL] end")
    print("done.")


if __name__ == "__main__":
    main()
