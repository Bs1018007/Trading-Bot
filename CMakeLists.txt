
# ============================================================================
# FILE: CMakeLists.txt
# ============================================================================
cmake_minimum_required(VERSION 3.15)
project(BybitTradingBot LANGUAGES CXX)

# ============================================================================
# Compiler Configuration
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags for maximum performance
set(CMAKE_CXX_FLAGS_RELEASE
    "-O3 -march=native -mtune=native -DNDEBUG -funroll-loops -ffast-math -finline-functions -fomit-frame-pointer"
)

set(CMAKE_CXX_FLAGS_DEBUG
    "-O0 -g -Wall -Wextra -Wpedantic"
)

# ============================================================================
# Find Required Packages
# ============================================================================
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(CURL REQUIRED)

# ============================================================================
# Platform-Specific Configuration
# ============================================================================
if(APPLE)
    message(STATUS "Configuring for macOS")
    
    # libwebsockets
    set(LIBWEBSOCKETS_PREFIX "/opt/homebrew/opt/libwebsockets")
    if(NOT EXISTS ${LIBWEBSOCKETS_PREFIX})
        set(LIBWEBSOCKETS_PREFIX "/usr/local/opt/libwebsockets")
    endif()
    set(LIBWEBSOCKETS_INCLUDE_DIRS "${LIBWEBSOCKETS_PREFIX}/include")
    find_library(LIBWEBSOCKETS_LIBRARIES 
        NAMES websockets
        PATHS "${LIBWEBSOCKETS_PREFIX}/lib"
        REQUIRED)
    
    # CURL
    set(CURL_PREFIX "/opt/homebrew/opt/curl")
    if(NOT EXISTS ${CURL_PREFIX})
        set(CURL_PREFIX "/usr/local/opt/curl")
    endif()
    set(CURL_INCLUDE_DIRS "${CURL_PREFIX}/include")
    find_library(CURL_LIBRARIES
        NAMES curl
        PATHS "${CURL_PREFIX}/lib"
        REQUIRED)
    
    message(STATUS "Found libwebsockets: ${LIBWEBSOCKETS_PREFIX}")
    message(STATUS "Found CURL: ${CURL_PREFIX}")
    
elseif(UNIX AND NOT APPLE)
    message(STATUS "Configuring for Linux")
    
    pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)
    pkg_check_modules(CURL REQUIRED libcurl)
    
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# ============================================================================
# Third-Party Library Paths
# ============================================================================
set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/third_party)

# simdjson
set(SIMDJSON_DIR ${THIRD_PARTY_DIR}/simdjson)
set(SIMDJSON_INCLUDE_DIR ${SIMDJSON_DIR}/singleheader)
set(SIMDJSON_LIB ${SIMDJSON_DIR}/build/libsimdjson.a)

# Aeron
set(AERON_DIR ${THIRD_PARTY_DIR}/aeron)
set(AERON_CLIENT_INCLUDE_DIR ${AERON_DIR}/aeron-client/src/main/cpp)
set(AERON_DRIVER_INCLUDE_DIR ${AERON_DIR}/aeron-driver/src/main/cpp)
set(AERON_C_INCLUDE_DIR ${AERON_DIR}/aeron-driver/src/main/c)
set(AERON_LIB_DIR ${AERON_DIR}/cppbuild/Release/lib)

# ============================================================================
# Find Aeron Libraries
# ============================================================================
find_library(AERON_C_LIB
    NAMES aeron
    PATHS ${AERON_LIB_DIR}
    REQUIRED
)

find_library(AERON_CPP_LIB
    NAMES aeron_client
    PATHS ${AERON_LIB_DIR}
    REQUIRED
)

find_library(AERON_DRIVER_LIB
    NAMES aeron_driver
    PATHS ${AERON_LIB_DIR}
    REQUIRED
)

# ============================================================================
# Source Files
# ============================================================================
set(SOURCES
    # Main
    src/main.cpp
    
    # Configuration
    src/config/BotConfiguration.cpp
    
    # Core
    src/core/OrderBook.cpp
    src/core/OrderBookManager.cpp
    src/core/SymbolManager.cpp
    
    # Network
    src/network/BybitRestClient.cpp
    src/network/BybitWebSocketClient.cpp
    
    # Messaging
    src/messaging/GlobalMediaDriver.cpp
    src/messaging/AeronPublisher.cpp
    src/messaging/SBEEncoder.cpp
    
    # Utils
    src/utils/DataLogger.cpp
    src/utils/PerformanceMonitor.cpp
    src/trading/TradingEngine.cpp
)

