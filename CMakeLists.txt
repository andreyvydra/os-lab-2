cmake_minimum_required(VERSION 3.10)
project(CacheBenchmark)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES_READ
    src/io-read.cpp
    src/cache.cpp
)

set(SOURCES_WRITE
    src/io-write.cpp
    src/cache.cpp
)


# Добавление исполняемого файла
add_executable(cache_benchmark_read ${SOURCES_READ})
add_executable(cache_benchmark_write ${SOURCES_WRITE})

# Подключение библиотек Windows
target_link_libraries(cache_benchmark_read ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(cache_benchmark_write ${CMAKE_THREAD_LIBS_INIT})