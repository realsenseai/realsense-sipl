/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#include <iostream>
#include <string>
#include <cstring>

#include <unistd.h>
#include <dlfcn.h>
#include <chrono>
#include <variant>

#include <iomanip>
#include <vector>

#include <memory>
#include <linux/if_ether.h>
#include "CUtils.hpp"
#include "NvSIPLCapStructs.h"  // Ensure pixel order definitions are available
#include "NvSIPLTrace.hpp"
#include "SIPLCoeCamera.hpp"


using namespace nvsipl;

// Timeout constants for fence operations
#define FENCE_FRAME_TIMEOUT_MS (100UL)

// Consumer type definitions for different output formats
enum class ConsumerType {
    RAW_CONSUMER,      // For ICP (raw) output
    YUV_CONSUMER,      // For ISP0/ISP1/ISP2 (YUV) output
    METADATA_CONSUMER  // For metadata processing
};

// Forward declarations for consumer classes
class CoeRawConsumer;
class CoeYuvConsumer;

// Enhanced consumer base class
class CoeConsumerBase {
public:
    virtual ~CoeConsumerBase() = default;
    virtual SIPLStatus ProcessBuffer(INvSIPLClient::INvSIPLNvMBuffer* pBuffer, uint32_t frameNum, uint32_t cameraModule) = 0;
    virtual ConsumerType GetType() const = 0;
    virtual const char* GetTypeName() const = 0;
    virtual void DisplayStatus() const = 0;

protected:
    uint32_t m_cameraModule = 0;
    uint32_t m_threadIndex = 0;
    std::string m_outputPrefix;
    bool m_dumpEnabled = false;
    uint32_t m_maxFrames = 0;
    uint32_t m_dumpedFrames = 0;
};

// RAW Consumer for ICP output
class CoeRawConsumer : public CoeConsumerBase {
private:
    // Dynamic RAW frame size calculation - no more hardcoded values!

        uint32_t GetDynamicRawBufferSize(INvSIPLClient::INvSIPLNvMBuffer* pBuffer)
    {
        if (!pBuffer) {
            LOG_ERR("[RAW_CONSUMER] Null buffer provided\n");
            return 0;
        }

        NvSciBufObj rawBufObj = pBuffer->GetNvSciBufImage();
        if (rawBufObj == nullptr) {
            LOG_ERR("[RAW_CONSUMER] Failed to get NvSciBufObj\n");
            return 0;
        }

        NvSciBufAttrList bufAttrList = nullptr;
        NvSciError sciErr = NvSciBufObjGetAttrList(rawBufObj, &bufAttrList);
        if (sciErr != NvSciError_Success) {
            LOG_ERR("[RAW_CONSUMER] Failed to get buffer attributes\n");
            return 0;
        }

        NvSciBufAttrKeyValuePair imgAttrs[] = {
            { NvSciBufImageAttrKey_PlanePitch, NULL, 0 },
            { NvSciBufImageAttrKey_PlaneHeight, NULL, 0 },
        };

        sciErr = NvSciBufAttrListGetAttrs(bufAttrList, imgAttrs, sizeof(imgAttrs) / sizeof(imgAttrs[0]));
        if (sciErr != NvSciError_Success) {
            LOG_ERR("[RAW_CONSUMER] Failed to get buffer attributes\n");
            return 0;
        }

        uint32_t pitch = (imgAttrs[0].len != 0) ? *(static_cast<const uint32_t*>(imgAttrs[0].value)) : 0;
        uint32_t height = (imgAttrs[1].len != 0) ? *(static_cast<const uint32_t*>(imgAttrs[1].value)) : 0;

        uint32_t bufferSize = pitch * height;

        LOG_DBG("[RAW_BUFFER_SIZE] Sensor %u - %u * %u = %u bytes\n",
                m_cameraModule, pitch, height, bufferSize);

        return bufferSize;
    }

public:
    CoeRawConsumer(uint32_t cameraModule, uint32_t threadIndex, bool bEnableRaw, uint32_t uNumWriteFrames)
    {
        m_cameraModule = cameraModule;
        m_threadIndex = threadIndex;
        m_outputPrefix = "raw";
        m_dumpEnabled = bEnableRaw;
        m_maxFrames = uNumWriteFrames;
        m_dumpedFrames = 0;
    }

    ConsumerType GetType() const override { return ConsumerType::RAW_CONSUMER; }
    const char* GetTypeName() const override { return "RAW"; }

    void DisplayStatus() const override {
        LOG_DBG("Processed: %u/%u frames, Dump: %s\n",
            m_dumpedFrames, m_maxFrames, (m_dumpEnabled ? "ON" : "OFF"));
    }

    SIPLStatus ProcessBuffer(INvSIPLClient::INvSIPLNvMBuffer* pBuffer, uint32_t frameNum, uint32_t cameraModule) override
    {
        if (!pBuffer) {
            LOG_ERR("[RAW_CONSUMER] Null buffer received\n");
            return NVSIPL_STATUS_BAD_ARGUMENT;
        }

        LOG_DBG("[RAW_CONSUMER] Processing ICP buffer (frame %u, sensor %u)\n", frameNum, cameraModule);

        // Check if we should save this frame
        if (m_dumpEnabled && m_dumpedFrames < m_maxFrames) {
            SIPLStatus status = SaveRawFrame(pBuffer, frameNum, cameraModule);
            if (status != NVSIPL_STATUS_OK) {
                LOG_ERR("[RAW_CONSUMER] Failed to save raw frame\n");
                return status;
            }
        }

        return NVSIPL_STATUS_OK;
    }

private:
    SIPLStatus SaveRawFrame(INvSIPLClient::INvSIPLNvMBuffer* pBuffer, uint32_t frameNum, uint32_t cameraModule)
    {
        // Calculate dynamic RAW buffer size instead of using hardcoded value
        uint32_t dynamicRawSize = GetDynamicRawBufferSize(pBuffer);
        if (dynamicRawSize == 0) {
            LOG_ERR("[RAW_CONSUMER] Failed to calculate dynamic RAW buffer size\n");
            return NVSIPL_STATUS_ERROR;
        }

        // Get direct CPU access to buffer
        NvSciError sciErr = NvSciError_Success;
        uint8_t *imageData = nullptr;

        sciErr = NvSciBufObjGetCpuPtr(pBuffer->GetNvSciBufImage(), (void**)&imageData);
        if (sciErr != NvSciError_Success) {
            LOG_ERR("[RAW_CONSUMER] Failed to get CPU pointer for raw data: %d\n", sciErr);
            return NVSIPL_STATUS_ERROR;
        }

        // Create filename for raw output
        char filename[256];
        snprintf(filename, sizeof(filename), "/tmp/coe_sensor%u_raw_frame_%u.raw",
                 cameraModule, frameNum);

        // Write raw data to file using dynamically calculated size
        SIPLStatus status = WriteRawDataToFile(filename, imageData, dynamicRawSize);
        if (status == NVSIPL_STATUS_OK) {
            m_dumpedFrames++;
            LOG_INFO("Saved frame %u -> %s (%u bytes) [%u/%u]\n",
                     frameNum, filename, dynamicRawSize, m_dumpedFrames, m_maxFrames);
        }

        return status;
    }

    SIPLStatus WriteRawDataToFile(const char* filename, const uint8_t* data, uint32_t size)
    {
        FILE* pFile = fopen(filename, "wb");
        if (!pFile) {
            LOG_ERR("[RAW_CONSUMER] Failed to create file: %s\n", filename);
            return NVSIPL_STATUS_ERROR;
        }

        size_t written = fwrite(data, 1, size, pFile);
        fclose(pFile);

        if (written != size) {
            LOG_ERR("[RAW_CONSUMER] Incomplete write to %s (wrote %u/%u bytes)\n",
            filename, written, size);
            return NVSIPL_STATUS_ERROR;
        }

        return NVSIPL_STATUS_OK;
    }
};

// YUV Consumer for ISP0/ISP1/ISP2 outputs
class CoeYuvConsumer : public CoeConsumerBase {
private:
    INvSIPLClient::ConsumerDesc::OutputType m_outputType;
    uint32_t m_outputIndex;
    NvSciSyncCpuWaitContext m_cpuWaitContext;

public:
    CoeYuvConsumer(uint32_t cameraModule, uint32_t threadIndex,
                   INvSIPLClient::ConsumerDesc::OutputType outputType,
                   NvSciSyncCpuWaitContext cpuWaitContext,
                   bool bDisableISP,
                   uint32_t uNumWriteFrames)
    {
        m_cameraModule = cameraModule;
        m_threadIndex = threadIndex;
        m_outputType = outputType;
        m_cpuWaitContext = cpuWaitContext;
        m_dumpEnabled = !bDisableISP;
        m_maxFrames = uNumWriteFrames;
        m_dumpedFrames = 0;

        // Determine output index and prefix
        if (outputType == INvSIPLClient::ConsumerDesc::OutputType::ISP0) {
            m_outputIndex = 0;
            m_outputPrefix = "ISP0";
        } else if (outputType == INvSIPLClient::ConsumerDesc::OutputType::ISP1) {
            m_outputIndex = 1;
            m_outputPrefix = "ISP1";
        } else if (outputType == INvSIPLClient::ConsumerDesc::OutputType::ISP2) {
            m_outputIndex = 2;
            m_outputPrefix = "ISP2";
        } else {
            m_outputIndex = 99;
            m_outputPrefix = "UNKNOWN";
        }
    }

    ConsumerType GetType() const override { return ConsumerType::YUV_CONSUMER; }
    const char* GetTypeName() const override { return m_outputPrefix.c_str(); }

    void DisplayStatus() const override {
        LOG_DBG("Processed: %u/%u frames, Dump: %s\n", m_dumpedFrames, m_maxFrames, (m_dumpEnabled ? "ON" : "OFF"));
    }

    SIPLStatus ProcessBuffer(INvSIPLClient::INvSIPLNvMBuffer* pBuffer, uint32_t frameNum, uint32_t cameraModule) override
    {
        if (!pBuffer) {
            LOG_ERR("[YUV_CONSUMER] Null buffer received\n");
            return NVSIPL_STATUS_BAD_ARGUMENT;
        }

        LOG_DBG("[YUV_CONSUMER] Processing %s buffer (frame %u, sensor %u)\n",
                        m_outputPrefix.c_str(), frameNum, cameraModule);

        // Check if we should save this frame
        if (m_dumpEnabled && m_dumpedFrames < m_maxFrames) {
            SIPLStatus status = SaveYuvFrame(pBuffer, frameNum, cameraModule);
            if (status != NVSIPL_STATUS_OK) {
                LOG_ERR("[YUV_CONSUMER] Failed to save YUV frame from %s\n", m_outputPrefix.c_str());
                return status;
            }
        }

        return NVSIPL_STATUS_OK;
    }

private:
    SIPLStatus SaveYuvFrame(INvSIPLClient::INvSIPLNvMBuffer* pBuffer, uint32_t frameNum, uint32_t cameraModule)
    {
        // Wait on EOF fence before processing
        NvSciSyncFence fence = NvSciSyncFenceInitializer;
        SIPLStatus status = pBuffer->GetEOFNvSciSyncFence(&fence);
        if (status != NVSIPL_STATUS_OK) {
            LOG_ERR("[YUV_CONSUMER] GetEOFNvSciSyncFence failed: %u\n", static_cast<uint32_t>(status));
            return status;
        }

        NvSciError sciErr = NvSciSyncFenceWait(&fence, m_cpuWaitContext, FENCE_FRAME_TIMEOUT_MS * 2000UL);
        if (sciErr != NvSciError_Success) {
            LOG_ERR("[YUV_CONSUMER] Fence wait failed: %d\n", sciErr);
            NvSciSyncFenceClear(&fence);
            return NVSIPL_STATUS_ERROR;
        }
        NvSciSyncFenceClear(&fence);

        // Get buffer attributes
        NvSciBufObj bufPtr = pBuffer->GetNvSciBufImage();
        if (!bufPtr) {
            LOG_ERR("[YUV_CONSUMER] Failed to get NvSciBuf image object\n");
            return NVSIPL_STATUS_ERROR;
        }

        // Extract YUV data using proper buffer analysis
        SIPLStatus extractStatus = ExtractAndSaveYuvData(bufPtr, frameNum, cameraModule);
        if (extractStatus == NVSIPL_STATUS_OK) {
            m_dumpedFrames++;
        }
        return extractStatus;
    }

    SIPLStatus ExtractAndSaveYuvData(NvSciBufObj bufPtr, uint32_t frameNum, uint32_t cameraModule)
    {
        // Create filename for YUV output
        char filename[256];
        snprintf(filename, sizeof(filename), "/tmp/coe_sensor%u_%s_frame_%u.yuv",
                 cameraModule, m_outputPrefix.c_str(), frameNum);

        // For now, we'll create a placeholder YUV file
        // In a full implementation, this would extract actual YUV pixel data
        FILE* pFile = fopen(filename, "wb");
        if (!pFile) {
            LOG_ERR("[YUV_CONSUMER] Failed to create file: %s\n", filename);
            return NVSIPL_STATUS_ERROR;
        }

        // Write placeholder YUV data header
        const char* yuvHeader = "YUV_PLACEHOLDER_DATA";
        size_t headerSize = strlen(yuvHeader);
        size_t written = fwrite(yuvHeader, 1, headerSize, pFile);
        fclose(pFile);

        if (written != headerSize) {
            LOG_ERR("[YUV_CONSUMER] Failed to write YUV placeholder to %s\n", filename);
            return NVSIPL_STATUS_ERROR;
        }

        LOG_INFO("Saved %s frame %u -> %s (placeholder YUV data) [%u/%u]\n",
                 m_outputPrefix.c_str(), frameNum, filename, (m_dumpedFrames + 1), m_maxFrames);

        return NVSIPL_STATUS_OK;
    }

