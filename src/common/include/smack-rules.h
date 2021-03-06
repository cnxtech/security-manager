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
 * @file        smack-rules.h
 * @author      Jacek Bukarewicz <j.bukarewicz@samsung.com>
 * @version     1.0
 * @brief       Header file of a class managing smack rules
 *
 */
#pragma once

#include <vector>
#include <string>
#include <smack-exceptions.h>
#include "pkg-info.h"

struct smack_accesses;

namespace SecurityManager {

class SmackRules
{
public:
    typedef std::string Rule;
    typedef std::vector<Rule> RuleVector;
    typedef std::vector<std::string> Pkgs;
    typedef std::vector<std::string> Labels;
    typedef std::vector<std::pair<std::string, std::vector<std::string>>> PkgsLabels;
    typedef std::vector<std::pair<std::string, std::vector<std::string>>> PkgsApps;

    SmackRules();
    virtual ~SmackRules();

    void add(const std::string &subject, const std::string &object,
            const std::string &permissions);
    void addModify(const std::string &subject, const std::string &object,
            const std::string &allowPermissions, const std::string &denyPermissions);
    void loadFromFile(const std::string &path);

    void addFromTemplate(
            const RuleVector &templateRules,
            const std::string &appProcessLabel,
            const std::string &pkgName,
            const int authorId);

    void addFromTemplateFile(
            const std::string &templatePath,
            const std::string &appProcessLabel,
            const std::string &pkgName,
            const int authorId);

    void apply() const;
    void clear() const;
    void saveToFile(const std::string &path) const;

    /**
     * Create cross dependencies for all applications in a package
     *
     * This is needed for all applications within a package to have
     * correct permissions to shared data.
     *
     * @param[in] pkgLabels - a list of process labels of all applications inside this package
     */
    void generatePackageCrossDeps(const Labels &pkgLabels);

    /**
     * Generate RO rules for all applications to SharedRO apps during appInstall/Uninstall
     * Each application gets read-only access to files shared by SharedRO packages.
     *
     * @param[in] pkgsLabels         vector of process labels per each existing package
     * @param[in] allPkgs            vector of PkgInfo objects of all existing packages
     */
    static void generateSharedRORules(PkgsLabels &pkgsLabels, std::vector<PkgInfo> &allPkgs);

    /**
     * Revoke SharedRO rules for applications when a package is being removed
     * Rules from all applications in \ref pkgsApps to SharedRO label of the package
     * under removal will be revoked from kernel.
     *
     * @param[in] pkgsLabels         vector of process labels per each existing package
     * @param[in] revokePkg   package name being removed
     */
    static void revokeSharedRORules(PkgsLabels &pkgsLabels, const std::string &revokePkg);

    /**
     * Install package-specific smack rules plus add rules for specified external apps.
     *
     * Function creates smack rules using predefined template. Rules are applied
     * to the kernel and saved on persistent storage so they are loaded on system boot.
     *
     * @param[in] appName - application identifier
     * @param[in] pkgName - package identifier
     * @param[in] authorId - author id of application
     * @param[in] pkgLabels - a list of process labels of all applications inside this package
     */
    static void installApplicationRules(
            const std::string &appName,
            const std::string &appProcessLabel,
            const std::string &pkgName,
            const int authorId,
            const Labels &pkgLabels);

    /**
     * Uninstall package-specific smack rules.
     *
     * Function loads package-specific smack rules, revokes them from the kernel
     * and removes them from the persistent storage.
     *
     * @param[in] pkgName - package identifier
     */
    static void uninstallPackageRules(const std::string &pkgName);

    /**
    * Uninstall application-specific smack rules.
    *
    * Function removes application specific rules from the kernel, and
    * removes them for persistent storage.
    *
    * @param[in] appName - application identifier
    * @param[in] appLabel - application process label
    */
    static void uninstallApplicationRules(const std::string &appName, const std::string &appLabel);

    /**
     * Update package specific rules
     *
     * This function regenerates all package rules that
     * need to exist currently for all application in that
     * package
     *
     * @param[in] pkgName - package identifier that the application is in
     * @param[in] pkgContents - list of all applications in the package
     */
    static void updatePackageRules(
            const std::string &pkgName,
            const std::vector<std::string> &pkgContents);

