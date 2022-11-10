/* Copyright (C) 2018. Anyfi Inc. - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */
#ifndef ANYFIMESH_CORE_STDAFX_H_H
#define ANYFIMESH_CORE_STDAFX_H_H

#include <memory>
#include <string>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <vector>
#if (defined(_WIN32) || defined(__WIN32__)) && !defined(WIN32) && !defined(__SYMBIAN32__)
#include <windows.h>
#else
#include <sys/types.h>
#include <dirent.h>
#include <thread>

#endif

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args &&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template<typename T>
std::string to_string(T value) {
    std::ostringstream os;
    os << value;
    return os.str();
}

template<typename Base, typename T>
inline bool instanceof(const T *ptr) {
    return dynamic_cast<const Base *>(ptr) != nullptr;
}

static unsigned long getCurrentTimeMilliseconds() {
    return (unsigned long) (std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1));
}

static std::string to_hexstring(uint8_t *data, int len) {
    std::stringstream ss;
    ss << std::hex;
    for (int i = 1; i <= len; i++)
        ss << (int) data[i] << (i % 10 == 0 ? "\n" : " ");
    return ss.str();
}

struct EnumHasher {
    template<typename T>
    std::size_t operator()(T t) const {
        return static_cast<std::size_t>(t);
    }
};

template<size_t N>
constexpr size_t length(char const (&)[N]) {
    return N - 1;
}

static std::vector<std::string> getFilesInDirectory(const std::string &path) {
    std::vector<std::string> out;
#if (defined(_WIN32) || defined(__WIN32__)) && !defined(WIN32) && !defined(__SYMBIAN32__)
    std::string pattern(path);
    pattern.append("\\*");
    WIN32_FIND_DATA data;
    HANDLE hFind;
    if ((hFind = FindFirstFile(pattern.c_str(), &data)) != INVALID_HANDLE_VALUE) {
        do {
            out.push_back(std::string(data.cFileName));
        } while (FindNextFile(hFind, &data) != 0);
        FindClose(hFind);
    }
#else
    DIR* dirp = opendir(path.c_str());
    struct dirent * dp;
    while ((dp = readdir(dirp)) != nullptr) {
        auto fileName = std::string(dp->d_name);
        if (fileName == "." || fileName == ".." || dp->d_type != DT_REG)
            continue;
        out.push_back(fileName);
    }
    closedir(dirp);
#endif
    return out;
} // getFilesInDirectory

static inline const std::string thread_id_str() {
    return to_string(static_cast<size_t>(std::hash<std::thread::id>()(std::this_thread::get_id())));
}
#endif //ANYFIMESH_CORE_STDAFX_H_H
