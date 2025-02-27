#ifndef LAB2_H
#define LAB2_H

#include <cstddef>
#include <sys/types.h>
#include <fcntl.h>

#define NOMINMAX
#include <windows.h>
#include <BaseTsd.h>
#include <map>
#include <list>
#include <vector>
#include <cstring>
#include <iostream>
#include <cerrno>
#include <iostream>
#include <algorithm>

#define BLOCK_SIZE 4096 // Max bytes in block
#define MAX_CACHE_SIZE 512 // Max blocks in cache (128KB)

HANDLE lab2_open(const char* path, DWORD access_mode, DWORD creation_disposition);
int lab2_close(const HANDLE file_handle);
SSIZE_T lab2_read(const HANDLE file_handle, void* buf, const size_t count);
SSIZE_T lab2_write(const HANDLE file_handle, const void* buf, const size_t count);
LONGLONG lab2_lseek(const HANDLE file_handle, const LONGLONG offset, const int whence);
int lab2_fsync(const HANDLE file_handle);

unsigned long lab2_get_cache_hits();
unsigned long lab2_get_cache_misses();
void lab2_reset_cache_counters();

#endif