/*
 * SPDX-FileCopyrightText: Copyright (c) 2025-2026 RealSense AI. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * d457_query.cpp — SIPL query for 2x D457 (2 cameras x depth+RGB+IR = 6 pipelines) on deser A.
 * Single module (enableMasks 0x0001, NO framework replication) -> 6 sensorInfo -> 6 pipelines.
 * VI capture VC = vcIdSrc: link0 = 0/1/2, link1 = 4/5/6 (distinct -> no collision). The deser HSL
 * (MAX967XXHsl mapping_A) remaps each camera's native VC0/1/2 to those output VCs (link1 via extended-VC).
 * deviceIndex encodes (link,stream): link=idx/3, stream=idx%3 (D457Sensor). link0 DS5@0x1a, link1@0x2a.
 * Resolution is data-driven via D457_WIDTH/HEIGHT/FPS.
 */
#include <cstdlib>
#include <cstdio>
#include <stdexcept>
#include <string>

// Built once, on first call. The JSON is derived from environment variables read at that
// moment; a function-local static is initialised exactly once and is thread-safe (C++11),
// which also means the returned pointer stays valid for the life of the process. Rebuilding
// per call would invalidate a pointer SIPL may still hold.
static std::string build_camera_config_json()
{
    // These values are spliced straight into the JSON that programs the capture pipeline, so they
    // are validated here rather than trusted: a malformed or out-of-range value would otherwise
    // produce silently-wrong hardware configuration (or malformed JSON) far from its cause.
    auto envNum = [](const char* k, const char* d, double lo, double hi, bool integral) {
        const char* raw = std::getenv(k);
        const std::string v((raw != nullptr && *raw != '\0') ? raw : d);
        size_t used = 0;
        double parsed = 0.0;
        try {
            parsed = std::stod(v, &used);
        } catch (const std::exception&) {
            used = 0;
        }
        if (used != v.size() || !(parsed >= lo) || !(parsed <= hi)
            || (integral && parsed != static_cast<double>(static_cast<long long>(parsed)))) {
            fprintf(stderr,
                "d457_query: %s=\"%s\" is not a valid %s in [%g, %g]; using the default %s\n",
                k, v.c_str(), integral ? "integer" : "number", lo, hi, d);
            return std::string(d);
        }
        return v;
    };
    const std::string W = envNum("D457_WIDTH", "1280", 64, 4096, true);
    const std::string H = envNum("D457_HEIGHT", "720", 64, 4096, true);
    const std::string F = envNum("D457_FPS", "30.0", 1, 120, false);

    // (i2cAddress, vcIdSrc/Dst, desc) per deviceIndex 0..5
    struct S { const char* addr; int vc; const char* d; };
    const S s[6] = {
        {"0x1a", 0, "link0 depth VC0"}, {"0x1a", 1, "link0 rgb VC1"},  {"0x1a", 2, "link0 ir VC2"},
        {"0x2a", 4, "link1 depth VC4"}, {"0x2a", 5, "link1 rgb VC5"},  {"0x2a", 6, "link1 ir VC6"},
    };
    std::string sens;
    for (int i = 0; i < 6; ++i) {
        if (i) sens += ",";
        sens +=
            "{\"name\":\"D457\",\"id\":" + std::to_string(i) +
            ",\"description\":\"" + s[i].d + "\",\"i2cAddress\":\"" + s[i].addr +
            "\",\"numContext\":1,\"isTriggerModeEnabled\":false,\"deviceIndex\":" + std::to_string(i) +
            ",\"virtualChannels\":[{\"vcIdSrc\":" + std::to_string(s[i].vc) +
            ",\"vcIdDst\":" + std::to_string(s[i].vc) +
            ",\"cfa\":\"rggb\",\"embeddedTopLines\":0,\"embeddedBottomLines\":0,\"inputFormat\":\"raw16\"" +
            ",\"width\":" + W + ",\"height\":" + H + ",\"fps\":" + F +
            ",\"isEmbeddedDataTypeEnabled\":false}]}";
    }
    std::string json =
        "[ { \"cameraConfigs\":[ { \"name\":\"D457_Camera\",\"moduleDriverName\":\"D457\",\"type\":\"GMSL\","
        "\"description\":\"2x D457 depth+rgb+ir\",\"serInfo\":{\"name\":\"MAX9295\",\"i2cAddress\":\"0x40\"},"
        "\"linkMode\":\"LINK_MODE_GMSL2_6GBPS\",\"fsyncMode\":\"osc_manual\","
        "\"mipiSettings\":{\"dphyRate\":2500000,\"phyMode\":\"dphy\",\"lanes\":4},"
        "\"sensorInfo\":[" + sens + "],\"cryptoConfigName\":\"\" } ] },"
        "{ \"platformTransportSettings\":[ { \"name\":\"advantech_mic742_thor\","
        "\"description\":\"Advantech MIC-742 Thor D457 GMSL CSI-A\",\"boardIdPrefix\":\"NVIDIA Jetson AGX Thor\","
        "\"enableMasks\":[\"0x0001\"],\"transportSettings\":[ { \"name\":\"transportSettings_d457_AB\","
        "\"type\":\"GMSL\",\"description\":\"CSI-AB\",\"csiPort\":\"csi-ab\","
        "\"deserInfo\":{\"name\":\"Max96712GmslDeserializer\",\"i2cAddress\":\"0x29\"},"
        "\"powerControlInfo\":{\"moduleInfo\":{\"name\":\"MAX20087\",\"i2cAddress\":\"0x28\"}},"
        "\"i2cDevice\":9,\"desI2CPort\":0,\"phyMode\":\"dphy\",\"groupInitProg\":true} ] } ] } ]";
    return json;
}

extern "C" const char* CNvMQuery_GetJsonData(void)
{
    static const std::string json = build_camera_config_json();
    return json.c_str();
}
