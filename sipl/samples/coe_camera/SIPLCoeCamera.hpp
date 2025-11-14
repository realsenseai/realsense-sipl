
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
#ifndef SIPL_COE_CAMERA_HPP
#define SIPL_COE_CAMERA_HPP
#include <memory>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <set>

#include "NvSIPLCamera.hpp"
#include "NvSIPLCameraQuery.hpp"
#include "NvSIPLCameraTypes.hpp"
#include "NvSIPLClient.hpp"

#include "nvscibuf.h"
#include "nvscisync.h"

#include "CUtils.hpp"
#include "CSyncObjManager.hpp"
#include "CCmdLineParser.hpp"
#include "NvSIPLPipelineMgr.hpp" // Pipeline manager

using namespace nvsipl;
// Maximum COE modules per platform
#define MAX_COE_MODULES_PER_PLATFORM 16

#define MAX_NUM_SURFACES (3U)

#define ISP_DUMP_TIMEOUT_US (1000000U)  // 1 second timeout for fence wait
typedef struct {
    NvSciBufType bufType;
    uint64_t size;
    uint32_t planeCount;
    NvSciBufAttrValImageLayoutType layout;
    uint32_t planeWidths[MAX_NUM_SURFACES];
    uint32_t planeHeights[MAX_NUM_SURFACES];
    uint32_t planePitches[MAX_NUM_SURFACES];
    uint32_t planeBitsPerPixels[MAX_NUM_SURFACES];
    uint32_t planeAlignedHeights[MAX_NUM_SURFACES];
    uint64_t planeAlignedSizes[MAX_NUM_SURFACES];
    uint8_t planeChannelCounts[MAX_NUM_SURFACES];
    uint64_t planeOffsets[MAX_NUM_SURFACES];
    uint32_t topPadding[MAX_NUM_SURFACES];
    uint32_t bottomPadding[MAX_NUM_SURFACES];
    bool needSwCacheCoherency;
    NvSciBufAttrValColorFmt planeColorFormats[MAX_NUM_SURFACES];
} BufferAttrs;

/**
 * Test fixture class for SIPL COE integration tests.
 * Uses the new COE Query APIs and CameraSystemConfig.
 */
class SIPLCoeCamera
{
public:
    SIPLCoeCamera() = default;
    ~SIPLCoeCamera() = default;
    SIPLStatus SetUp();
    SIPLStatus TearDown();
    // Thread management enums
    enum ThreadIndex {
        THREAD_INDEX_ICP = 0U,    // Image Capture Pipeline
        THREAD_INDEX_ISP0,        // Image Signal Processing 0
        THREAD_INDEX_ISP1,        // Image Signal Processing 1
        THREAD_INDEX_ISP2,        // Image Signal Processing 2
        THREAD_INDEX_EVENT,       // Event notifications
        THREAD_INDEX_CPUSIGNAL,   // CPU signaling
        THREAD_INDEX_COUNT
    };
    // Thread-specific data structure
    struct ThreadData {
        std::string threadName;
        INvSIPLFrameCompletionQueue *imageQueue;
        INvSIPLNotificationQueue *eventQueue;
    };

    // COE-specific camera configuration using new APIs
    CameraSystemConfig m_coeSystemConfig{};
    std::unique_ptr<INvSIPLCamera> m_upCamera;
    std::unique_ptr<INvSIPLCameraQuery> m_upQuery;
    // Basic initialization and cleanup methods
    SIPLStatus SetupCoeCamera();
    void CleanupCoeCamera();

