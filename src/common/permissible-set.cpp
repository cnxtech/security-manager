/*
 *  Copyright (c) 2016 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Contact: Rafal Krypa <r.krypa@samsung.com>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License
 */
/*
 * @file        permissible-set.cpp
 * @author      Rafal Krypa <r.krypa@samsung.com>
 * @author      Radoslaw Bartosiak <r.bartosiak@samsung.com>
 * @version     1.0
 * @brief       Implementation of API for adding, deleting and reading permissible names
 * @brief       (names of installed applications)
 */
#ifndef _GNU_SOURCE //for TEMP_FAILURE_RETRY
#define _GNU_SOURCE
#endif

#include <cstdio>
#include <cstring>
#include <fstream>
#include <linux/xattr.h>
#include <memory>
#include <pwd.h>
#include <string>
#include <sys/file.h>
#include <sys/smack.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <config.h>
#include <dpl/errno_string.h>
#include <dpl/exception.h>
#include <dpl/fstream_accessors.h>
#include <dpl/log/log.h>
#include <filesystem.h>
#include <permissible-set.h>
#include <privilege_db.h>
#include <security-manager-types.h>
#include <smack-labels.h>
#include <tzplatform_config.h>

#include "tzplatform-config.h"

namespace SecurityManager {
namespace PermissibleSet {

template <typename T>
static inline int getFd(T &fstream)
{
    return DPL::FstreamAccessors<T>::GetFd(fstream);
}

template <typename T>
static void openAndLockNameFile(const std::string &nameFile, T &fstream)
{
    fstream.open(nameFile);
    if (!fstream.is_open()) {
        LogError("Unable to open file" << nameFile << ": " << GetErrnoString(errno));
        ThrowMsg(PermissibleSetException::FileOpenError, "Unable to open file ");
    }

    int ret = TEMP_FAILURE_RETRY(flock(getFd(fstream), LOCK_EX));
    if (ret == -1) {
        LogError("Unable to lock file " << nameFile << ": " << GetErrnoString(errno));
        ThrowMsg(PermissibleSetException::FileLockError, "Unable to lock file");
    }
}

std::string getPerrmissibleFileLocation(uid_t uid, int installationType)
{
    TizenPlatformConfig tpc(uid);
    if ((installationType == SM_APP_INSTALL_GLOBAL)
            || (installationType == SM_APP_INSTALL_PRELOADED))
        return tpc.ctxMakePath(TZ_SYS_VAR, Config::SERVICE_NAME,
            Config::APPS_LABELS_FILE);
    else
        return tpc.ctxMakePath(TZ_SYS_VAR, Config::SERVICE_NAME,
            tpc.ctxGetEnv(TZ_USER_NAME), Config::APPS_LABELS_FILE);
}

static void markPermissibleFileValid(int fd, const std::string &nameFile, bool valid)
{
    int ret;
    if (valid)
        ret = TEMP_FAILURE_RETRY(fchmod(fd, 00444));
    else
        ret = TEMP_FAILURE_RETRY(fchmod(fd, 00000));
    if (ret == -1) {
        LogError("Error at fchmod " << nameFile << ": " << GetErrnoString(errno));
        ThrowMsg(PermissibleSetException::FileWriteError, "Error at fchmod");
    }
}

void updatePermissibleFile(uid_t uid, int installationType,
                           const std::vector<std::string> &labelsForUser)
{
    std::string nameFile = getPerrmissibleFileLocation(uid, installationType);
    std::ofstream fstream;
    openAndLockNameFile(nameFile, fstream);
    markPermissibleFileValid(getFd(fstream), nameFile, false);

    for (auto &label : labelsForUser) {
        fstream << label << '\n';
        if (fstream.bad()) {
            LogError("Unable to write to file " << nameFile << ": " << GetErrnoString(errno));
            ThrowMsg(PermissibleSetException::PermissibleSetException::FileWriteError,
                    "Unable to write to file");
        }
    }
    if (fstream.flush().fail()) {
        LogError("Error at fflush " << nameFile << ": " << GetErrnoString(errno));
        ThrowMsg(PermissibleSetException::FileWriteError, "Error at fflush");
    }
    if (fsync(getFd(fstream)) == -1) {
        LogError("Error at fsync " << nameFile << ": " << GetErrnoString(errno));
        ThrowMsg(PermissibleSetException::FileWriteError, "Error at fsync");
    }
    markPermissibleFileValid(getFd(fstream), nameFile, true);
}

void readLabelsFromPermissibleFile(const std::string &nameFile, std::vector<std::string> &appLabels)
{
    std::ifstream fstream;
    openAndLockNameFile(nameFile, fstream);

    std::string line;
    while (std::getline(fstream, line))
        appLabels.push_back(line);

    if (fstream.bad()) {
        LogError("Failure while reading file " << nameFile << ": " << GetErrnoString(errno));
        ThrowMsg(PermissibleSetException::FileReadError, "Failure while reading file");
    }
}

void initializeUserPermissibleFile(uid_t uid)
{
    std::string nameFile = getPerrmissibleFileLocation(uid, SM_APP_INSTALL_LOCAL);
    std::string nameDir = FS::dirName(nameFile);

    if (mkdir(nameDir.c_str(), 0755) != 0 && errno != EEXIST)
        ThrowMsg(PermissibleSetException::FileInitError,
            "Unable to create directory for user permissible file:" << GetErrnoString(errno));

    std::ofstream fstream;
    openAndLockNameFile(nameFile, fstream);
    SmackLabels::setSmackLabelForFd(getFd(fstream), "_");

    markPermissibleFileValid(getFd(fstream), nameFile, true);
}

void removeUserPermissibleFile(uid_t uid)
{
    std::string nameFile = getPerrmissibleFileLocation(uid, SM_APP_INSTALL_LOCAL);
    std::string nameDir = FS::dirName(nameFile);

    if (unlink(nameFile.c_str()) != 0 && errno != ENOENT)
        ThrowMsg(PermissibleSetException::FileRemoveError,
            "Unable to remove user permissible file:" << GetErrnoString(errno));

    if (rmdir(nameFile.c_str()) != 0 && errno != ENOENT)
        ThrowMsg(PermissibleSetException::FileRemoveError,
            "Unable to remove directory for user permissible file:" << GetErrnoString(errno));
}

} // PermissibleSet
} // SecurityManager