    // Get processing statistics
    uint32_t GetProcessedFrames() const { return m_dumpedFrames; }
    uint32_t GetMaxFrames() const { return m_maxFrames; }
    bool IsDumpEnabled() const { return m_dumpEnabled; }
};

// Consumer Factory for creating consumers with proper context
class CoeConsumerFactory {
public:
    static std::unique_ptr<CoeConsumerBase> CreateConsumer(
        uint32_t threadIndex,
        uint32_t cameraModule,
        NvSciSyncCpuWaitContext cpuWaitContext,
        bool bEnableRaw,
        bool bDisableISP0,
        bool bDisableISP1,
        bool bDisableISP2,
        uint32_t uNumWriteFrames)
    {
        if (threadIndex == SIPLCoeCamera::THREAD_INDEX_ICP) {
            // Create RAW consumer for ICP output
            return std::make_unique<CoeRawConsumer>(cameraModule, threadIndex,
                                                    bEnableRaw, uNumWriteFrames);
        } else if (threadIndex == SIPLCoeCamera::THREAD_INDEX_ISP0) {
            // Create YUV consumer for ISP0 output
            auto outputType = INvSIPLClient::ConsumerDesc::OutputType::ISP0;
            return std::make_unique<CoeYuvConsumer>(cameraModule, threadIndex, outputType,
                                                    cpuWaitContext, bDisableISP0, uNumWriteFrames);
        } else if (threadIndex == SIPLCoeCamera::THREAD_INDEX_ISP1) {
            // Create YUV consumer for ISP1 output
            auto outputType = INvSIPLClient::ConsumerDesc::OutputType::ISP1;
            return std::make_unique<CoeYuvConsumer>(cameraModule, threadIndex, outputType,
                                                    cpuWaitContext, bDisableISP1, uNumWriteFrames);
        } else if (threadIndex == SIPLCoeCamera::THREAD_INDEX_ISP2) {
            // Create YUV consumer for ISP2 output
            auto outputType = INvSIPLClient::ConsumerDesc::OutputType::ISP2;
            return std::make_unique<CoeYuvConsumer>(cameraModule, threadIndex, outputType,
                                                    cpuWaitContext, bDisableISP2, uNumWriteFrames);
        }

        return nullptr; // Unknown thread index
    }
};

// Global COE configuration
CameraSystemConfig g_coeSystemConfig;

NvSIPLPipelineConfiguration g_coePipelineCfg = {
    .captureOutputRequested = true,
    .isp0OutputRequested = true,
    .isp1OutputRequested = false,
    .isp2OutputRequested = false,
    .disableSubframe = true,
    .bufferCfg = {
        .maxCaptureBufferCount = 4U,  // Minimum required for COE mode
        .maxIsp0BufferCount = 64U,    // Standard ISP buffer count
        .maxIsp1BufferCount = 64U,    // Standard ISP buffer count
        .maxIsp2BufferCount = 64U,    // Standard ISP buffer count
    }
};

// Static thread function helpers
static void CoeImageThreadFunc(SIPLCoeCamera *test, uint32_t cameraModule, uint32_t threadIndex)
{
    test->CoeImageThread(cameraModule, threadIndex);
}

static void CoeEventThreadFunc(SIPLCoeCamera *test, uint32_t cameraModule, uint32_t threadIndex)
{
    test->CoeEventThread(cameraModule, threadIndex);
}

static void CoeCpuSignalThreadFunc(SIPLCoeCamera *test, uint32_t cameraModule, uint32_t threadIndex)
{
    test->CoeCpuSignalThread(cameraModule, threadIndex);
}

SIPLStatus SIPLCoeCamera::SetUp()
{
    // Initialize basic test setup for COE
    m_bInitialized = false;
    m_ConfigureAllThreadFailed = false;
    m_exitAllThreads = false;
    // Initialize thread states
    for (uint32_t i = 0; i < MAX_COE_MODULES_PER_PLATFORM; ++i) {
        for (uint32_t j = 0; j < THREAD_INDEX_COUNT; ++j) {
            m_threadReady[i][j] = false;
            m_queueCounts[i][j] = 0;
        }
    }
    // Initialize NvSci modules
    NvSciError sciErr = NvSciBufModuleOpen(&m_sciBufModule);
    if (sciErr != NvSciError_Success) {
        LOG_ERR("[SETUP]  NvSciBufModuleOpen failed: %d\n", sciErr);
        return NVSIPL_STATUS_ERROR;
    }
    sciErr = NvSciSyncModuleOpen(&m_sciSyncModule);
    if (sciErr != NvSciError_Success) {
        LOG_ERR("[SETUP]  NvSciSyncModuleOpen failed: %d\n", sciErr);
        return NVSIPL_STATUS_ERROR;
    }

    // Allocate main class CPU wait context for ISP buffer dumping
    LOG_DBG("Allocating main class CPU wait context for ISP buffer operations...\n");
    sciErr = NvSciSyncCpuWaitContextAlloc(m_sciSyncModule, &m_cpuWaitContext);
    if (sciErr != NvSciError_Success) {
        LOG_ERR("[SETUP]  Failed to allocate main class CPU wait context: %d\n", sciErr);
        return NVSIPL_STATUS_ERROR;
    }

    LOG_DBG("Main class CPU wait context allocated: %p\n", (void*)m_cpuWaitContext);
    // Setup COE camera with new COE Query APIs
    auto status = SetupCoeCamera();
    if (status != NVSIPL_STATUS_OK) {
        LOG_ERR("Failed to setup COE camera: %d\n", status);
        return NVSIPL_STATUS_ERROR;
    }
    m_bInitialized = true;

    LOG_INFO("COE Test setup completed successfully\n");
    return NVSIPL_STATUS_OK;
}

SIPLStatus SIPLCoeCamera::TearDown()
{
    if (m_bInitialized) {
        LOG_INFO("Tearing down COE Integration Test\n");

        // Check if threads are already stopped (by CoeDeinit())
        if (!m_exitAllThreads) {
            ExitAllCoeThreads();
        }

        // Cleanup camera
        CleanupCoeCamera();

        // Cleanup NvSci modules
        if (m_sciBufModule) {
            NvSciBufModuleClose(m_sciBufModule);
            m_sciBufModule = nullptr;
        }

        // Cleanup all allocated sync objects
        if (!m_allocatedSyncObjects.empty()) {
            LOG_INFO("Freeing %d allocated sync objects...\n", m_allocatedSyncObjects.size());
            for (auto& syncObj : m_allocatedSyncObjects) {
                if (syncObj != nullptr) {
                    NvSciSyncObjFree(syncObj);
                    syncObj = nullptr;
                }
            }
            m_allocatedSyncObjects.clear();
            LOG_INFO("All sync objects freed\n");
        }

        // Cleanup main class CPU wait context
        if (m_cpuWaitContext != nullptr) {
            LOG_INFO("Freeing main class CPU wait context...\n");
            NvSciSyncCpuWaitContextFree(m_cpuWaitContext);
            m_cpuWaitContext = nullptr;
            LOG_INFO("Main class CPU wait context freed\n");
        }

        if (m_sciSyncModule) {
            NvSciSyncModuleClose(m_sciSyncModule);
            m_sciSyncModule = nullptr;
        }

        m_bInitialized = false;
    }
    return NVSIPL_STATUS_OK;
}

SIPLStatus SIPLCoeCamera::SetupCoeCamera()
{
    m_upQuery = INvSIPLCameraQuery::GetInstance();
    if (!m_upQuery) {
        LOG_ERR("SIPL Query instance not available\n");
        return NVSIPL_STATUS_ERROR;
    }

    // Get COE camera system configuration using SIPL Query APIs
    SIPLStatus result = GetCoeSystemConfigFromQuery();
    if (result != NVSIPL_STATUS_OK) {
        LOG_ERR("Failed to get COE camera system config: %d\n", result);
        return result;
    }

    m_upCamera = INvSIPLCamera::GetInstance();
    if (!m_upCamera) {
        LOG_ERR("Failed to get INvSIPLCamera instance\n");
        return NVSIPL_STATUS_ERROR;
    }

    LOG_INFO("=== SIPL : SetupCoeCamera COMPLETED ===\n");
    return NVSIPL_STATUS_OK;
}

void SIPLCoeCamera::CleanupCoeCamera()
{
    m_upCamera.reset();
    m_upQuery.reset();
    LOG_INFO("COE camera cleanup completed\n");
}

// Complete CoeRegisterImages implementation like CameraRegisterImages() in SIPL
SIPLStatus SIPLCoeCamera::CoeRegisterImagesHelper(INvSIPLClient::ConsumerDesc::OutputType const outType,
                                               uint32_t const uSensor,
                                               uint32_t const index,
                                               uint32_t const numObjects,
                                               uint32_t const col)
{
    LOG_DBG("Registering %u images for sensor %u\n", numObjects, uSensor);
    LOG_DBG("Output type: %d module index: %u\n", static_cast<int>(outType), index);

    // Create buffer object manager for this module/output type combination
    m_bufObjManager[index][col] = std::make_unique<CoeBufObjManager>(m_sciBufModule);
    if (m_bufObjManager[index][col] == nullptr) {
        LOG_ERR("Failed to create CoeBufObjManager\n");
        return NVSIPL_STATUS_ERROR;
    }

    // Allocate buffer objects using the manager
    SIPLStatus status = m_bufObjManager[index][col]->AllocateBufObjs(m_upCamera.get(),
                                                                     uSensor,
                                                                     outType,
                                                                     numObjects);
    if (status != NVSIPL_STATUS_OK) {
        LOG_ERR("AllocateBufObjs failed with status: %u\n", static_cast<uint32_t>(status));
        return status;
    }

    LOG_DBG("Allocated %u buffer objects\n", m_bufObjManager[index][col]->m_sciBufObjs.size());

    // Register the allocated buffers with the camera
    LOG_INFO("This call can take time to complete\n");
    auto start_time = std::chrono::steady_clock::now();
    status = m_upCamera->RegisterImages(uSensor,
                                        outType,
                                        m_bufObjManager[index][col]->m_sciBufObjs);
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    LOG_DBG("RegisterImages completed in %u ms\n", duration.count());
    if (status != NVSIPL_STATUS_OK) {
        LOG_ERR("RegisterImages failed with status: %u\n", static_cast<uint32_t>(status));
        return status;
    }

    return NVSIPL_STATUS_OK;
}

SIPLStatus SIPLCoeCamera::CoeRegisterAutoControlPlugin(const PluginType type)
{
    // Check if any ISP outputs are enabled
    bool hasISPOutputs = false;
    for (auto& moduleInfo : m_coeModules) {
        uint32_t index = moduleInfo.index;
        if (m_queues[index].isp0CompletionQueue != nullptr ||
            m_queues[index].isp1CompletionQueue != nullptr ||
            m_queues[index].isp2CompletionQueue != nullptr) {
            hasISPOutputs = true;
            break;
        }
    }
    if (!hasISPOutputs) {
        LOG_INFO("No ISP outputs enabled (ICP-only configuration)\n");
        return NVSIPL_STATUS_OK;
    }

    SIPLStatus status = NVSIPL_STATUS_OK;
    struct CoeModuleData *module = CoeFirstModule();
    int moduleCount = 0;
    int totalModules = static_cast<int>(m_coeModules.size());
    std::set<uint32_t> registeredSensors;

    LOG_DBG("Total modules in m_coeModules: %d\n", totalModules);

    while (module != nullptr) {
        moduleCount++;
        uint32_t uSensor = module->sensorId;
        LOG_DBG("Processing Module #%d: %s (Sensor ID: %u)\n", moduleCount, module->name.c_str(), uSensor);
        if (registeredSensors.find(uSensor) != registeredSensors.end()) {
            LOG_WARN("Sensor %u already registered, skipping\n", uSensor);
            module = CoeNextModule();
            continue;
        }
        LOG_DBG("Loading NITO file for sensor: %s\n", module->sensorName.c_str());
        std::vector<uint8_t> blob;
        SIPLStatus loadStatus = LoadNITOFile(m_cmdline.sNitoFolderPath, module->sensorName, blob);
        if (loadStatus != NVSIPL_STATUS_OK) {
            LOG_ERR("LoadNITOFile failed for module: %s with status: %u\n", module->name, static_cast<uint32_t>(loadStatus));
            return loadStatus;
        }
        LOG_DBG("NITO file loaded successfully for module: %s (size: %u bytes)\n", module->name, blob.size());
        // Validate blob is not empty
        if (blob.empty()) {
            LOG_ERR("NITO blob is empty for module: %s\n", module->name);
            return NVSIPL_STATUS_ERROR;
        }
        LOG_DBG("Registering %s for sensor: %u\n", (type == NV_PLUGIN ? "NV_PLUGIN" : "CUSTOM_PLUGIN"), uSensor);
        LOG_DBG("Parameters: sensor=%u, type=%u, blob_size=%u\n", uSensor, static_cast<uint32_t>(type), blob.size());
        ISiplControlAuto* autoControl = nullptr; // For NV_PLUGIN, this should be nullptr
        status = m_upCamera->RegisterAutoControlPlugin(uSensor, type, autoControl, blob);
        if (status != NVSIPL_STATUS_OK) {
            LOG_ERR("RegisterAutoControlPlugin failed for sensor: %u with status: %u\n",
                uSensor, static_cast<uint32_t>(status));
            return status;
        }
        LOG_INFO("Auto control plugin registered successfully for sensor: %u\n", uSensor);
        registeredSensors.insert(uSensor);
        module = CoeNextModule();
    }
    LOG_INFO("Processed %d modules total\n", moduleCount);
    LOG_INFO("Registered auto control for %u unique sensors\n", registeredSensors.size());
    return NVSIPL_STATUS_OK;
}

