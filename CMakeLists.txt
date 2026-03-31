cmake_minimum_required(VERSION 3.16)
project(nvcs VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0 -DDEBUG")

# ── Find system libraries ────────────────────────────────────────────────────
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

# ── Core library ─────────────────────────────────────────────────────────────
set(NVCS_CORE_SOURCES
    src/util/hash.cpp
    src/util/compress.cpp
    src/util/fs.cpp
    src/core/object.cpp
    src/core/blob.cpp
    src/core/tree.cpp
    src/core/commit.cpp
    src/core/index.cpp
    src/core/refs.cpp
    src/core/repository.cpp
    src/net/protocol.cpp
    src/net/transport.cpp
)

add_library(nvcs_core STATIC ${NVCS_CORE_SOURCES})

target_include_directories(nvcs_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(nvcs_core
    PUBLIC
        OpenSSL::SSL
        OpenSSL::Crypto
        ZLIB::ZLIB
        Threads::Threads
)

# Suppress noisy warnings from third-party headers
target_compile_options(nvcs_core PRIVATE
    -Wno-deprecated-declarations
)

# ── CLI client ───────────────────────────────────────────────────────────────
set(NVCS_CLI_SOURCES
    cli/main.cpp
    cli/commands/init.cpp
    cli/commands/add.cpp
    cli/commands/commit.cpp
    cli/commands/status.cpp
    cli/commands/log.cpp
    cli/commands/branch.cpp
    cli/commands/checkout.cpp
    cli/commands/diff.cpp
    cli/commands/tag.cpp
    cli/commands/remote.cpp
    cli/commands/push.cpp
    cli/commands/pull.cpp
    cli/commands/clone.cpp
    cli/commands/config_cmd.cpp
    cli/commands/cat_object.cpp
)

add_executable(nvcs ${NVCS_CLI_SOURCES})
target_link_libraries(nvcs PRIVATE nvcs_core)
target_include_directories(nvcs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_compile_options(nvcs PRIVATE -Wno-deprecated-declarations)

# ── Server ───────────────────────────────────────────────────────────────────
add_executable(nvcs-server server/main.cpp)
target_link_libraries(nvcs-server PRIVATE nvcs_core)
target_include_directories(nvcs-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_compile_options(nvcs-server PRIVATE -Wno-deprecated-declarations)

# ── Install ──────────────────────────────────────────────────────────────────
install(TARGETS nvcs nvcs-server
    RUNTIME DESTINATION bin
)

# ── Build summary ────────────────────────────────────────────────────────────
message(STATUS "NVCS ${PROJECT_VERSION} — build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "OpenSSL: ${OPENSSL_VERSION}")
message(STATUS "ZLIB: ${ZLIB_VERSION_STRING}")
