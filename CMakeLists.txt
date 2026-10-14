cmake_minimum_required(VERSION 3.16)
project(emby_downloader_gui VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

set(CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../emby-downloader-core")

set(SOURCES
    src/poster_cache.cpp
    src/episode_filter.cpp
    src/episode_dialog.cpp
    src/server_dialog.cpp
    src/queue_view.cpp
    src/search_view.cpp
    src/app_window.cpp
    src/main.cpp
)

add_executable(emby-downloader-gui ${SOURCES})

target_include_directories(emby-downloader-gui PRIVATE
    include
    ${CORE_DIR}/include
    ${GTK3_INCLUDE_DIRS}
)

target_link_directories(emby-downloader-gui PRIVATE
    ${CORE_DIR}
    ${GTK3_LIBRARY_DIRS}
)

target_link_libraries(emby-downloader-gui PRIVATE
    ${CORE_DIR}/libemby_core.a
    ${GTK3_LIBRARIES}
    CURL::libcurl
    Threads::Threads
)

target_compile_options(emby-downloader-gui PRIVATE ${GTK3_CFLAGS_OTHER})