SIPLStatus SIPLCoeCamera::CoeConfigAllThreads()
{
    LOG_DBG("Configuring all COE threads\n");
    struct CoeModuleData *module = CoeFirstModule();
    while (module != nullptr) {
        uint32_t index = module->index;
        // Configure ICP thread
        if (m_queues[index].captureCompletionQueue != nullptr) {
            m_threadData[index][THREAD_INDEX_ICP].threadName = "COE_ICP";
            m_threadData[index][THREAD_INDEX_ICP].imageQueue = m_queues[index].captureCompletionQueue;
            m_threadData[index][THREAD_INDEX_ICP].eventQueue = nullptr;
            m_threads[index][THREAD_INDEX_ICP] =
                std::thread(&CoeImageThreadFunc, this, index, THREAD_INDEX_ICP);
        }
        // Configure ISP0 thread
        if (m_queues[index].isp0CompletionQueue != nullptr) {
            m_threadData[index][THREAD_INDEX_ISP0].threadName = "COE_ISP0";
            m_threadData[index][THREAD_INDEX_ISP0].imageQueue = m_queues[index].isp0CompletionQueue;
            m_threadData[index][THREAD_INDEX_ISP0].eventQueue = nullptr;
            m_threads[index][THREAD_INDEX_ISP0] =
                std::thread(&CoeImageThreadFunc, this, index, THREAD_INDEX_ISP0);
        }
        // Configure ISP1 thread
        if (m_queues[index].isp1CompletionQueue != nullptr) {
            m_threadData[index][THREAD_INDEX_ISP1].threadName = "COE_ISP1";
            m_threadData[index][THREAD_INDEX_ISP1].imageQueue = m_queues[index].isp1CompletionQueue;
            m_threadData[index][THREAD_INDEX_ISP1].eventQueue = nullptr;
            m_threads[index][THREAD_INDEX_ISP1] =
                std::thread(&CoeImageThreadFunc, this, index, THREAD_INDEX_ISP1);
        }
        // Configure ISP2 thread
        if (m_queues[index].isp2CompletionQueue != nullptr) {
            m_threadData[index][THREAD_INDEX_ISP2].threadName = "COE_ISP2";
            m_threadData[index][THREAD_INDEX_ISP2].imageQueue = m_queues[index].isp2CompletionQueue;
            m_threadData[index][THREAD_INDEX_ISP2].eventQueue = nullptr;
            m_threads[index][THREAD_INDEX_ISP2] =
                std::thread(&CoeImageThreadFunc, this, index, THREAD_INDEX_ISP2);
        }
        // Configure Event thread
        m_threadData[index][THREAD_INDEX_EVENT].threadName = "COE_EVENT";
        m_threadData[index][THREAD_INDEX_EVENT].imageQueue = nullptr;
        m_threadData[index][THREAD_INDEX_EVENT].eventQueue = m_queues[index].notificationQueue;
        m_threads[index][THREAD_INDEX_EVENT] =
            std::thread(&CoeEventThreadFunc, this, index, THREAD_INDEX_EVENT);
        // Configure CPU Signal thread
        m_threadData[index][THREAD_INDEX_CPUSIGNAL].threadName = "COE_CPUSIGNAL";
        m_threadData[index][THREAD_INDEX_CPUSIGNAL].imageQueue = nullptr;
        m_threadData[index][THREAD_INDEX_CPUSIGNAL].eventQueue = nullptr;
        m_threads[index][THREAD_INDEX_CPUSIGNAL] =
            std::thread(&CoeCpuSignalThreadFunc, this, index, THREAD_INDEX_CPUSIGNAL);
        module = CoeNextModule();
    }
    // Wait for all threads to be ready
    usleep(100000); // 100ms
    if (!AllCoeThreadsReady()) {
        m_ConfigureAllThreadFailed = true;
        LOG_ERR("COE threads failed to start\n");
        return NVSIPL_STATUS_ERROR;
    }
    LOG_DBG("COE thread configuration completed successfully\n");
    return NVSIPL_STATUS_OK;
}

bool SIPLCoeCamera::AllCoeThreadsReady()
{
    struct CoeModuleData *module = CoeFirstModule();
    bool ready = true;
    while (module != nullptr) {
        uint32_t index = module->index;
        for (uint32_t j = 0; j < THREAD_INDEX_COUNT; ++j) {
            if (m_threadData[index][j].imageQueue != nullptr ||
                m_threadData[index][j].eventQueue != nullptr) {
                ready = ready && m_threadReady[index][j].load();
            }
        }
        module = CoeNextModule();
    }
    return ready;
}

void SIPLCoeCamera::ExitAllCoeThreads()
{
    LOG_INFO("Exiting all COE threads\n");
    m_exitAllThreads = true;
    for (uint32_t i = 0; i < MAX_COE_MODULES_PER_PLATFORM; ++i) {
        for (uint32_t j = 0; j < THREAD_INDEX_COUNT; ++j) {
            if (m_threadReady[i][j].load()) {
                m_threads[i][j].join();
                m_threadReady[i][j] = false;
            }
        }
    }
}

// RAW surface dumping (kept for backward compatibility)
__attribute__((unused))
static void dumpRawSurfaceHelper(const char *filepath,
                              uint32_t frameID, const void *pix_data, size_t data_size)
{
    if (filepath == NULL) {
        return;
    }
    if (pix_data == NULL) {
        fprintf(stderr, "Warning: surface has null pixel data. Skipping RAW dump.\n");
        return;
    }
    if (data_size == 0) {
        fprintf(stderr, "Warning: surface has zero size. Skipping RAW dump.\n");
        return;
    }
    // Use the filename directly (already includes frame number)
    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to open '%s' for RAW dump writing\n", filepath);
        perror("fopen");
        return;
    }
    if (fwrite(pix_data, 1, data_size, fp) != data_size) {
        fprintf(stderr, "Failed to write RAW data to '%s'\n", filepath);
        perror("fwrite");
    }
    fclose(fp);
}
// Thread function implementations
void SIPLCoeCamera::CoeImageThread(uint32_t const cameraModule, uint32_t const threadIndex)
{
    LOG_DBG("COE Image Thread %u for module %u started\n", threadIndex, cameraModule);

    // Create appropriate consumer using factory
    std::unique_ptr<CoeConsumerBase> consumer = CoeConsumerFactory::CreateConsumer(
        threadIndex,
        cameraModule,
        m_cpuWaitContext,
        m_cmdline.bEnableRaw,
        m_cmdline.bDisableISP0,
        m_cmdline.bDisableISP1,
        m_cmdline.bDisableISP2,
        m_cmdline.uNumWriteFrames
    );

    if (!consumer) {
        LOG_ERR("Failed to create consumer for thread index: %u\n", threadIndex);
        return;
    }

    LOG_DBG("[CONSUMER_SETUP] Created %s consumer for thread %u, module %u\n",
                    consumer->GetTypeName(), threadIndex, cameraModule);

    SIPLStatus status = NVSIPL_STATUS_OK;
    INvSIPLClient::INvSIPLBuffer *pBuffer = nullptr;
    ThreadData *threadData = &m_threadData[cameraModule][threadIndex];
    m_threadReady[cameraModule][threadIndex] = true;

    while (!m_exitAllThreads && (status != NVSIPL_STATUS_EOF)) {
        if (threadData->imageQueue) {
            status = threadData->imageQueue->Get(pBuffer, 1000000); // 1 second timeout
            if (status == NVSIPL_STATUS_OK && pBuffer != nullptr) {
                m_queueCounts[cameraModule][threadIndex]++;

                // Cast to NvM buffer for processing
                INvSIPLClient::INvSIPLNvMBuffer *pNvMBuffer =
                    dynamic_cast<INvSIPLClient::INvSIPLNvMBuffer *>(pBuffer);

                if (pNvMBuffer != nullptr) {
                    uint32_t frameNumber = m_queueCounts[cameraModule][threadIndex];

                    LOG_DBG("[%s_CONSUMER] Processing buffer (frame %u, sensor %u, type %s)\n",
                                    consumer->GetTypeName(), frameNumber, cameraModule,
                                    (consumer->GetType() == ConsumerType::RAW_CONSUMER ? "RAW" : "YUV"));
                    if (m_cmdline.bEnableRaw && consumer->GetType() == ConsumerType::RAW_CONSUMER) {
                        SIPLStatus processStatus = consumer->ProcessBuffer(pNvMBuffer, frameNumber, cameraModule);
                        if (processStatus != NVSIPL_STATUS_OK) {
                            LOG_ERR("[RAW_CONSUMER] Buffer processing failed with status: %u\n",
                                    static_cast<uint32_t>(processStatus));
                        }
                    } else if (!m_cmdline.bDisableISP0 && threadIndex == THREAD_INDEX_ISP0) {
                        SIPLStatus status = WriteISPBufferToFile(pNvMBuffer, frameNumber, cameraModule, threadIndex);
                        if (status != NVSIPL_STATUS_OK) {
                            LOG_ERR("[YUV_CONSUMER] Failed to write ISP buffer to file\n");
                        }
                    } else if (!m_cmdline.bDisableISP1 && threadIndex == THREAD_INDEX_ISP1) {
                        // Use the appropriate consumer to process the buffer
                        SIPLStatus status = WriteISPBufferToFile(pNvMBuffer, frameNumber, cameraModule, threadIndex);
                        if (status != NVSIPL_STATUS_OK) {
                            LOG_ERR("[YUV_CONSUMER] Failed to write ISP buffer to file\n");
                        }
                    } else if (!m_cmdline.bDisableISP2 && threadIndex == THREAD_INDEX_ISP2) {
                        // Use the appropriate consumer to process the buffer
                        SIPLStatus status = WriteISPBufferToFile(pNvMBuffer, frameNumber, cameraModule, threadIndex);
                        if (status != NVSIPL_STATUS_OK) {
                            LOG_ERR("[YUV_CONSUMER] Failed to write ISP buffer to file\n");
                        }
                    }
                } else {
                    LOG_ERR("[%s_CONSUMER] Failed to cast to NvM buffer\n", consumer->GetTypeName());
                }

                // Return buffer to SIPL (following SIPL OnFrameAvailable pattern)
                SIPLStatus releaseStatus = pBuffer->Release();
                if (releaseStatus != NVSIPL_STATUS_OK) {
                    LOG_ERR("[%s_CONSUMER] Buffer release failed with status: %u\n",
                            consumer->GetTypeName(), static_cast<uint32_t>(releaseStatus));
                }
                pBuffer = nullptr;
            } else if (status != NVSIPL_STATUS_TIMED_OUT) {
                LOG_ERR("[%s_CONSUMER] Failed to get buffer, status: %u\n",
                    consumer->GetTypeName(), static_cast<uint32_t>(status));
            }
        } else {
            usleep(10000); // 10ms
        }
    }

    // Display final consumer statistics
    consumer->DisplayStatus();

    LOG_DBG("COE Image Thread %u (%s consumer) for module %u exited - processed %u total frames\n",
            threadIndex, consumer->GetTypeName(), cameraModule, m_queueCounts[cameraModule][threadIndex]);
}

