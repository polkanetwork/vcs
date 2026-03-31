cmake_minimum_required(VERSION 3.16)
project(nvcs VERSION 0.1.0 LANGUAGES CXX)

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
find_package(OpenGL REQUIRED)
find_program(CARGO cargo REQUIRED)
find_package(glfw3 REQUIRED)

if(TARGET OpenGL::GL)
  set(OPENGL_LIB OpenGL::GL)
elseif(DEFINED OPENGL_gl_LIBRARY)
  set(OPENGL_LIB ${OPENGL_gl_LIBRARY})
elseif(DEFINED OPENGL_LIBRARY)
  set(OPENGL_LIB ${OPENGL_LIBRARY})
else()
  message(FATAL_ERROR "Could not resolve OpenGL library target")
endif()

# ── Core library ─────────────────────────────────────────────────────────────
set(RUST_VERSION_SYSTEM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rust/version_system)
set(RUST_VERSION_SYSTEM_LIB ${RUST_VERSION_SYSTEM_DIR}/target/release/libversion_system.a)

add_custom_command(
    OUTPUT ${RUST_VERSION_SYSTEM_LIB}
    COMMAND ${CARGO} build --release
    WORKING_DIRECTORY ${RUST_VERSION_SYSTEM_DIR}
    COMMENT "Building Rust version_system library"
    VERBATIM
)

add_custom_target(version_system_rust ALL DEPENDS ${RUST_VERSION_SYSTEM_LIB})

add_library(version_system STATIC IMPORTED GLOBAL)
set_target_properties(version_system PROPERTIES
    IMPORTED_LOCATION ${RUST_VERSION_SYSTEM_LIB}
)
add_dependencies(version_system version_system_rust)

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
    src/version_system.cpp
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
        version_system
)

# Suppress noisy warnings from third-party headers
target_compile_options(nvcs_core PRIVATE
    -Wno-deprecated-declarations
)

# ── Server support library ───────────────────────────────────────────────────
add_library(nvcs_server STATIC server/server.cpp)
target_link_libraries(nvcs_server PUBLIC nvcs_core)
target_include_directories(nvcs_server
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_compile_options(nvcs_server PRIVATE -Wno-deprecated-declarations)

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
    cli/commands/version.cpp
)

add_executable(nvcs ${NVCS_CLI_SOURCES})
target_link_libraries(nvcs PRIVATE nvcs_core)
target_include_directories(nvcs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_compile_definitions(nvcs PRIVATE NVCS_VERSION="${PROJECT_VERSION}")
target_compile_options(nvcs PRIVATE -Wno-deprecated-declarations)

# ── Server ───────────────────────────────────────────────────────────────────
add_executable(nvcs-server server/main.cpp)
target_link_libraries(nvcs-server PRIVATE nvcs_server)
target_include_directories(nvcs-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_compile_options(nvcs-server PRIVATE -Wno-deprecated-declarations)

if(TARGET glfw3::glfw)
  set(GLFW_LIB glfw3::glfw)
elseif(TARGET glfw)
  set(GLFW_LIB glfw)
elseif(TARGET glfw3)
  set(GLFW_LIB glfw3)
else()
  message(FATAL_ERROR "Could not find GLFW3 target")
endif()

add_executable(nvcs-server-gui gui/main.cpp)
target_link_libraries(nvcs-server-gui PRIVATE nvcs_server ${GLFW_LIB} ${OPENGL_LIB})
target_include_directories(nvcs-server-gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_compile_options(nvcs-server-gui PRIVATE -Wno-deprecated-declarations)
if(APPLE)
  target_compile_definitions(nvcs-server-gui PRIVATE GL_SILENCE_DEPRECATION)
endif()

add_executable(nvcs-client-gui gui/client.cpp)
target_link_libraries(nvcs-client-gui PRIVATE nvcs_core ${GLFW_LIB} ${OPENGL_LIB})
target_include_directories(nvcs-client-gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_compile_options(nvcs-client-gui PRIVATE -Wno-deprecated-declarations)
if(APPLE)
  target_compile_definitions(nvcs-client-gui PRIVATE GL_SILENCE_DEPRECATION)
endif()

# ── Install ──────────────────────────────────────────────────────────────────
install(TARGETS nvcs nvcs-server nvcs-server-gui nvcs-client-gui
    RUNTIME DESTINATION bin
)

# ── Build summary ────────────────────────────────────────────────────────────
message(STATUS "NVCS ${PROJECT_VERSION} — build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "OpenSSL: ${OPENSSL_VERSION}")
message(STATUS "ZLIB: ${ZLIB_VERSION_STRING}")
