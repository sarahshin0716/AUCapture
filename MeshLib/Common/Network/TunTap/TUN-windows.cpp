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

// TUN-windows.cpp should only be compiled on windows
#ifdef _WIN32


#include "TUN.h"

#include <Windows.h>
#include <winioctl.h>

#include <stdio.h>

#include "Common/stdafx.h"
#include "Log/Frontend/Log.h"
#include "Common/Network/Buffer/NetworkBufferPool.h"


#define _TAP_IOCTL(nr) CTL_CODE(FILE_DEVICE_UNKNOWN, nr, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define TAP_IOCTL_GET_MAC                   _TAP_IOCTL(1)
#define TAP_IOCTL_GET_VERSION               _TAP_IOCTL(2)
#define TAP_IOCTL_GET_MTU                   _TAP_IOCTL(3)
#define TAP_IOCTL_GET_INFO                  _TAP_IOCTL(4)
#define TAP_IOCTL_CONFIG_POINT_TO_POINT     _TAP_IOCTL(5)
#define TAP_IOCTL_SET_MEDIA_STATUS          _TAP_IOCTL(6)
#define TAP_IOCTL_CONFIG_DHCP_MASQ          _TAP_IOCTL(7)
#define TAP_IOCTL_GET_LOG_LINE              _TAP_IOCTL(8)
#define TAP_IOCTL_CONFIG_DHCP_SET_OPT       _TAP_IOCTL(9)
#define TAP_IOCTL_CONFIG_TUN                _TAP_IOCTL(10)

#define TAP_COMPONENT_ID "tap0901"
#define DEVTEMPLATE "\\\\.\\Global\\%s.tap"
#define NETDEV_GUID "{4D36E972-E325-11CE-BFC1-08002BE10318}"
#define CONTROL_KEY "SYSTEM\\CurrentControlSet\\Control\\"

#define ADAPTERS_KEY CONTROL_KEY "Class\\" NETDEV_GUID
#define CONNECTIONS_KEY CONTROL_KEY "Network\\" NETDEV_GUID

typedef intptr_t(deviceFoundCallback)(const char *addr, const char *mask, char *idx, char *name);


#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop) )

PACK(struct packed_uint16_t {
    uint16_t d;
});

#define realloc_inplace(p, size) do {       \
	    void *__realloc_old = (void *) p;	\
	    p = (char *) realloc(p, size);		\
	    if (size && !p)					    \
		    free(__realloc_old);			\
    } while (0)

static inline uint16_t load_le16(const void *_p) {
    const struct packed_uint16_t *p = (const struct packed_uint16_t *) _p;
    return p->d;
}

#define TEXTBUF_MAX_LEN 131072
#define TEXTBUF_CHUNK_SIZE 4096

class TextBuf {
public:
    char *data;
    int pos;
    int bufLen;
    int error;

    static TextBuf *alloc() {
        return (TextBuf *)calloc(1, sizeof(struct TextBuf));
    }

    void truncate() {
        pos = 0;
        if (data)
            data[0] = 0;
    }

    int ensureSpace(int len) {
        int newBufLen = (pos + len + TEXTBUF_CHUNK_SIZE - 1) & ~(TEXTBUF_CHUNK_SIZE - 1);

        if (newBufLen <= bufLen)
            return 0;

        if (newBufLen > TEXTBUF_MAX_LEN) {
            error = -E2BIG;
            return error;
        }
        else {
            realloc_inplace(data, newBufLen);
            if (!data)
                error = -ENOMEM;
            else
                bufLen = newBufLen;
        }
        return error;
    }

    void appendBytes(const void *bytes, int len) {
        if (error)
            return;

        if (ensureSpace(len + 1))
            return;

        memcpy(data + pos, bytes, len);
        pos += len;
        data[pos] = 0;
    }

    void appendFromUTF16le(const void *_utf16) {
        const unsigned char *utf16 = (const unsigned char *) _utf16;
        unsigned char utf8[4];
        int c;

        if (!utf16)
            return;

        while (utf16[0] || utf16[1]) {
            if ((utf16[1] & 0xfc) == 0xd8 && (utf16[3] & 0xfc) == 0xdc) {
                c = ((load_le16(utf16) & 0x3ff) << 10) |
                    (load_le16(utf16 + 2) & 0x3ff);
                c += 0x10000;
                utf16 += 4;
            }
            else {
                c = load_le16(utf16);
                utf16 += 2;
            }

            if (c < 0x80) {
                utf8[0] = c;
                appendBytes(utf8, 1);
            }
            else if (c < 0x800) {
                utf8[0] = 0xc0 | (c >> 6);
                utf8[1] = 0x80 | (c & 0x3f);
                appendBytes(utf8, 2);
            }
            else if (c < 0x10000) {
                utf8[0] = 0xe0 | (c >> 12);
                utf8[1] = 0x80 | ((c >> 6) & 0x3f);
                utf8[2] = 0x80 | (c & 0x3f);
                appendBytes(utf8, 3);
            }
            else {
                utf8[0] = 0xf0 | (c >> 18);
                utf8[1] = 0x80 | ((c >> 12) & 0x3f);
                utf8[2] = 0x80 | ((c >> 6) & 0x3f);
                utf8[3] = 0x80 | (c & 0x3f);
                appendBytes(utf8, 4);
            }
        }
        utf8[0] = 0;
        appendBytes(utf8, 1);
    }