void SIPLCoeCamera::OnEvent(NvSIPLPipelineNotifier::NotificationData &oNotificationData)
{
    LOG_DBG("COE Event received: %u\n", static_cast<int>(oNotificationData.eNotifType));
    switch (oNotificationData.eNotifType) {
        case NvSIPLPipelineNotifier::NOTIF_INFO_ICP_PROCESSING_DONE:
            LOG_INFO("Pipeline: %u, NOTIF_INFO_ICP_PROCESSING_DONE\n", oNotificationData.uIndex);
            m_uNumFrameCaptured++;
            break;
        case NvSIPLPipelineNotifier::NOTIF_INFO_ISP_PROCESSING_DONE:
            LOG_INFO("Pipeline: %u, NOTIF_INFO_ISP_PROCESSING_DONE\n", oNotificationData.uIndex);
            break;
        case NvSIPLPipelineNotifier::NOTIF_INFO_ACP_PROCESSING_DONE:
            LOG_INFO("Pipeline: %u, NOTIF_INFO_ACP_PROCESSING_DONE\n", oNotificationData.uIndex);
            break;
        case NvSIPLPipelineNotifier::NOTIF_INFO_ICP_AUTH_SUCCESS:
            LOG_INFO("Pipeline: %u, ICP_AUTH_SUCCESS frame=%lu\n",
                    oNotificationData.uIndex, oNotificationData.frameSeqNumber);
            break;
        case NvSIPLPipelineNotifier::NOTIF_INFO_CDI_PROCESSING_DONE:
            LOG_ERR("Pipeline: %u, NOTIF_INFO_CDI_PROCESSING_DONE\n", oNotificationData.uIndex);
            break;
        case NvSIPLPipelineNotifier::NOTIF_WARN_ICP_FRAME_DROP:
            LOG_ERR("Pipeline: %u, NOTIF_WARN_ICP_FRAME_DROP\n", oNotificationData.uIndex);
            m_uNumFrameDrops++;
            break;
        case NvSIPLPipelineNotifier::NOTIF_WARN_ICP_FRAME_DISCONTINUITY:
            LOG_ERR("Pipeline: %u, NOTIF_WARN_ICP_FRAME_DISCONTINUITY\n", oNotificationData.uIndex);
            m_uNumFrameDiscontinuities++;
            break;
        case NvSIPLPipelineNotifier::NOTIF_WARN_ICP_CAPTURE_TIMEOUT:
            LOG_ERR("Pipeline: %u, NOTIF_WARN_ICP_CAPTURE_TIMEOUT\n", oNotificationData.uIndex);
            break;
        case NvSIPLPipelineNotifier::NOTIF_ERROR_ICP_BAD_INPUT_STREAM:
            LOG_ERR("Pipeline: %u, NOTIF_ERROR_ICP_BAD_INPUT_STREAM\n", oNotificationData.uIndex);
            break;
        case NvSIPLPipelineNotifier::NOTIF_ERROR_ICP_CAPTURE_FAILURE:
            LOG_ERR("Pipeline: %u, NOTIF_ERROR_ICP_CAPTURE_FAILURE\n", oNotificationData.uIndex);
            m_bInError = true;
            break;
        case NvSIPLPipelineNotifier::NOTIF_ERROR_ICP_EMB_DATA_PARSE_FAILURE:
            LOG_ERR("Pipeline: %u, NOTIF_ERROR_ICP_EMB_DATA_PARSE_FAILURE\n", oNotificationData.uIndex);
            m_bInError = true;
            break;
        case NvSIPLPipelineNotifier::NOTIF_ERROR_ISP_PROCESSING_FAILURE:
            LOG_ERR("Pipeline: %u, NOTIF_ERROR_ISP_PROCESSING_FAILURE\n", oNotificationData.uIndex);
            m_bInError = true;
            break;
        case NvSIPLPipelineNotifier::NOTIF_ERROR_ISP_PROCESSING_FAILURE_RECOVERABLE:
            LOG_ERR("Pipeline: %u, NOTIF_ERROR_ISP_PROCESSING_FAILURE_RECOVERABLE\n", oNotificationData.uIndex);
            break;
        case NvSIPLPipelineNotifier::NOTIF_ERROR_ACP_PROCESSING_FAILURE:
            LOG_ERR("Pipeline: %u, NOTIF_ERROR_ACP_PROCESSING_FAILURE\n", oNotificationData.uIndex);
            m_bInError = true;
            break;
        case NvSIPLPipelineNotifier::NOTIF_ERROR_CDI_SET_SENSOR_CTRL_FAILURE:
            LOG_ERR("Pipeline: %u, NOTIF_ERROR_CDI_SET_SENSOR_CTRL_FAILURE\n", oNotificationData.uIndex);
            break;
        case NvSIPLPipelineNotifier::NOTIF_ERROR_INTERNAL_FAILURE:
            LOG_ERR("Pipeline: %u, NOTIF_ERROR_INTERNAL_FAILURE\n", oNotificationData.uIndex);
            m_bInError = true;
            break;
        case NvSIPLPipelineNotifier::NOTIF_ERROR_ICP_AUTH_FAILURE:
            m_uNumFailedAuthentications++;
            LOG_ERR("Pipeline: %u, ICP_AUTH_FAILURE frame=%lu\n",
                    oNotificationData.uIndex, oNotificationData.frameSeqNumber);
            break;
        case NvSIPLPipelineNotifier::NOTIF_ERROR_ICP_AUTH_OUT_OF_ORDER:
            m_uNumOutOfOrderAuthentications++;
            LOG_ERR("Pipeline: %u, ICP_AUTH_OUT_OF_ORDER frame=%lu\n",
                    oNotificationData.uIndex, oNotificationData.frameSeqNumber);
            break;
        case NvSIPLPipelineNotifier::NOTIF_INIT_ERROR_FAILURE:
            {
                std::string errCode = "error";
                bool skipError = false;
                switch (oNotificationData.initErrCode) {
                    case InitErrorCode::CAMERA_POWER_FAILURE:
                        errCode = "CAMERA_POWER";
                        break;
                    case InitErrorCode::CAMERA_INIT_FAILURE:
                        errCode = "CAMERA_INIT";
                        break;
                    case InitErrorCode::CAMERA_EN_LINK_DETECT_FAILURE:
                        errCode = "CAMERA_EN_LINK_DETECT";
                        break;
                    case InitErrorCode::CAMERA_AUTH_FAILURE:
                        errCode = "CAMERA_AUTH";
                        break;
                    case InitErrorCode::CAMERA_POST_INIT_FAILURE:
                        errCode = "CAMERA_POST_INIT";
                        break;
                    case InitErrorCode::CAMERA_START_FAILURE:
                        errCode = "CAMERA_START";
                        break;
                    case InitErrorCode::CAMERA_OVER_VOLTAGE:
                        errCode = "CAMERA_OVER_VOLTAGE";
                        break;
                    case InitErrorCode::CAMERA_UNDER_VOLTAGE:
                        errCode = "CAMERA_UNDER_VOLTAGE";
                        break;
                    case InitErrorCode::CAMERA_LINE_TO_LINE_FAULT:
                        errCode = "LINE_TO_LINE_FAULT";
                        break;
                    case InitErrorCode::CAMERA_OVER_CURRENT:
                        errCode = "CAMERA_OVER_CURRENT";
                        break;
                    case InitErrorCode::CAMERA_THERMAL_SHUTDOWN:
                        errCode = "CAMERA_THERMAL_SHUTDOWN";
                        break;
                    case InitErrorCode::CAMERA_CIRCUIT_OPEN_DISCONNECTION:
                        errCode = "CIRCUIT_OPEN_DISCONNECTION";
                        break;
                    case InitErrorCode::CAMERA_INCORRECT_IMPEDANCE:
                        errCode = "INCORRECT_IMPEDANCE";
                        break;
                    case InitErrorCode::DEFAULT_INIT_ERROR_CODE_NONE:
                    default:
                        LOG_ERR("Pipeline: %u, Default/Unknown/Invalid Init Error Code\n",
                                 oNotificationData.uIndex);
                        skipError = true;
                        break;
                }
                if (!skipError) {
                    LOG_ERR("Pipeline: %u, observed init failures due to %s\n",
                            oNotificationData.uIndex, errCode.c_str());
                }
                break;
            }
        default:
            LOG_ERR("Pipeline: %u, Unknown/Invalid notification\n", oNotificationData.uIndex);
            break;
        }
        return;
}

void SIPLCoeCamera::CoeEventThread(uint32_t const cameraModule, uint32_t const threadIndex)
{
    LOG_INFO("COE Event Thread for module %u started\n", cameraModule);
    SIPLStatus status = NVSIPL_STATUS_OK;
    NvSIPLPipelineNotifier::NotificationData event;
    ThreadData *threadData = &m_threadData[cameraModule][threadIndex];
    m_threadReady[cameraModule][threadIndex] = true;
    while (!m_exitAllThreads && (status != NVSIPL_STATUS_EOF)) {
        if (threadData->eventQueue) {
            status = threadData->eventQueue->Get(event, 1000000); // 1 second timeout

            if (status == NVSIPL_STATUS_OK) {
                m_queueCounts[cameraModule][threadIndex]++;
                // Process notification
                LOG_DBG("COE Event received: %u\n", static_cast<int>(event.eNotifType));
                OnEvent(event);
            } else if (status == NVSIPL_STATUS_TIMED_OUT) {
                LOG_WARN("Queue timeout\n");
            } else if (status == NVSIPL_STATUS_EOF) {
                LOG_WARN("Queue shutdown\n");
            } else {
                LOG_ERR("Unexpected queue return status\n");
            }
        } else {
            usleep(10000); // 10ms
        }
    }
    LOG_INFO("COE Event Thread for module %u exited\n", cameraModule);
}
void SIPLCoeCamera::CoeCpuSignalThread(uint32_t const cameraModule, uint32_t const threadIndex)
{
    LOG_INFO("COE CPU Signal Thread for module %u started\n", cameraModule);
    m_threadReady[cameraModule][threadIndex] = true;
    while (!m_exitAllThreads) {
        // CPU signaling logic (simplified)
        usleep(10000); // 10ms
    }
    LOG_INFO("COE CPU Signal Thread for module %u exited\n", cameraModule);
}

SIPLStatus SIPLCoeCamera::CoeBufObjManager::AllocateBufObjs(
            INvSIPLCamera *siplCamera,
            uint32_t uSensor,
            INvSIPLClient::ConsumerDesc::OutputType output,
            uint32_t numObjects)
{
    // Helper for cleaning up attribute lists
    struct CloseNvSciBufAttrList {
        void operator()(NvSciBufAttrList *attrList) const {
            if (attrList != nullptr) {
                if ((*attrList) != nullptr) {
                    NvSciBufAttrListFree(*attrList);
                }
                delete attrList;
            }
        }
    };
    std::unique_ptr<NvSciBufAttrList, CloseNvSciBufAttrList> attrList;
    attrList.reset(new NvSciBufAttrList());
    NvSciError err = NvSciBufAttrListCreate(m_sciBufModule, attrList.get());
    CHK_NVSCISTATUS_AND_RETURN(err, "NvSciBufAttrListCreate()");
    NvSciBufType bufType = NvSciBufType_Image;
    NvSciBufAttrValAccessPerm accessPerm = NvSciBufAccessPerm_Readonly;
    bool isCpuAcccessReq = true;
    bool isCpuCacheEnabled = true;
    NvSciBufAttrKeyValuePair attrKvp[] = {
        { NvSciBufGeneralAttrKey_Types, &bufType, sizeof(bufType) },
        { NvSciBufGeneralAttrKey_RequiredPerm, &accessPerm, sizeof(accessPerm) },
        { NvSciBufGeneralAttrKey_NeedCpuAccess, &isCpuAcccessReq, sizeof(isCpuAcccessReq) },
        { NvSciBufGeneralAttrKey_EnableCpuCache, &isCpuCacheEnabled, sizeof(isCpuCacheEnabled) }
    };
    size_t uNumAttrs = (output == INvSIPLClient::ConsumerDesc::OutputType::ICP) ? 2U : 4U;
    err = NvSciBufAttrListSetAttrs(*(attrList.get()), attrKvp, uNumAttrs);
    CHK_NVSCISTATUS_AND_RETURN(err, "NvSciBufAttrListSetAttrs()");
    SIPLStatus status = siplCamera->GetImageAttributes(uSensor, output, *(attrList.get()));
    CHK_STATUS_AND_RETURN(status, "INvSIPLCamera::GetImageAttributes()");
    std::unique_ptr<NvSciBufAttrList, CloseNvSciBufAttrList> reconciledAttrList;
    std::unique_ptr<NvSciBufAttrList, CloseNvSciBufAttrList> conflictAttrList;
    reconciledAttrList.reset(new NvSciBufAttrList());
    conflictAttrList.reset(new NvSciBufAttrList());
    err = NvSciBufAttrListReconcile(attrList.get(),
                                    1U,
                                    reconciledAttrList.get(),
                                    conflictAttrList.get());
    CHK_NVSCISTATUS_AND_RETURN(err, "NvSciBufAttrListReconcile()");
    // Pre-allocate vector capacity to avoid reallocations during frame capture
    m_sciBufObjs.reserve(numObjects);
    for (size_t i = 0U; i < numObjects; i++) {
        NvSciBufObj bufObj {};
        err = NvSciBufObjAlloc(*(reconciledAttrList.get()), &bufObj);
        CHK_NVSCISTATUS_AND_RETURN(err, "NvSciBufObjAlloc()");
        CHK_PTR_AND_RETURN(bufObj, "NvSciBufObjAlloc()");
        m_sciBufObjs.push_back(bufObj);
    }
    return NVSIPL_STATUS_OK;
}

SIPLCoeCamera::CoeBufObjManager::~CoeBufObjManager()
{
    for (uint32_t i = 0U; i < m_sciBufObjs.size(); i++) {
        if (m_sciBufObjs[i] != nullptr) {
            NvSciBufObjFree(m_sciBufObjs[i]);
        }
    }
    // Swap sciBufObjs vector with an equivalent empty vector to force deallocation
    std::vector<NvSciBufObj>().swap(m_sciBufObjs);
}

