#pragma once

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <sys/stat.h>

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & _S_IFDIR) != 0)
#endif

struct dirent {
    char d_name[MAX_PATH];
};

struct DIR {
    HANDLE handle = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAA data = {};
    dirent entry = {};
    bool first = true;
};

inline DIR* opendir(const char* path) {
    if (path == nullptr || *path == '\0') {
        errno = ENOENT;
        return nullptr;
    }

    std::string pattern(path);
    const char last = pattern.empty() ? '\0' : pattern.back();
    if (last != '\\' && last != '/') {
        pattern += "\\";
    }
    pattern += "*";

    auto* dir = new DIR();
    dir->handle = FindFirstFileA(pattern.c_str(), &dir->data);
    if (dir->handle == INVALID_HANDLE_VALUE) {
        delete dir;
        errno = ENOENT;
        return nullptr;
    }
    return dir;
}

inline dirent* readdir(DIR* dir) {
    if (dir == nullptr || dir->handle == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return nullptr;
    }

    if (dir->first) {
        dir->first = false;
    } else if (!FindNextFileA(dir->handle, &dir->data)) {
        return nullptr;
    }

    strncpy_s(dir->entry.d_name, dir->data.cFileName, _TRUNCATE);
    return &dir->entry;
}

inline int closedir(DIR* dir) {
    if (dir == nullptr) {
        errno = EBADF;
        return -1;
    }
    if (dir->handle != INVALID_HANDLE_VALUE) {
        FindClose(dir->handle);
    }
    delete dir;
    return 0;
}

#endif
