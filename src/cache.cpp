#include "cache.hpp"
#include <windows.h>
#include <iostream>
#include <map>
#include <vector>
#include <cstring>
#include <chrono>
#include <numeric>
#include <algorithm>

static unsigned long cache_hits = 0;
static unsigned long cache_misses = 0;

struct CacheBlock {
    char* data;
    bool is_dirty; // Изменен ли блок
    bool was_accessed; // Был ли блок недавно использован
};

struct FileDescriptor {
    HANDLE file_handle;
    LONGLONG file_id; // Уникальный идентификатор файла
    LARGE_INTEGER offset;
};

typedef std::pair<LONGLONG, LONGLONG> CacheKey; // (file_id, block_id)

std::map<HANDLE, FileDescriptor> fd_table;
std::map<CacheKey, CacheBlock> cache_table;
std::vector<CacheKey> cache_order;
unsigned int clock_hand = 0;

// Функция для получения уникального идентификатора файла
LONGLONG get_file_id(HANDLE file_handle) {
    BY_HANDLE_FILE_INFORMATION file_info;
    if (!GetFileInformationByHandle(file_handle, &file_info)) {
        std::cerr << "GetFileInformationByHandle failed: " << GetLastError() << std::endl;
        return -1;
    }
    // Сочетание индекса тома и идентификатора файла уникально
    return (static_cast<LONGLONG>(file_info.nFileIndexHigh) << 32) | file_info.nFileIndexLow;
}

FileDescriptor& get_file_descriptor(const HANDLE file_handle) {
    const auto iterator = fd_table.find(file_handle);
    if (iterator == fd_table.end()) {
        static FileDescriptor invalid_fd = { INVALID_HANDLE_VALUE, -1, {0} };
        return invalid_fd;
    }
    return iterator->second;
}

unsigned long lab2_get_cache_hits() {
    return cache_hits;
}

unsigned long lab2_get_cache_misses() {
    return cache_misses;
}

void lab2_reset_cache_counters() {
    cache_hits = 0;
    cache_misses = 0;
}

void free_cache_block() {
    while (!cache_order.empty()) {
        CacheKey key = cache_order[clock_hand];
        CacheBlock& block = cache_table[key];

        if (block.was_accessed) {
            block.was_accessed = false; // Сброс флага доступа
        } else {
            if (block.is_dirty) { // Запись на диск, если блок изменен
                const LONGLONG file_id = key.first;
                const LONGLONG block_id = key.second;

                // Находим file_handle по file_id
                HANDLE file_handle = INVALID_HANDLE_VALUE;
                for (const auto& [handle, fd] : fd_table) {
                    if (fd.file_id == file_id) {
                        file_handle = handle;
                        break;
                    }
                }

                if (file_handle != INVALID_HANDLE_VALUE) {
                    LARGE_INTEGER offset;
                    offset.QuadPart = block_id * BLOCK_SIZE;

                    DWORD bytes_written;
                    if (!SetFilePointerEx(file_handle, offset, nullptr, FILE_BEGIN) ||
                        !WriteFile(file_handle, block.data, BLOCK_SIZE, &bytes_written, nullptr)) {
                        std::cerr << "WriteFile failed: " << GetLastError() << std::endl;
                    }
                }
            }

            // Освобождение памяти и удаление блока из кэша
            _aligned_free(block.data);
            cache_table.erase(key);
            cache_order.erase(cache_order.begin() + clock_hand);

            if (clock_hand >= cache_order.size()) {
                clock_hand = 0; // Сброс указателя clock_hand
            }
            break;
        }

        // Перемещение указателя clock_hand
        clock_hand = (clock_hand + 1) % cache_order.size();
    }
}

char* allocate_aligned_buffer() {
    void* buf = _aligned_malloc(BLOCK_SIZE, BLOCK_SIZE);
    if (!buf) {
        std::cerr << "_aligned_malloc failed" << std::endl;
        return nullptr;
    }
    return static_cast<char*>(buf);
}