    ~TextBuf() {
        if (data)
            free(data);
    }

private:
    TextBuf() {}
};


static intptr_t _searchDevice(deviceFoundCallback *cb, int all) {
    LONG status;
    HKEY adaptersKey, hKey;
    DWORD len, type;
    char buf[40];
    wchar_t name[40];
    char keyname[length(CONNECTIONS_KEY) + sizeof(buf) + 1 + length("\\Connection")];
    int i = 0, found = 0;
    intptr_t ret = -1;
    struct TextBuf *nameBuf = TextBuf::alloc();

    status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, ADAPTERS_KEY, 0, KEY_READ, &adaptersKey);
    if (status) {
        Anyfi::Log::error(__func__, "Error accessing registry key for network adapters");
        return -EIO;
    }

    while (true) {
        len = sizeof(buf);
        status = RegEnumKeyExA(adaptersKey, i++, buf, &len, NULL, NULL, NULL, NULL);
        if (status) {
            if (status != ERROR_NO_MORE_ITEMS)
                ret = -1;
            break;
        }

        snprintf(keyname, sizeof(keyname), "%s\\%s", ADAPTERS_KEY, buf);
        status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyname, 0, KEY_QUERY_VALUE, &hKey);
        if (status) continue;

        len = sizeof(buf);
        status = RegQueryValueExA(hKey, "ComponentId", NULL, &type, (unsigned char *)buf, &len);
        if (status || type != REG_SZ || strcmp(buf, TAP_COMPONENT_ID)) {
            RegCloseKey(hKey);
            continue;
        }

        len = sizeof(buf);
        status = RegQueryValueExA(hKey, "NetCfgInstanceId", NULL, &type, (unsigned char *)buf, &len);
        RegCloseKey(hKey);
        if (status || type != REG_SZ)
            continue;

        snprintf(keyname, sizeof(keyname), "%s\\%s\\Connection", CONNECTIONS_KEY, buf);
        status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyname, 0, KEY_QUERY_VALUE, &hKey);
        if (status) continue;

        len = sizeof(name);
        status = RegQueryValueExW(hKey, L"Name", NULL, &type, (unsigned char *)name, &len);
        RegCloseKey(hKey);
        if (status || type != REG_SZ) continue;

        nameBuf->truncate();
        nameBuf->appendFromUTF16le(name);
        if (nameBuf->error) {
            ret = nameBuf->error;
            delete nameBuf;
            break;
        }

        found++;

        ret = cb("10.9.28.7", "255.255.0.0", buf, nameBuf->data);
        if (!all)
            break;
    }

    RegCloseKey(adaptersKey);
    if (nameBuf != nullptr)
        delete nameBuf;

    if (!found)
        Anyfi::Log::error(__func__, "No Windows-TAP adapters found. Is the driver installed?");

    return ret;
}