    /**
     * Uninstall author-specific smack rules.
     *
     * param[in] authorId - identification (datbase key) of the author
     */
    static void uninstallAuthorRules(const int authorId);

    /**
     * Add rules related to private path sharing rules
     *
     * This function generates and applies rules needed to apply private sharing.
     * If isPathSharedAlready, no rule for owner, User or System to path label will be applied.
     * If isTargetSharingAlready, no rule for directory traversing is set for target.
     *
     * @param[in] ownerPkgName - package identifier of path owner
     * @param[in] ownerPkgLabels - vector of process labels of applications contained in package
     *                             which owner application belongs to
     * @param[in] targetAppLabel - process label of the target application
     * @param[in] pathLabel - a list of all applications in the package
     * @param[in] isPathSharedAlready - flag indicated, if path has been shared before
     * @param[in] isTargetSharingAlready - flag indicated, if target is already sharing anything
     *                                     with owner
     */
    static void applyPrivateSharingRules(const std::string &ownerPkgName,
                                         const SmackRules::Labels &ownerPkgLabels,
                                         const std::string &targetAppLabel,
                                         const std::string &pathLabel,
                                         bool isPathSharedAlready,
                                         bool isTargetSharingAlready);
    /**
     * Remove rules related to private path sharing rules
     *
     * This function generates and applies rules needed to apply private sharing.
     * If isPathSharedNoMore, rules for owner package contents, User or System to path label will
     * be removed.
     * If isTargetSharingNoMore, rule for directory traversing is removed for target.
     *
     * @param[in] ownerPkgName - package identifier of path owner
     * @param[in] ownerPkgLabels - vector of process labels of applications contained in package
     *                             which owner application belongs to
     * @param[in] targetAppLabel - process label of the target application
     * @param[in] pathLabel - a list of all applications in the package
     * @param[in] isPathSharedNoMore - flag indicated, if path is not shared anymore
     * @param[in] isTargetSharingNoMore - flag indicated, if target is not sharing anything
     *                                    with owner
     */
    static void dropPrivateSharingRules(const std::string &ownerPkgName,
                                        const Labels &ownerPkgLabels,
                                        const std::string &targetAppLabel,
                                        const std::string &pathLabel,
                                        bool isPathSharedNoMore,
                                        bool isTargetSharingNoMore);

    /**
     * This function will read all rules created by security-manager and
     * save them in one file. This file will be used during next system
     * boot.
     */
    static void mergeRules();

private:
    static void useTemplate(
            const std::string &templatePath,
            const std::string &outputPath,
            const std::string &appProcessLabel,
            const std::string &pkgName,
            const int authorId = -1);

    /**
     * Create a path for package rules
     *
     */
    static std::string getPackageRulesFilePath(const std::string &pkgName);

    /**
     * Create a path for application rules
     */
    static std::string getApplicationRulesFilePath(const std::string &appName);

    /**
     * Create a path for application rules
     */
    static std::string getPkgRulesFilePath(const std::string &pkgName);

    /**
     * Create a path for author rules
     */
    static std::string getAuthorRulesFilePath(int authorId);

    /**
     * Uninstall rules inside a specified file path
     *
     * This is a utility function that will clear all
     * rules in the file specified by path
     *
     * @param[in] path - path to the file that contains the rules
     */
    static void uninstallRules(const std::string &path);

    /**
     * Helper method: replace all occurrences of \ref needle in \ref haystack
     * with \ref replace.
     *
     * @param[in,out] haystack string to modify
     * @param needle string to find in \ref haystack
     * @param replace string to replace \ref needle with
     */
    static void strReplace(std::string &haystack, const std::string &needle,
            const std::string &replace);

    smack_accesses *m_handle;

    /**
     * Revoke rules for which label of given \ref appName is a subject.
     *
     * @param[in] appLabel = application process label
     */
    static void revokeAppSubject(const std::string &appLabel);
};

} // namespace SecurityManager
