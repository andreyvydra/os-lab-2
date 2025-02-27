#include <algorithm>
#include <iostream>
#include <chrono>
#include <vector>
#include <numeric>
#include <windows.h>
#include <BaseTsd.h>
#include <cstring>
#include "cache.hpp"
#include <iostream>
#include <string>

void run_benchmark(const std::string& file_path, int iterations, bool use_cache) {
    std::vector<char> buffer(BLOCK_SIZE);
    std::vector<double> durations;
    durations.reserve(iterations);

    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        if (use_cache) {
        
            const HANDLE fd = lab2_open(file_path.c_str(), GENERIC_READ, OPEN_EXISTING);

            if (fd == INVALID_HANDLE_VALUE) {
                std::cerr << "Error opening file for IO benchmark!" << std::endl;
                return;
            }

            SSIZE_T bytes_read = 0;
            do {
                bytes_read = lab2_read(fd, buffer.data(), BLOCK_SIZE);
                if (bytes_read < 0) {
                    std::cerr << "Error reading from file during IO benchmark!" << std::endl;
                    lab2_close(fd);
                    return;
                }
            } while (bytes_read > 0);

            lab2_close(fd);
        } else {
            // Открытие файла с отключенным кэшированием ОС
            HANDLE fd = CreateFileA(
                file_path.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
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

            DWORD bytes_read = 0;
            do {
                if (!ReadFile(fd, aligned_buffer, BLOCK_SIZE, &bytes_read, nullptr)) {
                    std::cerr << "Error reading from file during IO benchmark!" << std::endl;
                    _aligned_free(aligned_buffer);
                    CloseHandle(fd);
                    return;
                }
            } while (bytes_read > 0);

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
    std::cout << "Average read latency: " << avg_duration << " seconds\n";
    std::cout << "Minimum read latency: " << min_duration << " seconds\n";
    std::cout << "Maximum read latency: " << max_duration << " seconds\n\n";

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