HANDLE lab2_open(const char* path, DWORD access_mode, DWORD creation_disposition) {
    HANDLE file_handle = CreateFileA(
        path,
        access_mode,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        creation_disposition,
        FILE_FLAG_NO_BUFFERING, // Отключение кэширования ОС
        nullptr
    );

    if (file_handle == INVALID_HANDLE_VALUE) {
        std::cerr << "CreateFile failed: " << GetLastError() << std::endl;
        return INVALID_HANDLE_VALUE;
    }

    LONGLONG file_id = get_file_id(file_handle);
    if (file_id == -1) {
        CloseHandle(file_handle);
        return INVALID_HANDLE_VALUE;
    }

    FileDescriptor fd = { file_handle, file_id, {0} };
    fd_table[file_handle] = fd;
    return file_handle;
}

int lab2_close(const HANDLE file_handle) {
    const auto iterator = fd_table.find(file_handle);
    if (iterator == fd_table.end()) {
        SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }

    lab2_fsync(file_handle); // Синхронизация данных с диском
    CloseHandle(file_handle);
    fd_table.erase(iterator);
    return 0;
}

SSIZE_T lab2_read(const HANDLE file_handle, void* buf, const size_t count) {
    auto& [found_handle, file_id, offset] = get_file_descriptor(file_handle);
    if (found_handle == INVALID_HANDLE_VALUE || file_id == -1) {
        SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }

    size_t bytes_read = 0;
    const auto buffer = static_cast<char*>(buf);

    while (bytes_read < count) {
        LONGLONG block_id = offset.QuadPart / BLOCK_SIZE;
        const size_t block_offset = offset.QuadPart % BLOCK_SIZE;
        const size_t bytes_to_read = std::min(BLOCK_SIZE - block_offset, count - bytes_read);

        CacheKey key = { file_id, block_id };

        auto cache_iterator = cache_table.find(key);
        if (cache_iterator != cache_table.end()) {
            cache_hits++;
            CacheBlock& found_block = cache_iterator->second;
            found_block.was_accessed = true; // Обновление флага доступа

            size_t available_bytes = BLOCK_SIZE - block_offset;
            const size_t bytes_from_block = std::min(bytes_to_read, available_bytes);
            std::memcpy(buffer + bytes_read, found_block.data + block_offset, bytes_from_block);
            offset.QuadPart += bytes_from_block;
            bytes_read += bytes_from_block;
        } else {
            cache_misses++;
            // std::cout << "Cache miss: block_id = " << block_id << std::endl;

            if (cache_table.size() >= MAX_CACHE_SIZE) {
                free_cache_block();
            }

            char* aligned_buf = allocate_aligned_buffer();
            LARGE_INTEGER read_offset;
            read_offset.QuadPart = block_id * BLOCK_SIZE;

            DWORD bytes_read_from_disk;
            if (!SetFilePointerEx(found_handle, read_offset, nullptr, FILE_BEGIN) ||
                !ReadFile(found_handle, aligned_buf, BLOCK_SIZE, &bytes_read_from_disk, nullptr)) {
                std::cerr << "ReadFile failed: " << GetLastError() << std::endl;
                _aligned_free(aligned_buf);
                return -1;
            }

            if (bytes_read_from_disk < BLOCK_SIZE) {
                std::memset(aligned_buf + bytes_read_from_disk, 0, BLOCK_SIZE - bytes_read_from_disk);
            }

            const CacheBlock new_block = { aligned_buf, false, true };
            cache_table[key] = new_block;
            cache_order.push_back(key);

            size_t available_bytes = bytes_read_from_disk - block_offset;
            if (available_bytes <= 0) {
                break;
            }
            const size_t bytes_from_block = std::min(bytes_to_read, available_bytes);
            std::memcpy(buffer + bytes_read, aligned_buf + block_offset, bytes_from_block);
            offset.QuadPart += bytes_from_block;
            bytes_read += bytes_from_block;
        }
    }
    return static_cast<SSIZE_T>(bytes_read);
}