    // SIPL Query API method with JSON file support
    SIPLStatus GetCoeSystemConfigFromQuery();
    // COE camera setup methods
    SIPLStatus CoeSetPlatformCfg(CameraSystemConfig &cameraSystemConfig);
    SIPLStatus CoeSetPipelineCfg(NvSIPLPipelineConfiguration &pipelineCfg);
    SIPLStatus CoeInit();
    void CoeDeinit();
    SIPLStatus CoeSetupAndInit(CameraSystemConfig &cameraSystemConfig);
    // Image registration methods
    SIPLStatus CoeRegisterImagesHelper(INvSIPLClient::ConsumerDesc::OutputType const outType,
                                      uint32_t const uSensor,
                                      uint32_t const index,
                                      uint32_t const numObjects,
                                      uint32_t const col);
    // Main image registration function like CameraRegisterImages in SIPL CORE IT
    SIPLStatus CoeRegisterImages(uint32_t const numIcpObjects = 4U,
                                uint32_t const numIspObjects = 2U,
                                bool registerISPEOFSyncObj = true,
                                uint32_t const customIspObjects[3U] = nullptr);
    // Auto control plugin registration - NO-OP for COE testing
    SIPLStatus CoeRegisterAutoControlPlugin(const PluginType type = NV_PLUGIN);
    // Thread configuration methods
    SIPLStatus CoeConfigAllThreads();
    bool AllCoeThreadsReady();
    void ExitAllCoeThreads();
    // No additional thread management beyond standard pipeline threads
    // Buffer and Frame Validation Methods
    bool ValidateFrameSequenceNum(uint64_t frameSequenceNumber, uint32_t const cameraModule);
    bool ValidateOutputBuffer(uint32_t const index, uint32_t const threadIndex,
                             INvSIPLClient::INvSIPLNvMBuffer *pNvMBuffer);
    bool ValidateEmbeddedBufSize(uint32_t embeddedBufTopSize, uint32_t embeddedBufBottomSize,
                                SensorInfo &sensorInfo);
    // Metadata Validation Methods
    void CompareMetaData(uint32_t const cameraModule, uint32_t const threadIndex);
    void PrintMetaData(uint32_t const cameraModule, uint32_t const threadIndex, ThreadData *const threadData);
    bool ValidateMetaDataControlInfo(uint32_t const cameraModule, uint32_t const threadIndex);
    bool ValidateMetaDataStatistics(uint32_t const cameraModule, uint32_t const threadIndex);
    // Error Management Methods (same as legacy)
    bool CheckPipelineQueues();

    // Private helper methods for sync object allocation
    void AllocateNvSciSyncObjects(NvSciSyncModule &sciSyncModule, const uint32_t uSensor,
                                  INvSIPLClient::ConsumerDesc::OutputType OutputType,
                                  NvSiplNvSciSyncObjType fenceType, std::vector<NvSciSyncObj> &nvSyncObj,
                                  uint32_t numFences = 0U);

    // Thread functions (public so static helpers can access them)
    void CoeImageThread(uint32_t const cameraModule, uint32_t const threadIndex);
    void CoeEventThread(uint32_t const cameraModule, uint32_t const threadIndex);
    void CoeCpuSignalThread(uint32_t const cameraModule, uint32_t const threadIndex);
    // ISP buffer dumping functionality (SIPL OnFrameAvailable pattern)
    SIPLStatus WriteISPBufferToFile(INvSIPLClient::INvSIPLNvMBuffer *pNvmBuf, uint32_t frameNum, uint32_t cameraModule, uint32_t threadIndex);
    // Buffer utility functions
    SIPLStatus PopulateBufAttr(const NvSciBufObj& sciBufObj, BufferAttrs &bufAttrs);
    SIPLStatus GetISPBuffParams(BufferAttrs& bufAttrs,
                               float **xScale, float **yScale,
                               uint32_t **bytesPerPixel, uint32_t *numSurfaces,
                               bool *isPackedYUV);

    // COE utilities - simplified for compilation
    struct CoeModuleData {
        uint32_t sensorId;
        uint32_t index;
        std::string name;
        std::string platform;
        std::string sensorName;
    };
    struct CoeModuleData *CoeFirstModule();
    struct CoeModuleData *CoeNextModule();
    struct CoeModuleData *CoeGetModule();
    // Member variables for COE support
    NvSIPLPipelineConfiguration m_coePipelineCfg;
    std::vector<CoeModuleData> m_coeModules;
    uint32_t m_coeModuleIndex = 0;

    ThreadData m_threadData[MAX_COE_MODULES_PER_PLATFORM][THREAD_INDEX_COUNT];
    std::thread m_threads[MAX_COE_MODULES_PER_PLATFORM][THREAD_INDEX_COUNT];
    std::atomic<bool> m_threadReady[MAX_COE_MODULES_PER_PLATFORM][THREAD_INDEX_COUNT];
    std::atomic<bool> m_exitAllThreads;

    std::atomic<bool> m_ConfigureAllThreadFailed;
    // Queue tracking
    uint32_t m_queueCounts[MAX_COE_MODULES_PER_PLATFORM][THREAD_INDEX_COUNT];