SIPLStatus SIPLCoeCamera::GetCoeSystemConfigFromQuery()
{
    SIPLStatus status;
    CameraSystemConfig camSysConfig;
    std::vector<CoEOverride> coeOverrides;

    if (!m_upQuery) {
        LOG_ERR("ERROR: SIPL Query instance not available\n");
        return NVSIPL_STATUS_ERROR;
    }

    status = m_upQuery->ParseDatabase();
    if (status != NVSIPL_STATUS_OK) {
        LOG_ERR("ERROR: ParseDatabase() failed with status: %d\n", status);
        return status;
    }

    if (m_cmdline.sTestConfigFile != "") {

        LOG_INFO("Parsing JSON file: %s\n", m_cmdline.sTestConfigFile.c_str());
        status = m_upQuery->ParseJsonFile(m_cmdline.sTestConfigFile);
        CHK_STATUS_AND_RETURN(status, "INvSIPLQuery::ParseJsonFile");

        const CameraDeviceInfoList* deviceList = m_upQuery->GetDeviceInfoList();
        if (deviceList == nullptr || deviceList->cameraSystemConfig.cameras.empty()) {
            LOG_ERR("Failed to get device info list\n");
            return NVSIPL_STATUS_ERROR;
        }

        for (const auto& camera : deviceList->cameraSystemConfig.cameras) {
            camSysConfig.cameras.push_back(camera);
        }

        for (const auto& transport : deviceList->cameraSystemConfig.transports) {
            camSysConfig.transports.push_back(transport);
        }
    } else if (m_cmdline.sConfigName != "") {
        LOG_DBG("Getting platform configuration for %s\n", m_cmdline.sConfigName.c_str());
        status = m_upQuery->GetCameraSystemConfig(m_cmdline.sConfigName, camSysConfig);
        if (status != NVSIPL_STATUS_OK) {
            LOG_ERR("ERROR: GetCameraConfig failed for %s : %d\n", m_cmdline.sConfigName.c_str(), status);
            return NVSIPL_STATUS_ERROR;
        }
    } else {
        LOG_ERR("No config name or json file provided\n");
        LOG_ERR("Use either -c or -t to specify a config name or json file\n");
        LOG_ERR("usage: nvsipl_coe_camera -c VB1940_Camera\n");
        LOG_ERR("Available configs:\n");
        CCmdLineParser::ShowConfigs();
        LOG_ERR("usage: nvsipl_coe_camera -t /path/to/config.json\n");
        return NVSIPL_STATUS_ERROR;
    }

    if (m_cmdline.sCoEOverridePath != "") {
        LoadCoEOverrideFile(m_cmdline.sCoEOverridePath, coeOverrides);
        LOG_INFO("CoE override file loaded: %s\n", m_cmdline.sCoEOverridePath.c_str());
        for (const auto& override : coeOverrides) {
            PrintMacAddress(override.macAddress, "CoE Override MAC:");
            PrintIpAddress(override.ipAddress, "CoE Override IP:");
            LOG_INFO("CoE Override: HSB=%s, Interface=%s\n",
                    override.hsbName.c_str(), override.interfaceName.c_str());
        }

        // Apply CoE overrides to camera system configuration
        ApplyCoEOverrides(camSysConfig, coeOverrides);
    }

    m_coeSystemConfig = camSysConfig;

    for (size_t i = 0; i < camSysConfig.cameras.size(); ++i) {
        const auto& loadedCamera = camSysConfig.cameras[i];
        LOG_INFO("Camera[%d]: Name: %s, Platform: %s, Sensor ID: %d, Sensor Name: %s\n",
                        i, loadedCamera.name.c_str(), loadedCamera.platform.c_str(),
                        loadedCamera.sensorInfo.id, loadedCamera.sensorInfo.name.c_str());
        LOG_INFO("Resolution: %dx%d, Embedded lines: %d top, %d bottom, FPS: %f\n",
                        loadedCamera.sensorInfo.vcInfo.resolution.width,
                        loadedCamera.sensorInfo.vcInfo.resolution.height,
                        loadedCamera.sensorInfo.vcInfo.embeddedTopLines,
                        loadedCamera.sensorInfo.vcInfo.embeddedBottomLines,
                        loadedCamera.sensorInfo.vcInfo.fps);
        LOG_INFO("Input Format: %d, Pixel Order (CFA): 0x%x (%d)\n",
                        static_cast<uint32_t>(loadedCamera.sensorInfo.vcInfo.inputFormat),
                        static_cast<uint32_t>(loadedCamera.sensorInfo.vcInfo.cfa),
                        static_cast<uint32_t>(loadedCamera.sensorInfo.vcInfo.cfa));

        LOG_INFO("MIPI Settings:\n");
        LOG_INFO("Lanes: %d, Clock Rate: %d, DPHY Rate: %d, PHY Mode: %d, Lane Swizzle: %d\n",
                        loadedCamera.mipiSettings.lanes, loadedCamera.mipiSettings.mipiClockRate,
                        loadedCamera.mipiSettings.dphyRate, static_cast<uint32_t>(loadedCamera.mipiSettings.phyMode),
                        static_cast<uint32_t>(loadedCamera.mipiSettings.laneSwizzle));
    }

    return NVSIPL_STATUS_OK;
}

SIPLStatus SIPLCoeCamera::CoeSetPlatformCfg(CameraSystemConfig &cameraSystemConfig)
{
    LOG_INFO("CoeSetPlatformCfg start\n");

    if (!m_upCamera) {
        LOG_ERR("CoeSetPlatformCfg camera instance is null\n");
        return NVSIPL_STATUS_ERROR;
    }

    // Use new CameraSystemConfig API
    SIPLStatus status = m_upCamera->SetPlatformCfg(cameraSystemConfig);
    if (status != NVSIPL_STATUS_OK) {
        LOG_ERR("SetPlatformCfg failed for COE: %d\n", status);
        return status;
    }

    m_coeSystemConfig = cameraSystemConfig;
    // Build module list from camera system config
    m_coeModules.clear();
    for (size_t i = 0; i < cameraSystemConfig.cameras.size(); i++) {
        CoeModuleData module;
        module.sensorId = cameraSystemConfig.cameras[i].sensorInfo.id;
        module.index = static_cast<uint32_t>(i);
        module.name = cameraSystemConfig.cameras[i].name;
        module.platform = cameraSystemConfig.cameras[i].platform;
        module.sensorName = cameraSystemConfig.cameras[i].sensorInfo.name;
        m_coeModules.push_back(module);
        LOG_INFO("Module[%d] created: SensorID=%d, Name=%s\n",
                i, module.sensorId, module.name.c_str());
    }

    LOG_INFO("CoeSetPlatformCfg() completed\n");
    return NVSIPL_STATUS_OK;
}

SIPLStatus SIPLCoeCamera::CoeSetPipelineCfg(NvSIPLPipelineConfiguration &pipelineCfg)
{
    LOG_INFO("CoeSetPipelineCfg() start\n");
    LOG_DBG("Pipeline Config: ICP=%s, ISP0=%s, ISP1=%s, ISP2=%s\n",
                    (pipelineCfg.captureOutputRequested ? "ON" : "OFF"),
                    (pipelineCfg.isp0OutputRequested ? "ON" : "OFF"),
                    (pipelineCfg.isp1OutputRequested ? "ON" : "OFF"),
                    (pipelineCfg.isp2OutputRequested ? "ON" : "OFF"));
    if (!m_upCamera) {
        LOG_ERR("Camera instance is null\n");
        return NVSIPL_STATUS_ERROR;
    }
    for (const auto& module : m_coeModules) {
        uint32_t sensorId = module.sensorId;
        uint32_t index = module.index;
        LOG_DBG("Processing Module[%d]: SensorID=%d, Name=%s\n",
                        index, sensorId, module.name.c_str());
        // Initialize pipeline queues for this module
        m_queues[index] = {};

        SIPLStatus status = m_upCamera->SetPipelineCfg(sensorId, pipelineCfg, m_queues[index]);
        if (status != NVSIPL_STATUS_OK) {
            LOG_ERR("SetPipelineCfg failed for COE sensor: %u, status: %d\n", sensorId, status);
            return status;
        }
        LOG_DBG("SetPipelineCfg completed successfully for sensor %d\n", sensorId);
        // Log queue information
        LOG_DBG("Queues created for sensor %d:\n", sensorId);
        LOG_DBG("     - captureCompletionQueue: %s\n", (m_queues[index].captureCompletionQueue ? "CREATED" : "NULL"));
        LOG_DBG("     - isp0CompletionQueue: %s\n", (m_queues[index].isp0CompletionQueue ? "CREATED" : "NULL"));
        LOG_DBG("     - isp1CompletionQueue: %s\n", (m_queues[index].isp1CompletionQueue ? "CREATED" : "NULL"));
        LOG_DBG("     - isp2CompletionQueue: %s\n", (m_queues[index].isp2CompletionQueue ? "CREATED" : "NULL"));
        LOG_DBG("     - notificationQueue: %s\n", (m_queues[index].notificationQueue ? "CREATED" : "NULL"));
    }
    m_coePipelineCfg = pipelineCfg;
    LOG_DBG("=== CoeSetPipelineCfg() COMPLETED ===\n");
    return NVSIPL_STATUS_OK;
}

SIPLStatus SIPLCoeCamera::CoeInit()
{
    LOG_DBG("CoeInit() start\n");

    if (!m_upCamera) {
        LOG_ERR("Camera instance is null\n");
        return NVSIPL_STATUS_ERROR;
    }

    SIPLStatus status = m_upCamera->Init();
    if (status != NVSIPL_STATUS_OK) {
        LOG_ERR("Init failed: %d\n", status);
        return status;
    }

    LOG_INFO("CoeInit() completed\n");
    return NVSIPL_STATUS_OK;
}

void SIPLCoeCamera::CoeDeinit()
{
    LOG_INFO("CoeDeinit start\n");

    ExitAllCoeThreads();

    // Call camera deinit after all threads are stopped
    if (m_upCamera) {
        SIPLStatus status = m_upCamera->Deinit();
        if (status != NVSIPL_STATUS_OK) {
            LOG_ERR("Deinit failed: %d\n", status);
        }
    }

    LOG_INFO(" CoeDeinit completed\n");
}

// Change the sequence to match legacy SIPL  test exactly
SIPLStatus SIPLCoeCamera::CoeSetupAndInit(CameraSystemConfig &cameraSystemConfig)
{
    SIPLStatus status = CoeSetPlatformCfg(cameraSystemConfig);
    if (status != NVSIPL_STATUS_OK) {
        LOG_ERR("CoeSetPlatformCfg failed\n");
        return NVSIPL_STATUS_ERROR;
    }

    status = CoeSetPipelineCfg(g_coePipelineCfg);
    if (status != NVSIPL_STATUS_OK) {
        LOG_ERR("CoeSetPipelineCfg failed\n");
        return NVSIPL_STATUS_ERROR;
    }

    status = CoeInit();
    if (status != NVSIPL_STATUS_OK) {
        LOG_ERR("CoeInit failed\n");
        return NVSIPL_STATUS_ERROR;
    }

    int totalModules = static_cast<int>(m_coeModules.size());
    LOG_INFO("Total modules configured: %d\n", totalModules);
    struct CoeModuleData *module = CoeFirstModule();
    int moduleIndex = 0;
    while (module != nullptr) {
        moduleIndex++;
        LOG_INFO("Module #%d: %s (Sensor ID: %d, Index: %d)\n",
            moduleIndex, module->name.c_str(), module->sensorId, module->index);
        module = CoeNextModule();
    }

    return NVSIPL_STATUS_OK;
}
struct SIPLCoeCamera::CoeModuleData *SIPLCoeCamera::CoeFirstModule()
{
    m_coeModuleIndex = 0;
    return CoeGetModule();
}
struct SIPLCoeCamera::CoeModuleData *SIPLCoeCamera::CoeNextModule()
{
    m_coeModuleIndex++;
    return CoeGetModule();
}
struct SIPLCoeCamera::CoeModuleData *SIPLCoeCamera::CoeGetModule()
{
    if (m_coeModuleIndex < m_coeModules.size()) {
        return &m_coeModules[m_coeModuleIndex];
    }
    return nullptr;
}

