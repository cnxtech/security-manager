/*
 *  Copyright (c) 2014-2016 Samsung Electronics Co., Ltd All Rights Reserved
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
/**
 * @file        smack-rules.cpp
 * @author      Jacek Bukarewicz <j.bukarewicz@samsung.com>
 * @version     1.0
 * @brief       Implementation of a class managing smack rules
 *
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/smack.h>
#include <fcntl.h>
#include <fstream>
#include <cstring>
#include <sstream>
#include <string>
#include <memory>
#include <algorithm>

#include "dpl/log/log.h"
#include "dpl/errno_string.h"
#include "dpl/fstream_accessors.h"
#include "filesystem.h"
#include "smack-labels.h"
#include "tzplatform-config.h"

#include "smack-check.h"
#include "smack-rules.h"

namespace SecurityManager {

const std::string SMACK_PROCESS_LABEL_TEMPLATE     = "~PROCESS~";
const std::string SMACK_PATH_RW_LABEL_TEMPLATE     = "~PATH_RW~";
const std::string SMACK_PATH_RO_LABEL_TEMPLATE     = "~PATH_RO~";
const std::string SMACK_PATH_SHARED_RO_LABEL_TEMPLATE     = "~PATH_SHARED_RO~";
const std::string SMACK_PATH_TRUSTED_LABEL_TEMPLATE  = "~PATH_TRUSTED~";
const std::string APP_RULES_TEMPLATE_FILE_PATH = TizenPlatformConfig::makePath(TZ_SYS_RO_SHARE, "security-manager", "policy", "app-rules-template.smack");
const std::string PKG_RULES_TEMPLATE_FILE_PATH = TizenPlatformConfig::makePath(TZ_SYS_RO_SHARE, "security-manager", "policy", "pkg-rules-template.smack");
const std::string AUTHOR_RULES_TEMPLATE_FILE_PATH = TizenPlatformConfig::makePath(TZ_SYS_RO_SHARE, "security-manager", "policy", "author-rules-template.smack");
const std::string SHAREDRO_RULES_TEMPLATE_FILE_PATH = TizenPlatformConfig::makePath(TZ_SYS_RO_SHARE, "security-manager", "policy", "sharedro-rules-template.smack");
const std::string SMACK_RULES_PATH_MERGED      = LOCAL_STATE_DIR "/security-manager/rules-merged/rules.merged";
const std::string SMACK_RULES_PATH_MERGED_T    = LOCAL_STATE_DIR "/security-manager/rules-merged/rules.merged.temp";
const std::string SMACK_RULES_PATH             = LOCAL_STATE_DIR "/security-manager/rules";
const std::string SMACK_RULES_SHARED_RO_PATH   = LOCAL_STATE_DIR "/security-manager/rules/shared_ro";
const std::string SMACK_APP_IN_PACKAGE_PERMS   = "rwxat";
const std::string SMACK_APP_CROSS_PKG_PERMS    = "rxl";
const std::string SMACK_APP_PATH_OWNER_PERMS = "rwxat";
const std::string SMACK_APP_PATH_TARGET_PERMS = "rxl";
const std::string SMACK_APP_DIR_TARGET_PERMS = "x";
const std::string SMACK_USER = "User";
const std::string SMACK_SYSTEM = "System";
const std::string SMACK_SYSTEM_PRIVILEGED = "System::Privileged";
const std::string SMACK_APP_PATH_SYSTEM_PERMS = "rwxat";
const std::string SMACK_APP_PATH_USER_PERMS = "rwxat";
const std::string TEMPORARY_FILE_SUFFIX = ".temp";

SmackRules::SmackRules()
{
    if (smack_accesses_new(&m_handle) < 0) {
        LogError("Failed to create smack_accesses handle");
        throw std::bad_alloc();
    }
}

SmackRules::~SmackRules() {
    smack_accesses_free(m_handle);
}

void SmackRules::add(const std::string &subject, const std::string &object,
        const std::string &permissions)
{
    if (smack_accesses_add(m_handle, subject.c_str(), object.c_str(), permissions.c_str()))
        ThrowMsg(SmackException::LibsmackError, "smack_accesses_add");
}

void SmackRules::addModify(const std::string &subject, const std::string &object,
        const std::string &allowPermissions, const std::string &denyPermissions)
{
    if (smack_accesses_add_modify(m_handle, subject.c_str(), object.c_str(), allowPermissions.c_str(), denyPermissions.c_str()))
        ThrowMsg(SmackException::LibsmackError, "smack_accesses_add_modify");
}

void SmackRules::clear() const
{
    if (smack_accesses_clear(m_handle))
        ThrowMsg(SmackException::LibsmackError, "smack_accesses_clear");
}

void SmackRules::apply() const
{
    if (smack_accesses_apply(m_handle))
        ThrowMsg(SmackException::LibsmackError, "smack_accesses_apply");

}

void SmackRules::loadFromFile(const std::string &path)
{
    int fd;

    fd = TEMP_FAILURE_RETRY(open(path.c_str(), O_RDONLY));
    if (fd == -1) {
        LogError("Failed to open file: " << path);
        ThrowMsg(SmackException::FileError, "Failed to open file: " << path);
    }

    if (smack_accesses_add_from_file(m_handle, fd)) {
        LogError("Failed to load smack rules from file: " << path);
        ThrowMsg(SmackException::LibsmackError, "Failed to load smack rules from file: " << path);
    }

    if (close(fd) == -1) {
        // don't change the return code, the descriptor should be closed despite the error.
        LogWarning("Error while closing the file: " << path << ", error: " << GetErrnoString(errno));
    }
}

void SmackRules::saveToFile(const std::string &destPath) const
{
    int fd;
    int flags = O_CREAT | O_WRONLY | O_TRUNC;
    std::string path = destPath + TEMPORARY_FILE_SUFFIX;

    fd = TEMP_FAILURE_RETRY(open(path.c_str(), flags, 0644));
    if (fd == -1) {
        LogError("Failed to create file: " << path);
        ThrowMsg(SmackException::FileError, "Failed to create file: " << path);
    }

    if (smack_accesses_save(m_handle, fd)) {
        LogError("Failed to save rules to file: " << path);
        unlink(path.c_str());
        ThrowMsg(SmackException::LibsmackError, "Failed to save rules to file: " << path);
    }

    if (close(fd) == -1) {
        if (errno == EIO) {
            LogError("I/O Error occured while closing the file: " << path << ", error: " << GetErrnoString(errno));
            unlink(path.c_str());
            ThrowMsg(SmackException::FileError, "I/O Error occured while closing the file: " << path << ", error: " << GetErrnoString(errno));
        } else {
            // non critical error
            // don't change the return code, the descriptor should be closed despite the error.
            LogWarning("Error while closing the file: " << path << ", error: " << GetErrnoString(errno));
        }
    }

    if (0 > rename(path.c_str(), destPath.c_str())) {
        LogError("Error moving file " << path << " to " << destPath << ". Errno: " << GetErrnoString(errno));
        unlink(path.c_str());
        ThrowMsg(SmackException::FileError, "Error moving file " << path << " to " << destPath << ". Errno: " << GetErrnoString(errno));
    }
}

void SmackRules::addFromTemplateFile(
        const std::string &templatePath,
        const std::string &appProcessLabel,
        const std::string &pkgName,
        const int authorId)
{
    RuleVector templateRules;
    std::string line;
    std::ifstream templateRulesFile(templatePath);

    if (!templateRulesFile.is_open()) {
        LogError("Cannot open rules template file: " << templatePath);
        ThrowMsg(SmackException::FileError, "Cannot open rules template file: " << templatePath);
    }

    while (std::getline(templateRulesFile, line))
        if (!line.empty())
            templateRules.push_back(line);

    if (templateRulesFile.bad()) {
        LogError("Error reading template file: " << templatePath);
        ThrowMsg(SmackException::FileError, "Error reading template file: " << templatePath);
    }

    addFromTemplate(templateRules, appProcessLabel, pkgName, authorId);
}

void SmackRules::addFromTemplate(
        const RuleVector &templateRules,
        const std::string &appProcessLabel,
        const std::string &pkgName,
        const int authorId)
{
    std::string pathRWLabel, pathROLabel, pathSharedROLabel;
    std::string pathTrustedLabel;

    if (!pkgName.empty()) {
        pathRWLabel = SmackLabels::generatePathRWLabel(pkgName);
        pathROLabel = SmackLabels::generatePathROLabel(pkgName);
        pathSharedROLabel = SmackLabels::generatePathSharedROLabel(pkgName);
    }

    if (authorId >= 0)
        pathTrustedLabel = SmackLabels::generatePathTrustedLabel(authorId);

    for (auto rule : templateRules) {
        if (rule.empty())
            continue;

        std::stringstream stream(rule);
        std::string subject, object, permissions;
        stream >> subject >> object >> permissions;

        if (stream.fail() || !stream.eof()) {
            LogError("Invalid rule template: " << rule);
            ThrowMsg(SmackException::FileError, "Invalid rule template: " << rule);
        }

        strReplace(subject, SMACK_PROCESS_LABEL_TEMPLATE, appProcessLabel);
        strReplace(object,  SMACK_PROCESS_LABEL_TEMPLATE, appProcessLabel);
        strReplace(object,  SMACK_PATH_RW_LABEL_TEMPLATE, pathRWLabel);
        strReplace(object,  SMACK_PATH_RO_LABEL_TEMPLATE, pathROLabel);
        strReplace(object,  SMACK_PATH_SHARED_RO_LABEL_TEMPLATE, pathSharedROLabel);
        strReplace(object,  SMACK_PATH_TRUSTED_LABEL_TEMPLATE, pathTrustedLabel);

        if (subject.empty() || object.empty())
            continue;
        add(subject, object, permissions);
    }
}

void SmackRules::generatePackageCrossDeps(const Labels &pkgLabels)
{
    LogDebug ("Generating cross-package rules");

    std::string appsInPackagePerms = SMACK_APP_IN_PACKAGE_PERMS;

    for (const auto &subject : pkgLabels) {
        for (const auto &object : pkgLabels) {
            if (object == subject)
                continue;

            LogDebug ("Trying to add rule subject: " << subject
                      << " object: " << object << " perms: " << appsInPackagePerms);
            add(subject, object, appsInPackagePerms);
        }
    }
}

void SmackRules::generateSharedRORules(PkgsLabels &pkgsLabels, std::vector<PkgInfo> &allPkgs)
{
    LogDebug("Generating SharedRO rules");

    SmackRules rules;
    for (size_t i = 0; i < pkgsLabels.size(); ++i) {
        for (const std::string &appLabel : pkgsLabels[i].second) {
            for (size_t j = 0; j < allPkgs.size(); ++j) {
                if (!allPkgs[j].sharedRO)
                    continue;
                const std::string &pkgName = allPkgs[j].name;
                if (pkgsLabels[i].first != allPkgs[j].name)
                    rules.add(appLabel,
                              SmackLabels::generatePathSharedROLabel(pkgName),
                              SMACK_APP_CROSS_PKG_PERMS);
                else
                    rules.add(appLabel,
                              SmackLabels::generatePathSharedROLabel(pkgName),
                              SMACK_APP_PATH_OWNER_PERMS);
            }
        }
    }

    for (size_t j = 0; j < allPkgs.size(); ++j) {
        if (!allPkgs[j].sharedRO)
            continue;
        const std::string &pkgName = allPkgs[j].name;
        rules.addFromTemplateFile(SHAREDRO_RULES_TEMPLATE_FILE_PATH, std::string(), pkgName,-1);
    }

    if (smack_check())
        rules.apply();

    rules.saveToFile(SMACK_RULES_SHARED_RO_PATH);
}

void SmackRules::revokeSharedRORules(PkgsLabels &pkgsLabels, const std::string &revokePkg)
{
    LogDebug("Revoking SharedRO rules for target pkg " << revokePkg);

    if (!smack_check())
        return;

    SmackRules rules;
    for (size_t i = 0; i < pkgsLabels.size(); ++i) {
        for (const std::string &appLabel : pkgsLabels[i].second) {
            rules.add(appLabel,
                      SmackLabels::generatePathSharedROLabel(revokePkg),
                      SMACK_APP_CROSS_PKG_PERMS);
        }
    }

    rules.clear();
}

std::string SmackRules::getPackageRulesFilePath(const std::string &pkgName)
{
    return std::string(SMACK_RULES_PATH) + "/pkg_" + pkgName;
}

std::string SmackRules::getApplicationRulesFilePath(const std::string &appName)
{
    return std::string(SMACK_RULES_PATH) + "/app_" + appName;
}

std::string SmackRules::getAuthorRulesFilePath(const int authorId)
{
    return std::string(SMACK_RULES_PATH) + "/author_" + std::to_string(authorId);
}

void SmackRules::mergeRules()
{
    int tmp;
    FS::FileNameVector files = FS::getFilesFromDirectory(SMACK_RULES_PATH);

    // remove ignore files with ".temp" suffix
    files.erase(
        std::remove_if(files.begin(), files.end(),
            [&](const std::string &path) -> bool {
                if (path.size() < TEMPORARY_FILE_SUFFIX.size())
                    return false;
                return std::equal(
                    TEMPORARY_FILE_SUFFIX.rbegin(),
                    TEMPORARY_FILE_SUFFIX.rend(),
                    path.rbegin());
            }),
        files.end());

    std::ofstream dst(SMACK_RULES_PATH_MERGED_T, std::ios::binary);

    if (dst.fail()) {
        LogError("Error creating file: " << SMACK_RULES_PATH_MERGED_T);
        ThrowMsg(SmackException::FileError, "Error creating file: " << SMACK_RULES_PATH_MERGED_T);
    }

    for(auto const &e : files) {
        std::ifstream src(std::string(SMACK_RULES_PATH) + "/" + e, std::ios::binary);
        src.seekg(0, std::ios::end);
        size_t size = src.tellg();

        std::vector<char> buffer(size);
        src.seekg(0);
        src.read(buffer.data(), size);
        dst.write(buffer.data(), size);

        if (!buffer.empty() && buffer[size-1] != '\n')
            dst << '\n';

        if (dst.bad()) {
            LogError("I/O Error. File " << SMACK_RULES_PATH_MERGED << " will not be updated!");
            unlink(SMACK_RULES_PATH_MERGED_T.c_str());
            ThrowMsg(SmackException::FileError,
                "I/O Error. File " << SMACK_RULES_PATH_MERGED << " will not be updated!");
        }

        if (dst.fail()) {
            // src.rdbuf() was empty
            dst.clear();
        }
    }

    if (dst.flush().fail()) {
        LogError("Error flushing file: " << SMACK_RULES_PATH_MERGED_T);
        unlink(SMACK_RULES_PATH_MERGED_T.c_str());
        ThrowMsg(SmackException::FileError, "Error flushing file: " << SMACK_RULES_PATH_MERGED_T);
    }

    if (0 > fsync(DPL::FstreamAccessors<std::ofstream>::GetFd(dst))) {
        LogError("Error fsync on file: " << SMACK_RULES_PATH_MERGED_T);
        unlink(SMACK_RULES_PATH_MERGED_T.c_str());
        ThrowMsg(SmackException::FileError, "Error fsync on file: " << SMACK_RULES_PATH_MERGED_T);
    }

    dst.close();
    if (dst.fail()) {
        LogError("Error closing file: "  << SMACK_RULES_PATH_MERGED_T);
        unlink(SMACK_RULES_PATH_MERGED_T.c_str());
        ThrowMsg(SmackException::FileError, "Error closing file: " << SMACK_RULES_PATH_MERGED_T);
    }

    if ((tmp = rename(SMACK_RULES_PATH_MERGED_T.c_str(), SMACK_RULES_PATH_MERGED.c_str())) == 0)
        return;

    int err = errno;

    LogError("Error during file rename: "
        << SMACK_RULES_PATH_MERGED_T << " to "
        << SMACK_RULES_PATH_MERGED << " Errno: " << GetErrnoString(err));
    unlink(SMACK_RULES_PATH_MERGED_T.c_str());
    ThrowMsg(SmackException::FileError, "Error during file rename: "
        << SMACK_RULES_PATH_MERGED_T << " to "
        << SMACK_RULES_PATH_MERGED << " Errno: " << GetErrnoString(err));
}

void SmackRules::useTemplate(
        const std::string &templatePath,
        const std::string &outputPath,
        const std::string &appProcessLabel,
        const std::string &pkgName,
        const int authorId)
{
    SmackRules smackRules;
    smackRules.addFromTemplateFile(templatePath, appProcessLabel, pkgName, authorId);

    if (smack_check())
        smackRules.apply();

    smackRules.saveToFile(outputPath);
}

void SmackRules::installApplicationRules(
        const std::string &appName,
        const std::string &appProcessLabel,
        const std::string &pkgName,
        const int authorId,
        const Labels &pkgLabels)
{
    useTemplate(APP_RULES_TEMPLATE_FILE_PATH, getApplicationRulesFilePath(appName),
                appProcessLabel, pkgName, authorId);

    if (authorId >= 0)
        useTemplate(AUTHOR_RULES_TEMPLATE_FILE_PATH,
                    getAuthorRulesFilePath(authorId), appProcessLabel, pkgName, authorId);

    updatePackageRules(pkgName, pkgLabels);
}

void SmackRules::updatePackageRules(
        const std::string &pkgName,
        const Labels &pkgLabels)
{
    SmackRules smackRules;
    smackRules.addFromTemplateFile(
            PKG_RULES_TEMPLATE_FILE_PATH,
            std::string(),
            pkgName,
            -1);

    smackRules.generatePackageCrossDeps(pkgLabels);

    if (smack_check())
        smackRules.apply();

    smackRules.saveToFile(getPackageRulesFilePath(pkgName));
}


void SmackRules::revokeAppSubject(const std::string &appLabel)
{
    if (smack_revoke_subject(appLabel.c_str()))
        ThrowMsg(SmackException::LibsmackError, "smack_revoke_subject");
}

void SmackRules::uninstallPackageRules(const std::string &pkgName)
{
    uninstallRules(getPackageRulesFilePath(pkgName));
}

void SmackRules::uninstallApplicationRules(const std::string &appName, const std::string &appLabel)
{
    uninstallRules(getApplicationRulesFilePath(appName));
    revokeAppSubject(appLabel);
}

void SmackRules::uninstallRules(const std::string &path)
{
    if (access(path.c_str(), F_OK) == -1) {
        if (errno == ENOENT) {
            LogWarning("Smack rules not found in file: " << path);
            return;
        }

        LogWarning("Cannot access smack rules path: " << path);
        ThrowMsg(SmackException::FileError, "Cannot access smack rules path: " << path);
    }

    try {
        SmackRules rules;
        rules.loadFromFile(path);
        if (smack_check())
            rules.clear();
    } catch (const SmackException::Base &e) {
        LogWarning("Failed to clear smack kernel rules from file: " << path);
        // don't stop uninstallation
    }

    if (unlink(path.c_str()) == -1) {
        LogWarning("Failed to remove smack rules file: " << path);
        ThrowMsg(SmackException::FileError, "Failed to remove smack rules file: " << path);
    }
}

void SmackRules::strReplace(std::string &haystack, const std::string &needle,
            const std::string &replace)
{
    size_t pos;
    while ((pos = haystack.find(needle)) != std::string::npos)
        haystack.replace(pos, needle.size(), replace);
}

void SmackRules::uninstallAuthorRules(const int authorId)
{
    uninstallRules(getAuthorRulesFilePath(authorId));
}

void SmackRules::applyPrivateSharingRules(
        const std::string &ownerPkgName,
        const SmackRules::Labels &ownerPkgLabels,
        const std::string &targetLabel,
        const std::string &pathLabel,
        bool isPathSharedAlready,
        bool isTargetSharingAlready)
{
    SmackRules rules;
    if (!isTargetSharingAlready) {

        rules.add(targetLabel,
                  SmackLabels::generatePathRWLabel(ownerPkgName),
                  SMACK_APP_DIR_TARGET_PERMS);
    }
    if (!isPathSharedAlready) {
        for (const auto &appLabel: ownerPkgLabels) {
            rules.add(appLabel, pathLabel, SMACK_APP_PATH_OWNER_PERMS);
        }
        rules.add(SMACK_USER, pathLabel, SMACK_APP_PATH_USER_PERMS);
        rules.add(SMACK_SYSTEM, pathLabel, SMACK_APP_PATH_SYSTEM_PERMS);
        rules.add(SMACK_SYSTEM_PRIVILEGED, pathLabel, SMACK_APP_PATH_SYSTEM_PERMS);
    }
    rules.add(targetLabel, pathLabel, SMACK_APP_PATH_TARGET_PERMS);
    rules.apply();
}

void SmackRules::dropPrivateSharingRules(
        const std::string &ownerPkgName,
        const Labels &ownerPkgLabels,
        const std::string &targetLabel,
        const std::string &pathLabel,
        bool isPathSharedNoMore,
        bool isTargetSharingNoMore)
{
    SmackRules rules;
    if (isTargetSharingNoMore) {
        rules.addModify(targetLabel,
                  SmackLabels::generatePathRWLabel(ownerPkgName),
                  "", SMACK_APP_DIR_TARGET_PERMS);
    }
    if (isPathSharedNoMore) {
        for (const auto &appLabel: ownerPkgLabels) {
            rules.addModify(appLabel, pathLabel, "", SMACK_APP_PATH_OWNER_PERMS);
        }
        rules.addModify(SMACK_USER, pathLabel, "", SMACK_APP_PATH_USER_PERMS);
        rules.addModify(SMACK_SYSTEM, pathLabel, "", SMACK_APP_PATH_SYSTEM_PERMS);
        rules.addModify(SMACK_SYSTEM_PRIVILEGED, pathLabel, "", SMACK_APP_PATH_SYSTEM_PERMS);
    }
    rules.addModify(targetLabel, pathLabel, "", SMACK_APP_PATH_TARGET_PERMS);
    rules.apply();
}

} // namespace SecurityManager
