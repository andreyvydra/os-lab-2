#include <algorithm>
#include <iostream>
#include <chrono>
#include <vector>
#include <numeric>
#include <windows.h>
#include <BaseTsd.h> // Для SSIZE_T
#include <cstring>
#include <string>
#include "cache.hpp"

constexpr size_t FILE_SIZE = 1024 * 1024 * 1;

void run_benchmark(const std::string& file_path, int iterations, bool use_cache) {
    std::vector<char> buffer(BLOCK_SIZE, 'A');
    std::vector<double> durations;
    durations.reserve(iterations);

    // Создаем файл один раз перед началом теста
    HANDLE fd;
    if (use_cache) {
        fd = lab2_open(file_path.c_str(), GENERIC_READ | GENERIC_WRITE, CREATE_ALWAYS);
    } else {
        fd = CreateFileA(
            file_path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            CREATE_ALWAYS,
            FILE_FLAG_NO_BUFFERING, // Отключение кэширования ОС
            nullptr
        );
    }

    if (fd == INVALID_HANDLE_VALUE) {
        std::cerr << "Error opening file for IO benchmark!" << std::endl;
        return;
    }

    // Закрываем файл после создания
    if (use_cache) {
        lab2_close(fd);
    } else {
        CloseHandle(fd);
    }

    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        if (use_cache) {
            // Открываем файл для записи
            fd = lab2_open(file_path.c_str(), GENERIC_READ | GENERIC_WRITE, OPEN_EXISTING);
            if (fd == INVALID_HANDLE_VALUE) {
                std::cerr << "Error opening file for IO benchmark!" << std::endl;
                return;
            }
            
            for (size_t written = 0; written < FILE_SIZE; written += BLOCK_SIZE) {
                SSIZE_T ret = lab2_write(fd, buffer.data(), BLOCK_SIZE);
                if (ret != static_cast<SSIZE_T>(BLOCK_SIZE)) {
                    std::cerr << "Error writing to file during IO benchmark!" << std::endl;
                    lab2_close(fd);
                    return;
                }
            }

            lab2_close(fd);
        } else {
            // Открываем файл для записи
            fd = CreateFileA(
                file_path.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_NO_BUFFERING, // Отключение кэширования ОС
                nullptr
            );

            if (fd == INVALID_HANDLE_VALUE) {
                std::cerr << "Error opening file for IO benchmark!" << std::endl;
                return;
            }

            // Выделение выровненной памяти
            void* aligned_buffer = _aligned_malloc(BLOCK_SIZE, BLOCK_SIZE);
            if (!aligned_buffer) {
                std::cerr << "Error allocating aligned buffer!" << std::endl;
                CloseHandle(fd);
                return;
            }
            std::memset(aligned_buffer, 'A', BLOCK_SIZE);

            for (size_t written = 0; written < FILE_SIZE; written += BLOCK_SIZE) {
                DWORD bytes_written;
                if (!WriteFile(fd, aligned_buffer, BLOCK_SIZE, &bytes_written, nullptr)) {
                    std::cerr << "Error writing to file during IO benchmark!" << std::endl;
                    _aligned_free(aligned_buffer);
                    CloseHandle(fd);
                    return;
                }
            }

            FlushFileBuffers(fd); // Синхронизация данных с диском
            _aligned_free(aligned_buffer);
            CloseHandle(fd);
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        durations.push_back(duration.count());
    }

    const double avg_duration = std::accumulate(durations.begin(), durations.end(), 0.0) / durations.size();
    const double min_duration = *std::min_element(durations.begin(), durations.end());
    const double max_duration = *std::max_element(durations.begin(), durations.end());

    std::cout << "\nOverall Stats:\n";
    std::cout << "Average write latency: " << avg_duration << " seconds\n";
    std::cout << "Minimum write latency: " << min_duration << " seconds\n";
    std::cout << "Maximum write latency: " << max_duration << " seconds\n\n";

    if (use_cache) {
        std::cout << "Cache hits: " << lab2_get_cache_hits() << std::endl;
        std::cout << "Cache misses: " << lab2_get_cache_misses() << std::endl << std::endl;
        lab2_reset_cache_counters();
    }
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <file_path> <iterations> <use_cache>" << std::endl;
        return 1;
    }

    std::string file_path = argv[1];
    int iterations = std::stoi(argv[2]);
    bool use_cache = std::stoi(argv[3]) != 0;

    run_benchmark(file_path, iterations, use_cache);

    return 0;
}