static intptr_t _openTun(const char *addr, const char *mask, char *guid, char *name) {
    char devname[80];
    HANDLE tun_fh;
    ULONG data[3];
    DWORD len;

    snprintf(devname, sizeof(devname), DEVTEMPLATE, guid);
    tun_fh = CreateFileA(devname, GENERIC_WRITE | GENERIC_READ, 0, 0,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED,
        0);
    if (tun_fh == INVALID_HANDLE_VALUE) {
        Anyfi::Log::error(__func__, "Failed to open tun");
        return -1;
    }
    // Opened tun device

    if (!DeviceIoControl(tun_fh, TAP_IOCTL_GET_VERSION,
        data, sizeof(data),
        data, sizeof(data), &len, NULL)) {
        Anyfi::Log::error(__func__, "Failed to obtain TAP driver");
        return -1;
    }
    if (data[0] < 9 || (data[0] == 9 && data[1] < 9)) {
        Anyfi::Log::error(__func__, "TAP-Windows driver v9.9 or greater is required");
        return -1;
    }

    ULONG mtu;
    if (DeviceIoControl(tun_fh, TAP_IOCTL_GET_MTU,
        &mtu, sizeof(mtu),
        &mtu, sizeof(mtu), &len, NULL)) {
        Anyfi::Log::warn(__func__, "VPN MTU : " + mtu);
    }

    auto _addr = inet_addr(addr), _mask = inet_addr(mask);
    data[0] = htonl(_addr);
    data[2] = htonl(_mask);
    data[1] = htonl(_addr & _mask);

    if (!DeviceIoControl(tun_fh, TAP_IOCTL_CONFIG_TUN,
        data, sizeof(data),
        data, sizeof(data), &len, NULL)) {
        Anyfi::Log::error(__func__, "Failed to set TAP IP address");
        return -1;
    }

    std::string command = "netsh interface ip set address \"";
    command += name;
    command += "\" static 10.9.47.37 255.255.0.0 10.9.0.1 1";
    system(command.c_str());

    command = "netsh interface ip learn dnsserver \"";
    command += name;
    command += "\" address=10.9.0.1 index=1";
    system(command.c_str());

    system("route change 0.0.0.0 mask 0.0.0.0 10.9.0.1");

    data[0] = 1;
    if (!DeviceIoControl(tun_fh, TAP_IOCTL_SET_MEDIA_STATUS,
        data, sizeof(data[0]),
        data, sizeof(data[0]), &len, NULL)) {
        Anyfi::Log::error(__func__, "Failed to set TAP media status");
        return -1;
    }

    return (intptr_t) tun_fh;
}

static int _readTun(TUN::VpnInfo *info, uint8_t *buf, unsigned int len) {
    DWORD readBytes = 0;

//reread:
//    if (!info->read_pending && !ReadFile(info->tun_fh, buf, len, &readBytes, &info->overlapped)) {
//        DWORD err = GetLastError();
//        if (err == ERROR_IO_PENDING) {
//            info->read_pending = true;
//        } else if (err = ERROR_OPERATION_ABORTED) {
//            Anyfi::Log::error(__func__, "Tap device aborted connectivity. Disconnected");
//        } else {
//            Anyfi::Log::error(__func__, "Failed to read from TunTap device");
//        }
//        return -1;
//    } else if (!GetOverlappedResult(info->tun_fh, &info->overlapped, &readBytes, FALSE)) {
//        DWORD err = GetLastError();
//        if (err != ERROR_IO_INCOMPLETE) {
//            info->read_pending = false;
//            Anyfi::Log::warn(__func__, "Failed to complete read from TunTap device");
//            goto reread;
//        }
//        return -1;
//    }
//
//    info->read_pending = false;
//    return readBytes;

    LARGE_INTEGER nLarge;
    nLarge.QuadPart = 1;
    info->overlapped.Offset = nLarge.LowPart;
    info->overlapped.OffsetHigh = nLarge.HighPart;
    if (!ReadFile(info->tun_fh, buf, len, NULL, &info->overlapped)) {
        auto err = GetLastError();
        if (err == ERROR_IO_PENDING) {
            // right
        }
        else {
            // error
            exit(1);
        }
    }
    else {
        //error
        exit(1);
    }

    WaitForSingleObject(info->overlapped.hEvent, INFINITE);

    if (!GetOverlappedResult(info->tun_fh, &info->overlapped, &readBytes, TRUE)) {
        // error
        exit(1);
    }

    return readBytes;
}



//
// ========================================================================================
// TUN impl
//
int TUN::open_tun(std::string name) {
    int tun = _searchDevice(_openTun, 0);
    //system("route learn 0.0.0.0 mask 0.0.0.0 10.0.0.1");
    //system("route change 0.0.0.0 mask 0.0.0.0 10.0.0.1");
    return tun;
}

int TUN::set_address(std::string name, std::string addr, int prefix) {
    return 0;
}

void TUN::add_route(std::string name, std::string route, int prefix) {

}

void TUN::set_default_route(std::string route) {

}

void TUN::set_default_interface(const std::string &intf) {

}

std::string TUN::get_default_interface() {
    return "";
}

int TUN::bind_to_interface(int socket, const char *name) {
    return 0;
}

NetworkBufferPointer TUN::read_tun(TUN::VpnInfo *info) {
    auto buffer = NetworkBufferPool::acquire();
    buffer->resize((uint32_t) buffer->capacity());

    auto readBytes = _readTun(info, buffer->buffer(), buffer->capacity());
    if (readBytes <= 0)
        return buffer;

    buffer->resize((uint32_t) readBytes);
    buffer->setWritePos((uint32_t) readBytes);
    return buffer;
}

void TUN::write_tun(TUN::VpnInfo *info, NetworkBufferPointer buffer) {

}
//
// TUN impl
// ========================================================================================
//


#endif