# ============================================================================
# Create Executable
# ============================================================================
add_executable(trading_bot ${SOURCES})

# ============================================================================
# Include Directories
# ============================================================================
target_include_directories(trading_bot PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${SIMDJSON_INCLUDE_DIR}
    ${AERON_CLIENT_INCLUDE_DIR}
    ${AERON_DRIVER_INCLUDE_DIR}
    ${AERON_C_INCLUDE_DIR}
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/generated
)

# ============================================================================
# Link Libraries
# ============================================================================
target_link_libraries(trading_bot PRIVATE
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    ${LIBWEBSOCKETS_LIBRARIES}
    ${CURL_LIBRARIES}
    ${SIMDJSON_LIB}
    ${AERON_CPP_LIB}
    ${AERON_DRIVER_LIB}
    ${AERON_C_LIB}
    ${CMAKE_DL_LIBS}
    
)

# Platform-specific linking
if(APPLE)
    target_link_directories(trading_bot PRIVATE 
        "/opt/homebrew/opt/libwebsockets/lib"
        "/opt/homebrew/opt/curl/lib"
        "/usr/local/opt/libwebsockets/lib"
        "/usr/local/opt/curl/lib"
        ${AERON_LIB_DIR}
    )
    # Set rpath so Aeron libraries can be found at runtime
    set_target_properties(trading_bot PROPERTIES
        BUILD_RPATH "${AERON_LIB_DIR}"
        INSTALL_RPATH "${AERON_LIB_DIR}"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

# ============================================================================
# Build Diagnostics
# ============================================================================
message(STATUS "========================================")
message(STATUS "Bybit Trading Bot - Build Configuration")
message(STATUS "========================================")
message(STATUS "C++ Standard        : ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type          : ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler            : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "System              : ${CMAKE_SYSTEM_NAME}")
message(STATUS "----------------------------------------")
message(STATUS "Third-Party Libraries:")
message(STATUS "  simdjson          : ${SIMDJSON_DIR}")
message(STATUS "  Aeron             : ${AERON_DIR}")
message(STATUS "  Aeron client inc  : ${AERON_CLIENT_INCLUDE_DIR}")
message(STATUS "  Aeron driver inc  : ${AERON_DRIVER_INCLUDE_DIR}")
message(STATUS "  Aeron C lib       : ${AERON_C_LIB}")
message(STATUS "  Aeron C++ lib     : ${AERON_CPP_LIB}")
message(STATUS "  Aeron driver lib  : ${AERON_DRIVER_LIB}")
message(STATUS "----------------------------------------")
message(STATUS "System Libraries:")
message(STATUS "  libwebsockets     : ${LIBWEBSOCKETS_LIBRARIES}")
message(STATUS "  CURL              : ${CURL_LIBRARIES}")
message(STATUS "  OpenSSL           : ${OPENSSL_VERSION}")
message(STATUS "========================================")

# ============================================================================
# UTILITY TOOLS
# ============================================================================

# 1. Aeron Spy (Data Receiver)
#    This tool connects to the same Aeron channel to verify data flow.
add_executable(aeron_spy src/utils/AeronSpy.cpp)

target_include_directories(aeron_spy PRIVATE
    ${AERON_CLIENT_INCLUDE_DIR}
    ${AERON_DRIVER_INCLUDE_DIR}
    ${AERON_C_INCLUDE_DIR}
)

target_link_libraries(aeron_spy PRIVATE
    Threads::Threads
    ${AERON_CPP_LIB}
    ${AERON_DRIVER_LIB}
    ${AERON_C_LIB}
)

# Ensure Mac finds the libraries at runtime
if(APPLE)
    set_target_properties(aeron_spy PROPERTIES
        BUILD_RPATH "${AERON_LIB_DIR}"
        INSTALL_RPATH "${AERON_LIB_DIR}"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()