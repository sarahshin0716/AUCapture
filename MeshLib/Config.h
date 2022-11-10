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

#ifndef ANYFIMESH_CORE_CONFIG_H
#define ANYFIMESH_CORE_CONFIG_H

#include <mutex>

namespace Anyfi {
namespace Config {
    class L4 {
    public:
        const static int MAX_QUALITY = 1;
    };

    class AMAZON_S3 {
    public:
        const static std::string BUCKET_NAME;
        const static std::string REGION;
        const static std::string RELEASE_BASEPATH;
        const static std::string DEBUG_BASEPATH;

        const static std::string ACCESS_KEY;
        const static std::string SECRET_KEY;
    };

    using mutex_type = std::mutex;
    using lock_type = std::lock_guard<mutex_type>;
};

}

#endif //ANYFIMESH_CORE_CONFIG_H