void SIPLCoeCamera::AllocateNvSciSyncObjects(NvSciSyncModule &sciSyncModule, const uint32_t uSensor,
                                          INvSIPLClient::ConsumerDesc::OutputType OutputType,
                                          NvSiplNvSciSyncObjType fenceType,
                                          std::vector<NvSciSyncObj> &nvSyncObj, uint32_t numFences)
{
    SIPLStatus status = NVSIPL_STATUS_OK;
    NvSciError err = NvSciError_Success;
    NvSciSyncAttrList signalerAttrList = nullptr;
    NvSciSyncAttrList waiterAttrList = nullptr;
    NvSciSyncAttrList unreconciledList[2];
    NvSciSyncAttrList reconciledList = nullptr;
    NvSciSyncAttrList newConflictList = nullptr;
    const size_t inputCount = 2U;
    if (numFences == 0U) {
        if (fenceType == NVSIPL_PRESYNCOBJ) {
            numFences = 16U;
        } else {
            numFences = 1U;
        }
    }

    // Resize nvSyncObj according to fence type before allocating an object
    nvSyncObj.resize(numFences);
    err = NvSciSyncAttrListCreate(sciSyncModule, &waiterAttrList);
    if (err != NvSciError_Success) {
        LOG_ERR("[ALLOC_SYNC]  NvSciSyncAttrListCreate waiter failed: %d\n", err);
        return;
    }
    err = NvSciSyncAttrListCreate(sciSyncModule, &signalerAttrList);
    if (err != NvSciError_Success) {
        LOG_ERR("[ALLOC_SYNC]  NvSciSyncAttrListCreate signaler failed: %d\n", err);
        if (waiterAttrList != nullptr) {
            NvSciSyncAttrListFree(waiterAttrList);
        }
        return;
    }
    NvSciSyncAttrKeyValuePair keyValue[2];
    memset(keyValue, 0, sizeof(keyValue));
    bool cpuSignaler_waiter = true;
    keyValue[0].attrKey = NvSciSyncAttrKey_NeedCpuAccess;
    keyValue[0].value = (void *)&cpuSignaler_waiter;
    keyValue[0].len = sizeof(cpuSignaler_waiter);
    if (fenceType == NVSIPL_PRESYNCOBJ) {
        NvSciSyncAccessPerm cpuPerm = NvSciSyncAccessPerm_SignalOnly;
        keyValue[1].attrKey = NvSciSyncAttrKey_RequiredPerm;
        keyValue[1].value = (void *)&cpuPerm;
        keyValue[1].len = sizeof(cpuPerm);
        err = NvSciSyncAttrListSetAttrs(signalerAttrList, keyValue, 2);
        if (err != NvSciError_Success) {
            LOG_ERR("[ALLOC_SYNC]  NvSciSyncAttrListSetAttrs signaler failed: %d\n", err);
            goto cleanup;
        }
        status = m_upCamera->FillNvSciSyncAttrList(uSensor, OutputType, waiterAttrList, SIPL_WAITER);
        if (status != NVSIPL_STATUS_OK) {
            LOG_ERR("[ALLOC_SYNC]  FillNvSciSyncAttrList waiter failed: %u\n", static_cast<uint32_t>(status));
            goto cleanup;
        }
        unreconciledList[0] = waiterAttrList;
        unreconciledList[1] = signalerAttrList;
    } else {
        NvSciSyncAccessPerm cpuPerm = NvSciSyncAccessPerm_WaitOnly;
        keyValue[1].attrKey = NvSciSyncAttrKey_RequiredPerm;
        keyValue[1].value = (void *)&cpuPerm;
        keyValue[1].len = sizeof(cpuPerm);
        err = NvSciSyncAttrListSetAttrs(waiterAttrList, keyValue, 2);
        if (err != NvSciError_Success) {
            LOG_ERR("[ALLOC_SYNC]  NvSciSyncAttrListSetAttrs waiter failed: %d\n", err);
            goto cleanup;
        }
        status = m_upCamera->FillNvSciSyncAttrList(uSensor, OutputType, signalerAttrList, SIPL_SIGNALER);
        if (status != NVSIPL_STATUS_OK) {
            LOG_ERR("[ALLOC_SYNC]  FillNvSciSyncAttrList signaler failed: %u\n", static_cast<uint32_t>(status));
            goto cleanup;
        }
        unreconciledList[0] = signalerAttrList;
        unreconciledList[1] = waiterAttrList;
    }
    err = NvSciSyncAttrListReconcile(unreconciledList, inputCount, &reconciledList, &newConflictList);
    if (err != NvSciError_Success) {
        LOG_ERR("[ALLOC_SYNC]  NvSciSyncAttrListReconcile failed: %d\n", err);
        goto cleanup;
    }
    for (uint32_t fence = 0U; fence < nvSyncObj.size(); fence++) {
        err = NvSciSyncObjAlloc(reconciledList, &nvSyncObj[fence]);
        if (err != NvSciError_Success) {
            LOG_ERR("[ALLOC_SYNC]  NvSciSyncObjAlloc failed for fence %u: %d\n", fence, err);
            goto cleanup;
        }
    }
    LOG_DBG("Successfully allocated %u sync objects\n", nvSyncObj.size());
cleanup:
    if (reconciledList != nullptr) {
        NvSciSyncAttrListFree(reconciledList);
    }
    if (newConflictList != nullptr) {
        NvSciSyncAttrListFree(newConflictList);
    }
    if (signalerAttrList != nullptr) {
        NvSciSyncAttrListFree(signalerAttrList);
    }
    if (waiterAttrList != nullptr) {
        NvSciSyncAttrListFree(waiterAttrList);
    }
}

// WriteISPBufferToFile - Following SIPL OnFrameAvailable pattern
SIPLStatus SIPLCoeCamera::WriteISPBufferToFile(INvSIPLClient::INvSIPLNvMBuffer *pNvmBuf, uint32_t frameNum, uint32_t cameraModule, uint32_t threadIndex)
{
    LOG_DBG("WriteISPBufferToFile() start\n");
    if (pNvmBuf == nullptr) {
        LOG_ERR("pNvmBuf pointer is null\n");
        return NVSIPL_STATUS_BAD_ARGUMENT;
    }
    // Check if we should dump this ISP frame based on user arguments and count
    bool shouldDump = false;
    const char* outputType = "ISP";
    const char* fileExtension = "yuv";
    if ((threadIndex == THREAD_INDEX_ISP0 && !m_cmdline.bDisableISP0) ||
        (threadIndex == THREAD_INDEX_ISP1 && !m_cmdline.bDisableISP1) ||
        (threadIndex == THREAD_INDEX_ISP2 && !m_cmdline.bDisableISP2)) {
        LOG_DBG("WriteISPBufferToFile dumping\n");
        shouldDump = true;
    }
    if (!shouldDump) {
        LOG_INFO("WriteISPBufferToFile not dumping\n");
        return NVSIPL_STATUS_OK; // Not an error, just not dumping this type
    }
    // Check if we've already dumped enough frames for this sensor/output
    if (m_dumpedFrameCount[cameraModule][threadIndex] >= m_cmdline.uNumWriteFrames) {
        LOG_DBG("WriteISPBufferToFile already %u/%u dumped enough frames\n",
        m_dumpedFrameCount[cameraModule][threadIndex], m_cmdline.uNumWriteFrames);
        return NVSIPL_STATUS_OK; // Already dumped enough frames
    }
    // Convert thread index to output number for filename
    uint32_t outputIndex = 0;
    if (threadIndex == THREAD_INDEX_ISP0) {
        outputIndex = 0;
    } else if (threadIndex == THREAD_INDEX_ISP1) {
        outputIndex = 1;
    } else if (threadIndex == THREAD_INDEX_ISP2) {
        outputIndex = 2;
    }
    LOG_INFO("Dumping %s frame %u from sensor %u (output %u)\n", outputType, frameNum, cameraModule, outputIndex);
    // Wait on EOF fence
    NvSciSyncFence fence = NvSciSyncFenceInitializer;
    SIPLStatus status = pNvmBuf->GetEOFNvSciSyncFence(&fence);
    if (status != NVSIPL_STATUS_OK) {
        LOG_ERR("GetEOFNvSciSyncFence failed: %d", status);
        return status;
    }
    NvSciError sciErr = NvSciSyncFenceWait(&fence, m_cpuWaitContext, FENCE_FRAME_TIMEOUT_MS * 2000UL);
    if (sciErr != NvSciError_Success) {
        if (sciErr == NvSciError_Timeout) {
            LOG_ERR("Frame done NvSciSyncFenceWait timed out\n");
        } else {
            LOG_ERR("Frame done NvSciSyncFenceWait failed: %d\n", sciErr);
        }
        NvSciSyncFenceClear(&fence);
        return NVSIPL_STATUS_ERROR;
    }
    NvSciSyncFenceClear(&fence);
    // Get buffer and extract attributes
    NvSciBufObj bufPtr = pNvmBuf->GetNvSciBufImage();
    if (bufPtr == nullptr) {
        LOG_ERR("Failed to get NvSciBuf image object\n");
        return NVSIPL_STATUS_ERROR;
    }
    BufferAttrs bufAttrs;
    status = PopulateBufAttr(bufPtr, bufAttrs);
    if (status != NVSIPL_STATUS_OK) {
        LOG_ERR("PopulateBufAttr failed\n");
        return NVSIPL_STATUS_BAD_ARGUMENT;
    }
    // Get buffer parameters
    uint32_t numSurfaces = 0U;
    float *xScalePtr = nullptr, *yScalePtr = nullptr;
    uint32_t *bytesPerPixelPtr = nullptr;
    bool isPackedYUV = false;
    status = GetISPBuffParams(bufAttrs,
                              &xScalePtr,
                              &yScalePtr,
                              &bytesPerPixelPtr,
                              &numSurfaces,
                              &isPackedYUV);
    if (status != NVSIPL_STATUS_OK) {
        LOG_ERR("GetISPBuffParams failed\n");
        return status;
    }
    // Prepare pixel extraction
    uint32_t pBuffPitches[MAX_NUM_SURFACES] = { 0U };
    uint8_t *pBuff[MAX_NUM_SURFACES] = { 0U };
    uint32_t size[MAX_NUM_SURFACES] = { 0U };
    uint32_t imageSize = 0U;
    uint32_t height = bufAttrs.planeHeights[0];
    uint32_t width = bufAttrs.planeWidths[0];
    for (uint32_t i = 0U; i < numSurfaces; i++) {
        size[i] = (width * xScalePtr[i] * height * yScalePtr[i] * bytesPerPixelPtr[i]);
        imageSize += size[i];
        pBuffPitches[i] = (uint32_t)((float)width * xScalePtr[i]) * bytesPerPixelPtr[i];
    }
    // Allocate temporary buffer for pixel data
    uint8_t *pImageBuff = new (std::nothrow) uint8_t[imageSize];
    if (pImageBuff == nullptr) {
        LOG_ERR("Out of memory\n");
        return NVSIPL_STATUS_OUT_OF_MEMORY;
    }
    std::fill(pImageBuff, pImageBuff + imageSize, 0x00);
    uint8_t *buffIter = pImageBuff;
    for (uint32_t i = 0U; i < numSurfaces; i++) {
        pBuff[i] = buffIter;
        buffIter += (uint32_t)(height * yScalePtr[i] * pBuffPitches[i]);
    }
    // Extract pixels
    sciErr = NvSciBufObjGetPixels(bufPtr, nullptr, (void **)pBuff, size, pBuffPitches);
    if (sciErr != NvSciError_Success) {
        LOG_ERR("NvSciBufObjGetPixels failed: %d\n", sciErr);
        delete[] pImageBuff;
        return NVSIPL_STATUS_ERROR;
    }
    // Write to file with ISP output naming
    char filename[256];
    snprintf(filename, sizeof(filename), "/tmp/%s_sensor%u_%s%u_frame_%u.%s",
             m_cmdline.sFiledumpPrefix.c_str(), cameraModule, outputType, outputIndex, frameNum, fileExtension);
    FILE *pOutFile = fopen(filename, "wb");
    if (pOutFile == nullptr) {
        LOG_ERR("Failed to create output file: %s\n", filename);
        delete[] pImageBuff;
        return NVSIPL_STATUS_ERROR;
    }
    for (uint32_t i = 0U; i < numSurfaces; i++) {
        if (fwrite(pBuff[i], size[i], 1U, pOutFile) != 1U) {
            LOG_ERR("File write failed for surface %u\n", i);
            fclose(pOutFile);
            delete[] pImageBuff;
            return NVSIPL_STATUS_ERROR;
        }
    }
    fclose(pOutFile);
    delete[] pImageBuff;
    // Increment dump count for this sensor/output and show success message
    m_dumpedFrameCount[cameraModule][threadIndex]++;
    LOG_INFO("Dumped %s frame %u -> %s (%u bytes) [%u/%u]\n",
        outputType, frameNum, filename, imageSize,
        m_dumpedFrameCount[cameraModule][threadIndex], m_cmdline.uNumWriteFrames);
    return NVSIPL_STATUS_OK;
}

