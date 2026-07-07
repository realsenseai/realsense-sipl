/*
 * Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
 *
 * traceon.cpp — LD_PRELOAD shim to enable SIPL traces in nvsipl_camera (diagnostics).
 *
 * Build (on the rig, against the SDK headers):
 *   g++ -shared -fPIC -O0 -o libtraceon.so traceon.cpp \
 *       -I/home/mic-742/sipl_full/usr/src/jetson_sipl_api/sipl/include \
 *       -L/usr/lib/aarch64-linux-gnu/nvidia -lnvsipl -lnvsipl_query -lnvsipl_devblk
 * Run (sudo strips LD_PRELOAD, so set it inside the root shell):
 *   sudo bash -c "LD_PRELOAD=$PWD/libtraceon.so nvsipl_camera -H -R -0 -1 -2 -m 0x0001 -c D457_Camera -v 4 -r 3"
 *
 * NOTE: INvSIPLTrace + INvSIPLQueryTrace have public headers (correct ABI). INvSIPLDeviceBlockTrace
 * has NO public header — the mirror below GUESSES its vtable matches INvSIPLTrace; it did NOT surface
 * devblk init logs this session, so the mirror is likely wrong. TODO: reverse the real
 * INvSIPLDeviceBlockTrace vtable from libnvsipl_devblk.so (or use the device-block error-info API)
 * to expose the CNvMCamera::Init (status 10) device-block failure.
 */
#include "NvSIPLTrace.hpp"
#include "query/include/NvSIPLQueryTrace.hpp"

namespace nvsipl {
class INvSIPLDeviceBlockTrace {            // mirror of INvSIPLTrace ABI (no public header) — VERIFY
public:
    typedef void (*TraceFuncPtr)(const char*, int);
    enum TraceLevel { LevelNone = 0, LevelError, LevelWarning, LevelInfo, LevelDebug };
    static INvSIPLDeviceBlockTrace* GetInstance(void);
    virtual void SetHook(TraceFuncPtr, unsigned int) = 0;
    virtual void SetLevel(TraceLevel) = 0;
    virtual void DisableLineInfo(void) = 0;
protected:
    virtual ~INvSIPLDeviceBlockTrace() = default;
};
}

__attribute__((constructor)) static void enable_trace(void)
{
    using namespace nvsipl;
    if (auto* t = INvSIPLTrace::GetInstance())            t->SetLevel(INvSIPLTrace::LevelDebug);
    if (auto* q = INvSIPLQueryTrace::GetInstance())       q->SetLevel(INvSIPLQueryTrace::LevelSpew);
    if (auto* d = INvSIPLDeviceBlockTrace::GetInstance()) d->SetLevel(INvSIPLDeviceBlockTrace::LevelDebug);
}
