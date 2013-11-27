/*
 *  Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Contact: Bumjin Im <bj.im@samsung.com>
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
 * @file        open-for-manager.h
 * @author      Zbigniew Jasinski (z.jasinski@samsung.com)
 * @version     1.0
 * @brief       Implementation of open-for management functions
 */

#ifndef _OPEN_FOR_MANAGER_H_
#define _OPEN_FOR_MANAGER_H_

#include <sys/socket.h>
#include <sys/types.h>

#include <string>

namespace SecurityServer
{
    // classess
    class SockCred
    {
    public:
        SockCred();
        bool getCred(int socket);
        std::string getLabel(void) const;

    private:
        struct ucred m_cr;
        unsigned m_len;
        std::string m_sockSmackLabel;
    };

    class SharedFile
    {
    public:
        SharedFile();
        int getFD(const std::string &filename, int socket, int &fd);

    private:
        bool fileExist(const std::string &filename) const;
        bool dirExist(const std::string &dirpath) const;
        bool deleteDir(const std::string &dirpath) const;
        int openFile(const std::string &filename);
        bool createFile(const std::string &filename);
        bool setFileLabel(const std::string &filename, const std::string &label) const;
        bool getFileLabel(const std::string &filename);
        bool checkFileNameSyntax(const std::string &filename) const;

        uid_t m_fileUID;
        gid_t m_fileGID;
        mode_t m_fileMode;
        std::string m_fileSmackLabel;

        SockCred m_sockCred;
    };
}

#endif // _OPEN_FOR_MANAGER_H_