SIPLStatus SIPLCoeCamera::PopulateBufAttr(const NvSciBufObj& sciBufObj, BufferAttrs &bufAttrs)
{
    NvSciError err = NvSciError_Success;
    NvSciBufAttrList bufAttrList;
    // Get all buffer attributes needed for ISP analysis
    NvSciBufAttrKeyValuePair imgAttrs[] = {
        { NvSciBufImageAttrKey_Size, NULL, 0 },                           //0
        { NvSciBufImageAttrKey_PlaneCount, NULL, 0 },                     //1
        { NvSciBufImageAttrKey_PlanePitch, NULL, 0 },                     //2
        { NvSciBufImageAttrKey_PlaneWidth, NULL, 0 },                     //3
        { NvSciBufImageAttrKey_PlaneHeight, NULL, 0 },                    //4
        { NvSciBufImageAttrKey_PlaneBitsPerPixel, NULL, 0 },              //5
        { NvSciBufImageAttrKey_PlaneAlignedHeight, NULL, 0 },             //6
        { NvSciBufImageAttrKey_PlaneAlignedSize, NULL, 0 },               //7
        { NvSciBufImageAttrKey_PlaneChannelCount, NULL, 0 },              //8
        { NvSciBufImageAttrKey_PlaneOffset, NULL, 0 },                    //9
        { NvSciBufImageAttrKey_PlaneColorFormat, NULL, 0 },               //10
        { NvSciBufImageAttrKey_TopPadding, NULL, 0 },                     //11
        { NvSciBufImageAttrKey_BottomPadding, NULL, 0 },                  //12
        { NvSciBufGeneralAttrKey_CpuNeedSwCacheCoherency, NULL, 0 },      //13
    };
    err = NvSciBufObjGetAttrList(sciBufObj, &bufAttrList);
    if (err != NvSciError_Success) {
        LOG_ERR("[POPULATE_ATTR]  NvSciBufObjGetAttrList failed: %d\n", err);
        return NVSIPL_STATUS_ERROR;
    }
    err = NvSciBufAttrListGetAttrs(bufAttrList, imgAttrs, sizeof(imgAttrs) / sizeof(imgAttrs[0]));
    if (err != NvSciError_Success) {
        LOG_ERR("[POPULATE_ATTR]  NvSciBufAttrListGetAttrs failed: %d\n", err);
        return NVSIPL_STATUS_ERROR;
    }
    // buffer attributes
    if (imgAttrs[0].len != 0) {
        bufAttrs.size = *(static_cast<const uint64_t*>(imgAttrs[0].value));
    } else {
        LOG_WARN("Skipped populating size as length of attribute was 0\n");
        bufAttrs.size = 0;
    }
    if (imgAttrs[1].len != 0) {
        bufAttrs.planeCount = *(static_cast<const uint32_t*>(imgAttrs[1].value));
    } else {
        LOG_WARN("Skipped populating planeCount as length of attribute was 0\n");
        bufAttrs.planeCount = 1;
    }
    // iterate through all planes
    uint32_t maxPlanes = (bufAttrs.planeCount > MAX_NUM_SURFACES) ? MAX_NUM_SURFACES : bufAttrs.planeCount;
    // Plane pitches
    if (imgAttrs[2].len != 0) {
        memcpy(bufAttrs.planePitches,
               static_cast<const uint32_t*>(imgAttrs[2].value),
               maxPlanes * sizeof(bufAttrs.planePitches[0]));
    } else {
        LOG_WARN("Skipped populating planePitches\n");
        memset(bufAttrs.planePitches, 0, sizeof(bufAttrs.planePitches));
    }
    // Plane dimensions
    if (imgAttrs[3].len != 0) {
        memcpy(bufAttrs.planeWidths,
               static_cast<const uint32_t*>(imgAttrs[3].value),
               maxPlanes * sizeof(bufAttrs.planeWidths[0]));
    } else {
        LOG_WARN("Skipped populating planeWidths\n");
        memset(bufAttrs.planeWidths, 0, sizeof(bufAttrs.planeWidths));
    }
    if (imgAttrs[4].len != 0) {
        memcpy(bufAttrs.planeHeights,
               static_cast<const uint32_t*>(imgAttrs[4].value),
               maxPlanes * sizeof(bufAttrs.planeHeights[0]));
    } else {
        LOG_WARN("Skipped populating planeHeights\n");
        memset(bufAttrs.planeHeights, 0, sizeof(bufAttrs.planeHeights));
    }
    // Bits per pixel
    if (imgAttrs[5].len != 0) {
        memcpy(bufAttrs.planeBitsPerPixels,
               static_cast<const uint32_t*>(imgAttrs[5].value),
               maxPlanes * sizeof(bufAttrs.planeBitsPerPixels[0]));
    } else {
        LOG_WARN("Skipped populating planeBitsPerPixels\n");
        memset(bufAttrs.planeBitsPerPixels, 0, sizeof(bufAttrs.planeBitsPerPixels));
    }
    // Additional attributes for comprehensive format support
    if (imgAttrs[6].len != 0) {
        memcpy(bufAttrs.planeAlignedHeights,
               static_cast<const uint32_t*>(imgAttrs[6].value),
               maxPlanes * sizeof(bufAttrs.planeAlignedHeights[0]));
    }
    if (imgAttrs[7].len != 0) {
        memcpy(bufAttrs.planeAlignedSizes,
               static_cast<const uint64_t*>(imgAttrs[7].value),
               maxPlanes * sizeof(bufAttrs.planeAlignedSizes[0]));
    }
    if (imgAttrs[8].len != 0) {
        memcpy(bufAttrs.planeChannelCounts,
               static_cast<const uint8_t*>(imgAttrs[8].value),
               maxPlanes * sizeof(bufAttrs.planeChannelCounts[0]));
    }
    if (imgAttrs[9].len != 0) {
        memcpy(bufAttrs.planeOffsets,
               static_cast<const uint64_t*>(imgAttrs[9].value),
               maxPlanes * sizeof(bufAttrs.planeOffsets[0]));
    }
    if (imgAttrs[10].len != 0) {
        memcpy(bufAttrs.planeColorFormats,
               static_cast<const NvSciBufAttrValColorFmt*>(imgAttrs[10].value),
               maxPlanes * sizeof(bufAttrs.planeColorFormats[0]));
    }
    if (imgAttrs[11].len != 0) {
        memcpy(bufAttrs.topPadding,
               static_cast<const uint32_t*>(imgAttrs[11].value),
               maxPlanes * sizeof(bufAttrs.topPadding[0]));
    }
    if (imgAttrs[12].len != 0) {
        memcpy(bufAttrs.bottomPadding,
               static_cast<const uint32_t*>(imgAttrs[12].value),
               maxPlanes * sizeof(bufAttrs.bottomPadding[0]));
    }
    if (imgAttrs[13].len != 0) {
        bufAttrs.needSwCacheCoherency = *(static_cast<const bool*>(imgAttrs[13].value));
    } else {
        LOG_INFO("Skipped populating needSwCacheCoherency\n");
        bufAttrs.needSwCacheCoherency = false;
    }
    return NVSIPL_STATUS_OK;
}
// Buffer parameter calculation following CFileWriter pattern
SIPLStatus SIPLCoeCamera::GetISPBuffParams(BufferAttrs& bufAttrs,
                                         float **xScale, float **yScale,
                                         uint32_t **bytesPerPixel, uint32_t *numSurfaces,
                                         bool *isPackedYUV)
{
    // Surface parameter tables for different formats
    typedef struct {
        float heightFactor[MAX_NUM_SURFACES];
        float widthFactor[MAX_NUM_SURFACES];
        uint32_t numSurfaces;
    } BufUtilSurfParams;
    static BufUtilSurfParams BufSurfParamsTable_Default = {
        .heightFactor = {1.0f, 0.0f, 0.0f},
        .widthFactor = {1.0f, 0.0f, 0.0f},
        .numSurfaces = 1,
    };
    static BufUtilSurfParams BufSurfParamsTable_YUV[2] = {
        /* YUV 420 */
        {
            .heightFactor = {1.0f, 0.5f, 0.5f},
            .widthFactor = {1.0f, 0.5f, 0.5f},
            .numSurfaces = 3,
        },
        /* YUV 444 */
        {
            .heightFactor = {1.0f, 1.0f, 1.0f},
            .widthFactor = {1.0f, 1.0f, 1.0f},
            .numSurfaces = 3,
        },
    };
    static uint32_t bytesPerPixelTable[6] = {1U, 0U, 0U, 0U, 0U, 0U};
    uint32_t subSamplingType;
    float *xScalePtr = nullptr, *yScalePtr = nullptr;
    uint32_t *bytesPerPixelPtr = nullptr;
    *numSurfaces = 1U;
    *isPackedYUV = false;
    LOG_DBG("Analyzing buffer format...\n");
    LOG_DBG("Plane count: %u\n", bufAttrs.planeCount);
    LOG_DBG("Bits per pixel[0]: %u\n", bufAttrs.planeBitsPerPixels[0]);
    LOG_DBG("Color format[0]: %u\n", static_cast<uint32_t>(bufAttrs.planeColorFormats[0]));
    if ((bufAttrs.planeColorFormats[0] >= NvSciColor_A8Y8U8V8) &&
        (bufAttrs.planeColorFormats[0] <= NvSciColor_A16Y16U16V16)) {
        // YUV PACKED FORMAT
        switch(bufAttrs.planeBitsPerPixels[0]) {
            case 8:
                bytesPerPixelTable[0] = 1U;
                break;
            case 10:
            case 12:
            case 14:
            case 16:
                bytesPerPixelTable[0] = 2U;
                break;
            case 20:
                bytesPerPixelTable[0] = 3U;
                break;
            case 32:
                bytesPerPixelTable[0] = 4U;
                break;
            case 64:
                bytesPerPixelTable[0] = 8U;
                break;
            default:
                LOG_ERR("[GET_PARAMS]  Unsupported bits per pixel for YUV packed: %u\n",
                bufAttrs.planeBitsPerPixels[0]);
                return NVSIPL_STATUS_ERROR;
        }
        bytesPerPixelPtr = &bytesPerPixelTable[0];
        xScalePtr = &BufSurfParamsTable_Default.widthFactor[0];
        yScalePtr = &BufSurfParamsTable_Default.heightFactor[0];
        *numSurfaces = BufSurfParamsTable_Default.numSurfaces;
        *isPackedYUV = true;
    } else if ((1U == bufAttrs.planeCount) &&
               (bufAttrs.planeColorFormats[0] == NvSciColor_Y16)) {
        // LUMA16 FORMAT
        LOG_INFO("Detected LUMA16 format\n");
        bytesPerPixelTable[0] = 2U;
        xScalePtr = &BufSurfParamsTable_Default.widthFactor[0];
        yScalePtr = &BufSurfParamsTable_Default.heightFactor[0];
        *numSurfaces = BufSurfParamsTable_Default.numSurfaces;
        bytesPerPixelPtr = &bytesPerPixelTable[0];
    } else if ((1U == bufAttrs.planeCount) &&
               ((bufAttrs.planeColorFormats[0] == NvSciColor_Float_A16B16G16R16) ||
                ((bufAttrs.planeColorFormats[0] >= NvSciColor_B8G8R8A8) &&
                 (bufAttrs.planeColorFormats[0] <= NvSciColor_A8B8G8R8)))) {
        // RGBA FORMAT
        LOG_INFO("Detected RGBA format\n");
        xScalePtr = &BufSurfParamsTable_Default.widthFactor[0];
        yScalePtr = &BufSurfParamsTable_Default.heightFactor[0];
        *numSurfaces = BufSurfParamsTable_Default.numSurfaces;
        if (bufAttrs.planeColorFormats[0] == NvSciColor_Float_A16B16G16R16) {
            bytesPerPixelTable[0] = 8U;
        } else {
            bytesPerPixelTable[0] = 4U;
        }
        bytesPerPixelPtr = &bytesPerPixelTable[0];
    } else if ((1U == bufAttrs.planeCount) &&
               ((bufAttrs.planeColorFormats[0] < NvSciColor_U8V8) ||
               (bufAttrs.planeColorFormats[0] == NvSciColor_X4Bayer12RGGB_RJ))) {
        // RAW FORMAT
        LOG_INFO("Detected RAW format\n");
        switch(bufAttrs.planeBitsPerPixels[0]) {
            case 8:
                bytesPerPixelTable[0] = 1U;
                break;
            case 10:
            case 12:
            case 14:
            case 16:
                bytesPerPixelTable[0] = 2U;
                break;
            case 32:
                bytesPerPixelTable[0] = 4U;
                break;
            case 64:
                bytesPerPixelTable[0] = 8U;
                break;
            default:
                LOG_ERR("Unsupported bits per pixel for RAW: %u\n", bufAttrs.planeBitsPerPixels[0]);
                return NVSIPL_STATUS_ERROR;
        }
        bytesPerPixelPtr = &bytesPerPixelTable[0];
        xScalePtr = &BufSurfParamsTable_Default.widthFactor[0];
        yScalePtr = &BufSurfParamsTable_Default.heightFactor[0];
        *numSurfaces = BufSurfParamsTable_Default.numSurfaces;
    } else if ((2U == bufAttrs.planeCount) &&
               ((NvSciColor_Y8 == bufAttrs.planeColorFormats[0]) ||
               (NvSciColor_Y10 == bufAttrs.planeColorFormats[0]) ||
               (NvSciColor_Y12 == bufAttrs.planeColorFormats[0]) ||
               (NvSciColor_Y16 == bufAttrs.planeColorFormats[0]))) {
        // YUV SEMI PLANAR FORMAT
        LOG_INFO("Detected YUV SEMI PLANAR format\n");
        if ((bufAttrs.planeHeights[0] == bufAttrs.planeHeights[1]) &&
            (bufAttrs.planeWidths[0] == bufAttrs.planeWidths[1])) {
            subSamplingType = 1; // YUV444
            LOG_INFO("Subsampling: YUV444\n");
        } else if ((bufAttrs.planeHeights[0] == (2U * bufAttrs.planeHeights[1])) &&
                   (bufAttrs.planeWidths[0] == (2U * bufAttrs.planeWidths[1]))) {
            subSamplingType = 0; // YUV420
            LOG_INFO("INFO: Subsampling: YUV420\n");
        } else {
            LOG_ERR("Unsupported YUV channel count/dimensions\n");
            LOG_ERR("Plane 0: %ux%u\n", bufAttrs.planeWidths[0], bufAttrs.planeHeights[0]);
            LOG_ERR("Plane 1: %ux%u\n", bufAttrs.planeWidths[1], bufAttrs.planeHeights[1]);
            return NVSIPL_STATUS_NOT_SUPPORTED;
        }
        xScalePtr = &BufSurfParamsTable_YUV[subSamplingType].widthFactor[0];
        yScalePtr = &BufSurfParamsTable_YUV[subSamplingType].heightFactor[0];
        *numSurfaces = BufSurfParamsTable_YUV[subSamplingType].numSurfaces;
        switch(bufAttrs.planeBitsPerPixels[0]) {
            case 8:
                bytesPerPixelTable[0] = 1U;
                break;
            case 10:
            case 12:
            case 14:
            case 16:
                bytesPerPixelTable[0] = 2U;
                break;
            case 32:
                bytesPerPixelTable[0] = 4U;
                break;
            case 64:
                bytesPerPixelTable[0] = 8U;
                break;
            default:
                LOG_ERR("[GET_PARAMS]  Unsupported bits per pixel for YUV semi-planar: %u\n",
                bufAttrs.planeBitsPerPixels[0]);
                return NVSIPL_STATUS_ERROR;
        }
        bytesPerPixelTable[1] = bytesPerPixelTable[0];
        bytesPerPixelTable[2] = bytesPerPixelTable[0];
        bytesPerPixelPtr = &bytesPerPixelTable[0];
    } else {
        LOG_ERR("[GET_PARAMS]  Unsupported plane format\n");
        LOG_ERR("[GET_PARAMS]    - Plane count: %u\n", bufAttrs.planeCount);
        LOG_ERR("[GET_PARAMS]    - Color format[0]: %u\n", static_cast<uint32_t>(bufAttrs.planeColorFormats[0]));
        return NVSIPL_STATUS_NOT_SUPPORTED;
    }

    if (xScale) {
        *xScale = xScalePtr;
    }
    if (yScale) {
        *yScale = yScalePtr;
    }
    if (bytesPerPixel) {
        *bytesPerPixel = bytesPerPixelPtr;
    }
    LOG_DBG("Buffer parameters calculated: %u surfaces, %s packed YUV, %u bytes per pixel[0]\n",
        *numSurfaces, (*isPackedYUV ? "YES" : "NO"), bytesPerPixelPtr[0]);
    return NVSIPL_STATUS_OK;
}

