/*
 *  Copyright (c) 2016. Anyfi Inc.
 *
 *  All Rights Reserved.
 *
 *  NOTICE:  All information contained herein is, and remains the property of
 *  Anyfi Inc. and its suppliers, if any.  The intellectual and technical concepts
 *  contained herein are proprietary to Anyfi Inc. and its suppliers and may be
 *  covered by U.S. and Foreign Patents, patents in process, and are protected
 *  by trade secret or copyright law.
 *
 *  Dissemination of this information or reproduction of this material is
 *  strictly forbidden unless prior written permission is obtained from Anyfi Inc.
 */

#ifndef ANYFIMESH_CORE_LOGUPLOADER_H
#define ANYFIMESH_CORE_LOGUPLOADER_H

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>
#endif

class LogUploadTask {
public:
    LogUploadTask(const std::string &localPath, const std::string &serverPath) {
        _localFilePath = localPath;
        _serverPath = serverPath;
    }

    void startUpload(const std::vector<std::string> logFileList);
private:
    std::string _localFilePath;
    std::string _serverPath;

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    FRIEND_TEST(LogUploadTask, init);
#endif
};


#endif //ANYFIMESH_CORE_LOGUPLOADER_H
