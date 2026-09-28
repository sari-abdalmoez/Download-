cmake_minimum_required(VERSION 3.16)

project(SARI_Video_Downloader
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)

add_executable(sari-downloader
    main.cpp
)

target_link_libraries(sari-downloader
    PRIVATE
    CURL::libcurl
)

if (MSVC)
    target_compile_options(sari-downloader PRIVATE /W4)
else()
    target_compile_options(sari-downloader PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()
