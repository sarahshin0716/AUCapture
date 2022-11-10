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

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <Common/Network/Buffer/NetworkBuffer.h>
#include <aws/core/utils/HashingUtils.h>
#include <iostream>
#include <fstream>

// MARK: NDK r15, r17에서 cstdio 컴파일 에러 발생 https://github.com/android-ndk/ndk/issues/480
#include <cstdio>

#include "LogUploadTask.h"
#include "Common/stdafx.h"
#include "Config.h"

void LogUploadTask::startUpload(const std::vector<std::string> logFileList) {
    auto credentials = Aws::Auth::AWSCredentials(Anyfi::Config::AMAZON_S3::ACCESS_KEY,
                                                 Anyfi::Config::AMAZON_S3::SECRET_KEY);
    const Aws::String bucketName = Anyfi::Config::AMAZON_S3::BUCKET_NAME;
    const Aws::String region = Anyfi::Config::AMAZON_S3::REGION;

    for (const auto &fileName : logFileList) {
        const std::string localPath = _localFilePath + fileName;

        std::ifstream in(localPath, std::ifstream::ate | std::ifstream::binary);
        long long int fileSize = in.tellg();
        in.close();

        auto input_data = Aws::MakeShared<Aws::FStream>("PutObjectInputStream",
                                                        localPath.c_str(), std::ios_base::in | std::ios_base::binary);
        if (!input_data->good()) {
            Anyfi::Log::error(__func__, localPath + " is not good");
            continue;
        }
        if (!input_data->is_open()) {
            Anyfi::Log::error(__func__, localPath + " is not open");
            continue;
        }

        if (fileSize == 0) {
            input_data->close();
            std::remove(localPath.c_str());
            continue;
        }

        Aws::Client::ClientConfiguration clientConfig;
        clientConfig.scheme = Aws::Http::Scheme::HTTPS;
        clientConfig.region = Aws::Region::AP_NORTHEAST_2;
        clientConfig.connectTimeoutMs = 600000;
        clientConfig.requestTimeoutMs = 600000;
        // TODO: 추후 인증서를 수동으로 추가하고 true로 변경(참고 https://github.com/aws/aws-sdk-cpp/issues/791)
        clientConfig.verifySSL = false;

        Aws::S3::S3Client s3_client(credentials, clientConfig);

        const Aws::String keyName = _serverPath + fileName;

        Aws::S3::Model::PutObjectRequest objectRequest;
        objectRequest.WithBucket(bucketName).WithKey(keyName);


        objectRequest.SetBody(input_data);
        objectRequest.SetContentType("text/plain");
        objectRequest.SetContentLength(fileSize);

#ifdef NDEBUG
#else
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::LOG_SERVER, __func__,
                          "Uploading " + localPath +
                          " to S3 bucket("+region+") at key " + keyName);
#endif

        auto putObjectOutcome = s3_client.PutObject(objectRequest);
        input_data->close();
        if (putObjectOutcome.IsSuccess()) {
#ifdef NDEBUG
#else
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::LOG_SERVER, __func__, "...Done remove file: " + localPath);
#endif
            std::remove(localPath.c_str());
        } else {
            Anyfi::Log::error(__func__, "record....... PutObject file : " + localPath + " error: " +
                    putObjectOutcome.GetError().GetExceptionName() + " " + putObjectOutcome.GetError().GetMessage());
        }
    }
}