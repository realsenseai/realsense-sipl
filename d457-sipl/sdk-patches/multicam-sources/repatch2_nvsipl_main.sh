#!/bin/bash
SIPL=$HOME/sipl_full/usr/src/jetson_sipl_api/sipl
MAIN=$SIPL/samples/camera/main.cpp
cp ~/multicam_bak/main.cpp $MAIN   # restore clean
python3 - "$MAIN" <<'PY'
import sys
f=sys.argv[1]; s=open(f).read()
anchor='}, configVariant);'
i=s.find(anchor)
if i<0: print('ANCHOR NOT FOUND'); sys.exit(1)
ins='''

#ifdef ENABLE_CAMERA_HAL
    // D457 MULTI-CAMERA per-link offset: the query replicates the module per enabled link with identical
    // VCs (0/1/2) AND identical sensor i2c (0x1a), which (a) collide at the Tegra VI and (b) can't both be
    // reached on the shared bus. Offset BOTH by linkIndex: VC += link*4 (link0=0/1/2, link1=4/5/6, matches
    // deser HSL mapping_A) and sensor i2cAddress += link*0x10 (link0=0x1a, link1=0x2a, matches D457Sensor
    // per-link m_i2cAddr). Serializer addr is auto-assigned per link by the framework.
    for (auto& _m : oSensorSystemConfig.modules) {
        if (std::holds_alternative<sensorconfig::GmslModule>(_m.moduleType)) {
            auto& _gm = std::get<sensorconfig::GmslModule>(_m.moduleType);
            const uint32_t _link = _gm.linkIndex;
            // Output-VC placement must match the deser HSL: keep each link's streams inside ONE 4-VC
            // extended-VC msb group and <= VC7 (Tegra VI ignores VC>=8). SPL=sensors/link, LPG=links per
            // group = 4/SPL. offset = (link/LPG)*4 + (link%LPG)*SPL. SPL3: link0->0, link1->4 (=link*4).
            // SPL2: link0->0, link1->2, link2->4, link3->6 (packed VC0-7).
            const uint32_t _spl = static_cast<uint32_t>(_gm.sensorConfigs.size());
            const uint32_t _lpg = (_spl >= 1U && (4U / _spl) >= 1U) ? (4U / _spl) : 1U;
            const uint32_t _voff = (_link / _lpg) * 4U + (_link % _lpg) * _spl;
            if (_link != 0U && _link != UINT32_MAX) {
                for (auto& _scv : _gm.sensorConfigs) {
                    std::visit([&](auto& _sc){
                        _sc.address.i2cAddress = static_cast<uint16_t>(_sc.address.i2cAddress + _link * 0x10U);
                        if (_sc.address.virtualI2CAddress.has_value()) {
                            _sc.address.virtualI2CAddress =
                                static_cast<uint16_t>(_sc.address.virtualI2CAddress.value() + _link * 0x10U);
                        }
                        for (auto& _vc : _sc.vcInfoList) {
                            if (_vc.vcIdSrc != UINT32_MAX) { _vc.vcIdSrc += _voff; }
                            if (_vc.vcIdDst != UINT32_MAX) { _vc.vcIdDst += _voff; }
                        }
                    }, _scv);
                }
            }
        }
    }
#endif
'''
j=i+len(anchor)
open(f,'w').write(s[:j]+ins+s[j:])
print('main.cpp: patched (VC + i2c-addr per-link offset)')
PY
( cd $SIPL/samples/camera/build_dbg && timeout 200 make -j4 2>&1 | grep -iE 'error|Built target nvsipl_camera' | head -8 )
ls -la $SIPL/samples/camera/build_dbg/nvsipl_camera