SSIZE_T lab2_write(const HANDLE file_handle, const void* buf, const size_t count) {
    auto& [found_handle, file_id, offset] = get_file_descriptor(file_handle);
    if (found_handle == INVALID_HANDLE_VALUE || file_id == -1) {
        SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }

    size_t bytes_written = 0;
    const auto buffer = static_cast<const char*>(buf);

    while (bytes_written < count) {
        LONGLONG block_id = offset.QuadPart / BLOCK_SIZE;
        const size_t block_offset = offset.QuadPart % BLOCK_SIZE;
        const size_t to_write = std::min(BLOCK_SIZE - block_offset, count - bytes_written);

        CacheKey key = { file_id, block_id };
        auto cache_it = cache_table.find(key);
        CacheBlock* block_ptr = nullptr;


        if (cache_it != cache_table.end()) {
            cache_hits++;
            block_ptr = &cache_it->second;

            if (std::memcmp(block_ptr->data + block_offset, buffer + bytes_written, to_write) != 0) {
                std::memcpy(block_ptr->data + block_offset, buffer + bytes_written, to_write);
                block_ptr->is_dirty = true;
            }

            block_ptr->was_accessed = true;
        } else {
            cache_misses++;

            if (cache_table.size() >= MAX_CACHE_SIZE) {
                free_cache_block();
            }

            char* aligned_buf = allocate_aligned_buffer();
            if (!aligned_buf) {
                return -1;
            }

            LARGE_INTEGER read_offset;
            read_offset.QuadPart = block_id * BLOCK_SIZE;

            DWORD bytes_read;
            if (!SetFilePointerEx(found_handle, read_offset, nullptr, FILE_BEGIN) ||
                !ReadFile(found_handle, aligned_buf, BLOCK_SIZE, &bytes_read, nullptr)) {
                std::cerr << "ReadFile failed: " << GetLastError() << std::endl;
                _aligned_free(aligned_buf);
                return -1;
            }

            if (bytes_read < BLOCK_SIZE) {
                std::memset(aligned_buf + bytes_read, 0, BLOCK_SIZE - bytes_read);
            }

            CacheBlock& block = cache_table[key] = { aligned_buf, false, true };
            cache_order.push_back(key);
            block_ptr = &block;

            std::memcpy(block_ptr->data + block_offset, buffer + bytes_written, to_write);
            block_ptr->is_dirty = true; // Помечаем блок как "грязный"
        }
        
        offset.QuadPart += to_write;
        bytes_written += to_write;
    }
    return static_cast<SSIZE_T>(bytes_written);
}

LONGLONG lab2_lseek(const HANDLE file_handle, const LONGLONG offset, const int whence) {
    auto& [found_handle, file_id, file_offset] = get_file_descriptor(file_handle);
    if (found_handle == INVALID_HANDLE_VALUE || file_id == -1) {
        SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }

    if (whence != FILE_BEGIN) {
        SetLastError(ERROR_INVALID_PARAMETER); // Поддерживается только абсолютное позиционирование
        return -1;
    }

    if (offset < 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    file_offset.QuadPart = offset;
    return file_offset.QuadPart;
}

int lab2_fsync(const HANDLE file_handle) {
    const HANDLE found_handle = get_file_descriptor(file_handle).file_handle;
    if (found_handle == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }

    for (auto& [key, block] : cache_table) {
        if (key.first == get_file_descriptor(file_handle).file_id && block.is_dirty) {
            LARGE_INTEGER write_offset;
            write_offset.QuadPart = key.second * BLOCK_SIZE;

            DWORD bytes_written;
            if (!SetFilePointerEx(found_handle, write_offset, nullptr, FILE_BEGIN) ||
                !WriteFile(found_handle, block.data, BLOCK_SIZE, &bytes_written, nullptr)) {
                std::cerr << "WriteFile failed: " << GetLastError() << std::endl;
                return -1;
            }
            block.is_dirty = false;
        }
    }

    if (!FlushFileBuffers(found_handle)) {
        //std::cerr << "FlushFileBuffers failed: " << GetLastError() << std::endl;
        return -1;
    }

    return 0;
}