/*
 * SPDX-FileCopyrightText: Copyright (c) 2018-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

/* STL Headers */
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <iostream>
#include <getopt.h>
#include <vector>
#include <iomanip>

#include "NvSIPLTrace.hpp" // NvSIPLTrace to set library trace level
#include "NvSIPLQuery.hpp" // NvSIPLQuery to display platform config

#ifndef CCMDPARSER_HPP
#define CCMDPARSER_HPP

using namespace std;
using namespace nvsipl;

class CCmdLineParser
{
 public:
    // Command line options
    int32_t uRunDurationSec = 5;
    string sConfigName = "";
    string sTestConfigFile = "";
    bool bEnableRaw = false;
    bool bDisableISP0 = false;
    bool bDisableISP1 = false;
    bool bDisableISP2 = false;
    string sFiledumpPrefix = "";
    uint64_t uNumWriteFrames = 0;
    uint32_t verbosity = 1u;
    string sNitoFolderPath = "";
    string sCoEOverridePath = "";

    static void ShowConfigs()
    {
        auto pQuery = INvSIPLCameraQuery::GetInstance();
        if (pQuery == nullptr) {
            cout << "INvSIPLCameraQuery::GetInstance() failed\n";
        }

        auto status = pQuery->ParseDatabase();
        if (status != NVSIPL_STATUS_OK) {
            LOG_ERR("INvSIPLCameraQuery::ParseDatabase failed\n");
        }

        for (auto &cfg : pQuery->GetCameraConfigNames()) {
            cout << "\t" << std::setw(35) << std::left << cfg << endl;
        }
    }

    static void ShowUsage(void)
    {
        cout << "Usage:\n";
        cout << "-h or --help                               :Prints this help\n";
        cout << "-r or --runfor <seconds>                   :Exit application after n seconds (default is 5s)\n";
        cout << "-c or --platform-config 'name'             :Platform configuration. Supported values\n";
        ShowConfigs();
        cout << "-t or --test-config-file <file>            :Set custom platform config json file\n";
        cout << "-R or --enableRawOutput                    :enable the Raw output(default is disabled)\n";
        cout << "-0 or --disableISP0Output                  :Disable the ISP0 output(default is enabled)\n";
        cout << "-1 or --disableISP1Output                  :Disable the ISP1 output(default is enabled)\n";
        cout << "-2 or --disableISP2Output                  :Disable the ISP2 output(default is enabled)\n";
        cout << "-W or --writeFrames <val>                  :Number of frames to write to file (default is 0)\n";
        cout << "-f or --filedump-prefix 'str'              :Dump file with filename prefix 'str' when output is enabled.\n";
        cout << "                                           :Eg: '-f coe' when RAW output is enabled(coe_sensor0_raw_frame_1.raw).\n";
        cout << "                                           :Eg: '-f coe' when ISP0 output is enabled(coe_sensor0_ISP0_frame_1.yuv).\n";
        cout << "                                           :Eg: '-f coe' when ISP1 output is enabled(coe_sensor0_ISP1_frame_1.yuv).\n";
        cout << "                                           :Eg: '-f coe' when ISP2 output is enabled(coe_sensor0_ISP2_frame_1.yuv420).\n";
        cout << "-v or --verbosity <val>                    :Verbosity level (default is 1)\n";
        cout << "-N or --nito <folder>                      :Path to folder containing NITO files\n";
        cout << "                                           :Order of preference: NITO_PATH environment variable, -N argument, default path\n";
        cout << "                                           :default path: /var/nvidia/nvcam/settings/sipl/\n";
        cout << "                                           :Eg: '-N /home/ubuntu/vb1940.nito'\n";
        cout << "--coeConfigOverridePath <file>             :Path to CoE config override file\n";
        return;
    }

    int Parse(int argc, char* argv[])
    {
        const char *const short_options = "hc:r:t:R012W:f:v:N:";
        const struct option long_options[] =
        {
            { "help",                     no_argument,       0, 'h' },
            { "runfor",                   required_argument, 0, 'r' },
            { "platform-config",          required_argument, 0, 'c' },
            { "test-config-file",         required_argument, 0, 't' },
            { "enableRawOutput",          no_argument,       0, 'R' },
            { "disableISP0Output",        no_argument,       0, '0' },
            { "disableISP1Output",        no_argument,       0, '1' },
            { "disableISP2Output",        no_argument,       0, '2' },
            { "writeFrames",              required_argument, 0, 'W' },
            { "filedump-prefix",          required_argument, 0, 'f' },
            { "verbosity",                required_argument, 0, 'v' },
            { "nito",                     required_argument, 0, 'N' },
            { "coeConfigOverridePath",    required_argument, 0,  0  },
            { 0,                          0,                 0,  0  }
        };

        int index = 0;
        auto bShowHelp = false;

        while (1) {
            const auto getopt_ret = getopt_long(argc, argv, short_options , &long_options[0], &index);
            if (getopt_ret == -1) {
                // Done parsing all arguments.
                break;
            }

            switch (getopt_ret) {
            default: /* Unrecognized option */
            case '?': /* Unrecognized option */
                cout << "Invalid or Unrecognized command line option. Specify -h or --help for options\n";
                bShowHelp = true;
                break;
            case 'h': /* -h or --help */
                bShowHelp = true;
                break;
            case 'r':
                uRunDurationSec = atoi(optarg);
                break;
            case 'c':
                sConfigName = string(optarg);
                break;
            case 't':
                sTestConfigFile = string(optarg);
                break;
            case 'R':
                bEnableRaw = true;
                break;
            case '0':
                bDisableISP0 = true;
                break;
            case '1':
                bDisableISP1 = true;
                break;
            case '2':
                bDisableISP2 = true;
                break;
            case 'f':
                sFiledumpPrefix = string(optarg);
                break;
            case 'W':
                uNumWriteFrames = atoi(optarg);
                break;
            case 'v':
                verbosity = atoi(optarg);
                break;
            case 'N':
                sNitoFolderPath = string(optarg);
                break;
            case 0:
                if (strcmp(long_options[index].name, "coeConfigOverridePath") == 0) {
                    sCoEOverridePath = string(optarg);
                }
                break;
            }
        }

        if (argc == 1) {
            bShowHelp = true;
        }

        if (bShowHelp) {
            ShowUsage();
            return -1;
        }

        return 0;
    }

    void PrintArgs() const
    {
        if (uRunDurationSec != -1) {
            cout << "Running for " << uRunDurationSec << " seconds\n";
        }
        if (bEnableRaw) {
            cout << "Raw output: enabled" << endl;
        } else {
            cout << "Raw output: disabled" << endl;
        }
        if (bDisableISP0) {
            cout << "ISP0 output: disabled" << endl;
        } else {
            cout << "ISP0 output: enabled" << endl;
        }
        if (bDisableISP1) {
            cout << "ISP1 output: disabled" << endl;
        } else {
            cout << "ISP1 output: enabled" << endl;
        }
        if (bDisableISP2) {
            cout << "ISP2 output: disabled" << endl;
        } else {
            cout << "ISP2 output: enabled" << endl;
        }
        if (sFiledumpPrefix != "") {
            cout << "File dump prefix: " << sFiledumpPrefix << endl;
        }
        if (uNumWriteFrames != 0u) {
            cout << "Number of frames to write to file: " << uNumWriteFrames << endl;
        }
        if (sNitoFolderPath != "") {
            cout << "NITO folder path: " << sNitoFolderPath << endl;
        }
        if (sCoEOverridePath != "") {
            cout << "CoE override path: " << sCoEOverridePath << endl;
        }
    }
};

#endif //CCMDPARSER_HPP