    uint32_t m_icpProcessingDoneEventCounts[MAX_COE_MODULES_PER_PLATFORM];
    uint32_t m_authenticationSuccessEventCounts[MAX_COE_MODULES_PER_PLATFORM];
    uint32_t m_authenticationFailureEventCounts[MAX_COE_MODULES_PER_PLATFORM];
    uint32_t m_authenticationOutOfOrderEventCounts[MAX_COE_MODULES_PER_PLATFORM];
    // Frame and Metadata Validation
    uint32_t m_compareMetaDataFailCount[MAX_COE_MODULES_PER_PLATFORM];
    uint32_t m_capturedFrameCounts[MAX_COE_MODULES_PER_PLATFORM][THREAD_INDEX_COUNT];
    bool m_isPrevFrameNumberValid[MAX_COE_MODULES_PER_PLATFORM];
    uint64_t m_prevFrameNumber[MAX_COE_MODULES_PER_PLATFORM];
    uint64_t m_firstFrameTimestamp;
    std::array<uint64_t, MAX_COE_MODULES_PER_PLATFORM> m_firstFrameCaptureTSC;
    // NvSci modules
    NvSciBufModule m_sciBufModule = nullptr;
    NvSciSyncModule m_sciSyncModule = nullptr;

    NvSciSyncCpuWaitContext m_cpuWaitContext = nullptr;

    std::vector<NvSciSyncObj> m_allocatedSyncObjects;

    std::unique_ptr<CSyncObjManager> m_syncObjManager[MAX_COE_MODULES_PER_PLATFORM][4]; // [module][outputType]

    std::vector<NvSciSyncObj> m_nvSyncObj[MAX_COE_MODULES_PER_PLATFORM]; // Per-module sync objects

    // Pipeline queues for each module
    NvSIPLPipelineQueues m_queues[MAX_COE_MODULES_PER_PLATFORM];
    // Auto control plugins
    std::unique_ptr<ISiplControlAuto> m_upCustomPlugins[MAX_COE_MODULES_PER_PLATFORM];

    class CoeBufObjManager
    {
    public:
        std::vector<NvSciBufObj> m_sciBufObjs;
        CoeBufObjManager(NvSciBufModule sciBufModule) : m_sciBufModule(sciBufModule) { }
        SIPLStatus AllocateBufObjs(INvSIPLCamera *siplCamera,
                                   uint32_t uSensor,
                                   INvSIPLClient::ConsumerDesc::OutputType output,
                                   uint32_t numObjects);
        ~CoeBufObjManager();
    private:
        NvSciBufModule m_sciBufModule {};
    };
    std::unique_ptr<CoeBufObjManager> m_bufObjManager[MAX_COE_MODULES_PER_PLATFORM][4]; // [module][outputType]

    bool m_bInitialized = false;
    // Test validation flags
    bool m_verifyImage = true;
    bool m_verifyNotification = false;
    bool m_showMetadata = false;
    bool m_compareMetadata = false;
    bool m_compareMetadataFailed = false;
    bool m_verifyEmbeddedLineInfo = false;
    bool m_checkEmbInfo = false;
    bool m_validateControlInfo = false;
    bool m_validateStatisticsInfo = false;
    bool m_validateContinuousCapture = false;
    bool m_validateFrameDropEvent = false;
    bool m_verifyFrameDrop = false;
    bool m_verifyCaptureFailed = false;

    bool m_acpProcessErrorNotifReceived[MAX_COE_MODULES_PER_PLATFORM] = {false};
    bool m_acpSettDiscontNotifReceived[MAX_COE_MODULES_PER_PLATFORM] = {false};
    bool m_icpParseEmbDataErrorNotifReceived[MAX_COE_MODULES_PER_PLATFORM] = {false};
    bool m_ispProcessErrorNotifReceived = false;

    INvSIPLClient::ImageMetaData m_metadata[MAX_COE_MODULES_PER_PLATFORM][THREAD_INDEX_COUNT];

    // Command line parser
    CCmdLineParser m_cmdline;

    // Dump tracking variables
    uint32_t m_dumpedFrameCount[MAX_COE_MODULES_PER_PLATFORM][THREAD_INDEX_COUNT] = {{0}};
    bool m_dumpingEnabled = false;
    uint32_t m_dumpCount = 0;
    void OnEvent(NvSIPLPipelineNotifier::NotificationData &oNotificationData);
    uint32_t m_uNumFrameCaptured;
    uint32_t m_uNumFrameDrops;
    uint32_t m_uNumFrameDiscontinuities;
    uint32_t m_uNumFailedAuthentications;
    uint32_t m_uNumOutOfOrderAuthentications;
    bool m_bInError;
};
#endif // SIPL_COE_CAMERA_HPP