SIPLStatus SIPLCoeCamera::CoeRegisterImages(uint32_t const numIcpObjects,
                                         uint32_t const numIspObjects,
                                         bool registerISPEOFSyncObj,
                                         uint32_t const customIspObjects[3U])
{
    LOG_DBG("Entry: ICP=%u, ISP=%u, EOF=%s\n", numIcpObjects, numIspObjects, (registerISPEOFSyncObj ? "Y" : "N"));
    SIPLStatus status = NVSIPL_STATUS_OK;
    struct CoeModuleData *module = CoeFirstModule();
    uint32_t count = 0U;
    uint32_t numObjects = 0U;
    // Setup output types array
    INvSIPLClient::ConsumerDesc::OutputType outputs[4U];
    outputs[count++] = INvSIPLClient::ConsumerDesc::OutputType::ICP;
    if (g_coePipelineCfg.isp0OutputRequested) {
        outputs[count++] = INvSIPLClient::ConsumerDesc::OutputType::ISP0;
    }
    if (g_coePipelineCfg.isp1OutputRequested) {
        outputs[count++] = INvSIPLClient::ConsumerDesc::OutputType::ISP1;
    }
    if (g_coePipelineCfg.isp2OutputRequested) {
        outputs[count++] = INvSIPLClient::ConsumerDesc::OutputType::ISP2;
    }

    LOG_DBG("Output types configured: %u\n", count);
    // Count total modules
    uint32_t totalModules = 0;
    struct CoeModuleData *countModule = CoeFirstModule();
    while (countModule != nullptr) {
        totalModules++;
        countModule = CoeNextModule();
    }

    LOG_DBG("Found %u COE modules to process\n", totalModules);
    // Process each module
    uint32_t moduleCount = 0;
    module = CoeFirstModule(); // Reset to first module
    while (module != nullptr) {
        moduleCount++;
        uint32_t uSensor = module->sensorId;
        uint32_t index = module->index;
        LOG_INFO("Processing Module[%u/%u]: %s\n", moduleCount, totalModules, module->name);

        // Register images for each output type
        for (uint32_t i = 0; i < count; i++) {
            // Determine number of objects for this output type
            if (outputs[i] == INvSIPLClient::ConsumerDesc::OutputType::ICP) {
                numObjects = numIcpObjects;
            } else if (outputs[i] == INvSIPLClient::ConsumerDesc::OutputType::ISP0) {
                numObjects = (customIspObjects != nullptr && customIspObjects[0] > 0) ? customIspObjects[0] : numIspObjects;
            } else if (outputs[i] == INvSIPLClient::ConsumerDesc::OutputType::ISP1) {
                numObjects = (customIspObjects != nullptr && customIspObjects[1] > 0) ? customIspObjects[1] : numIspObjects;
            } else if (outputs[i] == INvSIPLClient::ConsumerDesc::OutputType::ISP2) {
                numObjects = (customIspObjects != nullptr && customIspObjects[2] > 0) ? customIspObjects[2] : numIspObjects;
            }
            const char* outputName = (outputs[i] == INvSIPLClient::ConsumerDesc::OutputType::ICP ? "ICP" :
                                     outputs[i] == INvSIPLClient::ConsumerDesc::OutputType::ISP0 ? "ISP0" :
                                     outputs[i] == INvSIPLClient::ConsumerDesc::OutputType::ISP1 ? "ISP1" :
                                     outputs[i] == INvSIPLClient::ConsumerDesc::OutputType::ISP2 ? "ISP2" : "UNKNOWN");
            LOG_INFO("Registering %s with %u objects\n", outputName, numObjects);
            status = CoeRegisterImagesHelper(outputs[i], uSensor, index, numObjects, static_cast<uint8_t>(outputs[i]));
            if (status != NVSIPL_STATUS_OK) {
                LOG_ERR("CoeRegisterImagesHelper failed for %s (sensor %u, status %u)\n",
                    outputName, uSensor, static_cast<uint32_t>(status));
                return status;
            }
        }

        // Register EOF sync objects if ISP outputs are enabled
        LOG_INFO("Checking EOF sync object registration: count=%u, registerISPEOF=%s\n", count, (registerISPEOFSyncObj ? "Y" : "N"));
        if (registerISPEOFSyncObj && count > 1) {
            INvSIPLClient::ConsumerDesc::OutputType ispOutputType = INvSIPLClient::ConsumerDesc::OutputType::ICP;
            for (uint32_t i = 1; i < count; i++) { // Start from 1 to skip ICP
                if (outputs[i] != INvSIPLClient::ConsumerDesc::OutputType::ICP) {
                    ispOutputType = outputs[i];
                    break;
                }
            }
            const char* ispName = (ispOutputType == INvSIPLClient::ConsumerDesc::OutputType::ISP0 ? "ISP0" :
                                  ispOutputType == INvSIPLClient::ConsumerDesc::OutputType::ISP1 ? "ISP1" :
                                  ispOutputType == INvSIPLClient::ConsumerDesc::OutputType::ISP2 ? "ISP2" : "OTHER");
            LOG_INFO("Registering EOF sync object for Module[%u] using %s\n", index, ispName);
            AllocateNvSciSyncObjects(m_sciSyncModule, uSensor, ispOutputType, NVSIPL_EOFSYNCOBJ, m_nvSyncObj[index], 1U);
            status = m_upCamera->RegisterNvSciSyncObj(uSensor, ispOutputType, NVSIPL_EOFSYNCOBJ, m_nvSyncObj[index][0]);
            if (status != NVSIPL_STATUS_OK) {
                LOG_ERR("RegisterNvSciSyncObj failed for sensor %u, output %s, status %u\n",
                    uSensor, ispName, static_cast<uint32_t>(status));
                return status;
            }
        }
        module = CoeNextModule();
    }
    LOG_INFO("All %u modules processed successfully\n", moduleCount);

    return NVSIPL_STATUS_OK;
}

int main(int argc, char *argv[])
{
    SIPLCoeCamera *coeCamera = new SIPLCoeCamera();
    SIPLStatus status = NVSIPL_STATUS_OK;

    CCmdLineParser& cmdline = coeCamera->m_cmdline;
    auto ret = cmdline.Parse(argc, argv);
    if (ret != 0) {
        // No need to print any error, Parse() would have printed error.
        return -1;
    }

    INvSIPLTrace::GetInstance()->SetLevel((INvSIPLTrace::TraceLevel)INvSIPLTrace::LevelInfo);
    CLogger::GetInstance().SetLogLevel((CLogger::LogLevel) cmdline.verbosity);

    LOG_INFO("=== nvsipl_coe_camera ===\n");
    status = coeCamera->SetUp();
    if (status != NVSIPL_STATUS_OK) {
        LOG_ERR("CoeCamera::SetUp failed\n");
        return 1;
    }

    // Display the camera configurations being used
    LOG_DBG("Camera System Configuration:\n");
    LOG_DBG("- Number of cameras: %u\n", coeCamera->m_coeSystemConfig.cameras.size());
    LOG_DBG("- Number of transports: %u\n", coeCamera->m_coeSystemConfig.transports.size());
    for (size_t i = 0; i < coeCamera->m_coeSystemConfig.cameras.size(); i++) {
        const auto& camera = coeCamera->m_coeSystemConfig.cameras[i];
        LOG_DBG("- Camera %u: %s (Platform: %s)\n",
            i, camera.name.c_str(), camera.platform.c_str());
        LOG_DBG("- Sensor ID: %u, Name: %s\n",
            camera.sensorInfo.id, camera.sensorInfo.name.c_str());
        if (std::holds_alternative<nvsipl::CoECamera>(camera.cameratype)) {
            const nvsipl::CoECamera& coeCam = std::get<nvsipl::CoECamera>(camera.cameratype);
            PrintIpAddress(coeCam.sensors[0].ipAddress, "Sensor IP:");
            PrintMacAddress(coeCam.sensors[0].macAddress, "Sensor MAC:");
        }
    }

    // Setup COE camera configuration using new CameraSystemConfig
    LOG_INFO("=== coe setup and init ===\n");
    status = coeCamera->CoeSetupAndInit(coeCamera->m_coeSystemConfig);
    if (status != NVSIPL_STATUS_OK) {
        LOG_ERR("CoeSetupAndInit failed\n");
        goto cleanup_phase;
    }

    // Separate image registration phase like SIPL
    LOG_INFO("=== image registration ===\n");
    status = coeCamera->CoeRegisterImages();
    if (status != NVSIPL_STATUS_OK) {
        LOG_ERR("CoeRegisterImages failed\n");
        goto cleanup_phase;
    }

    // Separate auto control plugin registration like SIPL
    LOG_INFO("\n=== auto control plugin registration ===\n");
    status = coeCamera->CoeRegisterAutoControlPlugin();
    if (status != NVSIPL_STATUS_OK) {
        LOG_ERR("CoeRegisterAutoControlPlugin failed\n");
        goto cleanup_phase;
    }

    // Configure all processing threads
    status = coeCamera->CoeConfigAllThreads();
    if (status != NVSIPL_STATUS_OK) {
        LOG_ERR("CoeConfigAllThreads failed\n");
        goto cleanup_phase;
    }

    // Start streaming on COE camera - operates on ALL sensors
    LOG_INFO("=== start streaming ===\n");
    status = coeCamera->m_upCamera->Start();
    if (status != NVSIPL_STATUS_OK) {
        LOG_ERR("camera Start failed with status: %d\n", status);
        goto cleanup_phase;
    }

    // By default run for 5 seconds unless overridden by command line argument
    LOG_INFO("Running COE streaming for %u seconds...\n", cmdline.uRunDurationSec);
    std::this_thread::sleep_for(std::chrono::seconds(cmdline.uRunDurationSec));

    // Stop streaming for all sensors
    LOG_INFO("=== stop streaming ===\n");
    status = coeCamera->m_upCamera->Stop();
    if (status != NVSIPL_STATUS_OK) {
        LOG_ERR("camera Stop failed with status: %d\n", status);
        goto cleanup_phase;
    }

    if (coeCamera->m_ConfigureAllThreadFailed) {
        LOG_ERR("COE thread configuration failed\n");
    }

cleanup_phase:
    coeCamera->CoeDeinit();
    coeCamera->TearDown();
    LOG_INFO("=== coe camera test completed ===\n");
    delete coeCamera;
    return 0;
}
