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

#ifndef COE_CSYNCOBJMANAGER_HPP
#define COE_CSYNCOBJMANAGER_HPP

// Standard header files
#include <memory>
#include <vector>

// SIPL header files
#include "NvSIPLCommon.hpp"
#include "NvSIPLCamera.hpp"

// COE utilities
#include "CUtils.hpp"

// Other NVIDIA header files
#include "nvscisync.h"
#include "nvmedia_core.h"

using namespace nvsipl;

struct CloseNvSciSyncAttrList
{
    void operator()(NvSciSyncAttrList *attrList) const {
        if (attrList != nullptr) {
            if ((*attrList) != nullptr) {
                NvSciSyncAttrListFree(*attrList);
            }
            delete attrList;
        }
    }
};

/**
 * COE-specific NvSciSync Object Manager
 * Handles allocation and management of NvSciSync objects for COE cameras
 */
class CSyncObjManager
{
public:
    std::vector<NvSciSyncObj> m_sciSyncObjs;

    CSyncObjManager(NvSciSyncModule sciSyncModule) :
        m_sciSyncModule(sciSyncModule)
    {
        LOG_INFO("CSyncObjManager created\n");
    }

    /**
     * Allocate NvSciSync objects for COE camera pipeline
     * @param siplCamera - SIPL camera instance
     * @param uSensor - Sensor ID
     * @param output - Output type (ICP, ISP0, ISP1, ISP2)
     * @param clientType - Client type (SIPL_SIGNALER or SIPL_WAITER)
     * @param numObjects - Number of sync objects to allocate
     */
    SIPLStatus AllocateSyncObjs(
                INvSIPLCamera *siplCamera,
                uint32_t uSensor,
                INvSIPLClient::ConsumerDesc::OutputType output,
                NvSiplNvSciSyncClientType clientType,
                uint32_t numObjects)
    {
        LOG_DBG("AllocateSyncObjs() START\n");
        LOG_DBG("    - Sensor: %u\n", uSensor);
        LOG_DBG("    - Output: %d\n", static_cast<int>(output));
        LOG_DBG("    - ClientType: %d\n", static_cast<int>(clientType));
        LOG_DBG("    - NumObjects: %u\n", numObjects);

        std::unique_ptr<NvSciSyncAttrList, CloseNvSciSyncAttrList> attrList[2];
        attrList[0].reset(new NvSciSyncAttrList());
        NvSciError err = NvSciSyncAttrListCreate(m_sciSyncModule, attrList[0].get());
        CHK_NVSCISTATUS_AND_RETURN(err, "NvSciSyncAttrListCreate() for CPU attrs");

        attrList[1].reset(new NvSciSyncAttrList());
        err = NvSciSyncAttrListCreate(m_sciSyncModule, attrList[1].get());
        CHK_NVSCISTATUS_AND_RETURN(err, "NvSciSyncAttrListCreate() for SIPL attrs");

        LOG_DBG("Created NvSciSync attribute lists successfully\n");

        bool isCpuAcccessReq = true;
        NvSciSyncAccessPerm cpuPerm = (clientType == SIPL_SIGNALER) ?
                                            NvSciSyncAccessPerm_WaitOnly :
                                            NvSciSyncAccessPerm_SignalOnly;
        NvSciSyncAttrKeyValuePair attrKvp[] = {
            { NvSciSyncAttrKey_NeedCpuAccess, &isCpuAcccessReq, sizeof(isCpuAcccessReq) },
            { NvSciSyncAttrKey_RequiredPerm, &cpuPerm, sizeof(cpuPerm) },
        };

        err = NvSciSyncAttrListSetAttrs(*(attrList[0].get()), attrKvp, sizeof(attrKvp) / sizeof(attrKvp[0]));
        CHK_NVSCISTATUS_AND_RETURN(err, "NvSciSyncAttrListSetAttrs() for CPU attrs");

        LOG_DBG("Set CPU sync attributes successfully\n");

        SIPLStatus status = siplCamera->FillNvSciSyncAttrList(uSensor, output, *(attrList[1].get()), clientType);
        CHK_STATUS_AND_RETURN(status, "INvSIPLCamera::FillNvSciSyncAttrList()");

        LOG_DBG("SIPL filled sync attributes successfully\n");

        std::unique_ptr<NvSciSyncAttrList, CloseNvSciSyncAttrList> reconciledAttrList;
        std::unique_ptr<NvSciSyncAttrList, CloseNvSciSyncAttrList> conflictAttrList;
        reconciledAttrList.reset(new NvSciSyncAttrList());
        conflictAttrList.reset(new NvSciSyncAttrList());

        NvSciSyncAttrList unreconciledList[2];
        unreconciledList[0] = *(attrList[0].get());
        unreconciledList[1] = *(attrList[1].get());
        err = NvSciSyncAttrListReconcile(unreconciledList,
                                        2U,
                                        reconciledAttrList.get(),
                                        conflictAttrList.get());
        CHK_NVSCISTATUS_AND_RETURN(err, "NvSciSyncAttrListReconcile()");

        LOG_DBG("Reconciled sync attributes successfully\n");

        for (size_t i = 0U; i < numObjects; i++) {
            NvSciSyncObj syncObj {};
            err = NvSciSyncObjAlloc(*(reconciledAttrList.get()), &syncObj);
            CHK_NVSCISTATUS_AND_RETURN(err, "NvSciSyncObjAlloc()");
            CHK_PTR_AND_RETURN(syncObj, "NvSciSyncObjAlloc()");

            m_sciSyncObjs.push_back(syncObj);
            LOG_DBG("Allocated sync object %u successfully\n", i);
        }

        LOG_DBG("AllocateSyncObjs() COMPLETED - Allocated %u sync objects\n", numObjects);
        return NVSIPL_STATUS_OK;
    };

    ~CSyncObjManager()
    {
        LOG_DBG("CSyncObjManager destructor - cleaning up %u sync objects\n", m_sciSyncObjs.size());

        for (uint32_t i = 0U; i < m_sciSyncObjs.size(); i++) {
            if (m_sciSyncObjs[i] != nullptr) {
                NvSciSyncObjFree(m_sciSyncObjs[i]);
                LOG_DBG("Freed sync object %u\n", i);
            }
        }
        // Swap vector with empty vector to force deallocation
        std::vector<NvSciSyncObj>().swap(m_sciSyncObjs);

        LOG_DBG("CSyncObjManager destructor completed\n");
    };

private:
    NvSciSyncModule m_sciSyncModule {};
};

#endif // COE_CSYNCOBJMANAGER_HPP