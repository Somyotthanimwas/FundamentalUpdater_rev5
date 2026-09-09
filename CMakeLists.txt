cmake_minimum_required(VERSION 3.20)

project(FundamentalUpdater_rev5)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")

# ============================================================
# VCPKG / MINGW
# ============================================================

set(VCPKG_ROOT "/home/thanimwas/vcpkg")
set(VCPKG_TRIPLET "x64-mingw-static")

set(VCPKG_INCLUDE
    "${VCPKG_ROOT}/installed/${VCPKG_TRIPLET}/include"
)

set(VCPKG_LIB
    "${VCPKG_ROOT}/installed/${VCPKG_TRIPLET}/lib"
)

# ============================================================
# CURL
# ============================================================

# IMPORTANT:
# This project is cross-compiled from Linux/WSL
# to Windows MinGW.
#
# vcpkg CURL expects this target to exist:
# CURL::win32_winsock
#
# Therefore create it BEFORE find_package(CURL).

if(NOT TARGET CURL::win32_winsock)

    add_library(
        CURL::win32_winsock
        INTERFACE
        IMPORTED
        GLOBAL
    )

    set_target_properties(
        CURL::win32_winsock
        PROPERTIES
            INTERFACE_LINK_LIBRARIES
                "ws2_32"
    )

endif()

find_package(CURL REQUIRED)# ============================================================
# BOOST
#
# IMPORTANT:
# Use ONLY MinGW Boost from vcpkg.
# Do NOT use /usr/include/boost.
# ============================================================

set(BOOST_ROOT
    "${VCPKG_ROOT}/installed/${VCPKG_TRIPLET}"
)

set(Boost_NO_SYSTEM_PATHS ON)

find_package(Boost REQUIRED)

message(STATUS
    "Boost include: ${Boost_INCLUDE_DIRS}"
)

# ============================================================
# JSONCPP
# ============================================================

find_path(
    JSONCPP_INCLUDE_DIR
    json/json.h
    PATHS
        "${VCPKG_INCLUDE}"
        NO_DEFAULT_PATH
)

find_library(
    JSONCPP_LIBRARY
    NAMES jsoncpp
    PATHS
        "${VCPKG_LIB}"
        NO_DEFAULT_PATH
)

if(NOT JSONCPP_INCLUDE_DIR)

    message(FATAL_ERROR
        "JsonCpp header not found"
    )

endif()

if(NOT JSONCPP_LIBRARY)

    message(FATAL_ERROR
        "JsonCpp library not found"
    )

endif()

message(STATUS
    "JsonCpp include: ${JSONCPP_INCLUDE_DIR}"
)

message(STATUS
    "JsonCpp library: ${JSONCPP_LIBRARY}"
)

# ============================================================
# MAIN FUNDAMENTAL UPDATER V4
# ============================================================

add_executable(
    FundamentalUpdater_rev5

    src/main.cpp
    src/Application.cpp
    src/PriceUpdater.cpp
    src/PriceToAmiBroker.cpp
    src/EmbeddedScripts.cpp
    src/ProcessRunner.cpp
    src/Config.cpp
    src/LineNotifier.cpp
    src/FundamentalUpdater.cpp
)

target_include_directories(
    FundamentalUpdater_rev5

    PRIVATE

        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(
    FundamentalUpdater_rev5

    PRIVATE

        CURL::libcurl
)

# ============================================================
# FUNDAMENTAL CAPTURE TEST
# ============================================================

add_executable(
    FundamentalCaptureTest

    src/FundamentalCaptureTest.cpp
)

target_compile_definitions(
    FundamentalCaptureTest

    PRIVATE

        BOOST_ERROR_CODE_HEADER_ONLY
)

target_include_directories(
    FundamentalCaptureTest

    PRIVATE

        ${CMAKE_SOURCE_DIR}/include
        ${JSONCPP_INCLUDE_DIR}
        ${VCPKG_INCLUDE}
)

target_link_libraries(
    FundamentalCaptureTest

    PRIVATE

        CURL::libcurl
        ${JSONCPP_LIBRARY}
)

# ============================================================
# FUNDAMENTAL CAPTURE REAL
# ============================================================

add_executable(
    FundamentalCapture

    src/FundamentalCapture.cpp
)

target_compile_definitions(
    FundamentalCapture

    PRIVATE

        BOOST_ERROR_CODE_HEADER_ONLY
        BOOST_SYSTEM_NO_DEPRECATED
)

target_include_directories(
    FundamentalCapture

    PRIVATE

        ${CMAKE_SOURCE_DIR}/include
        ${JSONCPP_INCLUDE_DIR}
        ${VCPKG_INCLUDE}
)

target_link_libraries(
    FundamentalCapture

    PRIVATE

        CURL::libcurl
        ${JSONCPP_LIBRARY}
)

# ============================================================
# FUNDAMENTAL CSV CONVERTER
#
# JSON -> fundamental_v4.csv
# ============================================================

add_executable(
    FundamentalCsvConverter

    src/FundamentalCsvConverter.cpp
)

target_include_directories(
    FundamentalCsvConverter

    PRIVATE

        ${CMAKE_SOURCE_DIR}/include
        ${JSONCPP_INCLUDE_DIR}
)

target_link_libraries(
    FundamentalCsvConverter

    PRIVATE

        ${JSONCPP_LIBRARY}
)

# ============================================================
# EMBED FUNDAMENTAL V4 CSV
#
# fundamental_v4.csv is embedded into the EXE at build time.
# User does NOT need to distribute the CSV separately.
# ============================================================

set(FUNDAMENTAL_V4_CSV
    "${CMAKE_SOURCE_DIR}/Data/Fundamental/fundamental_v4.csv"
)

set(EMBEDDED_DIR
    "${CMAKE_BINARY_DIR}/generated"
)

set(EMBEDDED_FUNDAMENTAL_CPP
    "${EMBEDDED_DIR}/EmbeddedFundamentalData.cpp"
)

set(EMBEDDED_FUNDAMENTAL_H
    "${CMAKE_SOURCE_DIR}/include/EmbeddedFundamentalData.h"
)

add_custom_command(
    OUTPUT
        "${EMBEDDED_FUNDAMENTAL_CPP}"

    COMMAND
        ${CMAKE_COMMAND} -E make_directory
        "${EMBEDDED_DIR}"

    COMMAND
        python3
        "${CMAKE_SOURCE_DIR}/tools/embed_csv.py"
        "${FUNDAMENTAL_V4_CSV}"
        "${EMBEDDED_FUNDAMENTAL_CPP}"

    DEPENDS
        "${FUNDAMENTAL_V4_CSV}"
        "${CMAKE_SOURCE_DIR}/tools/embed_csv.py"
        "${EMBEDDED_FUNDAMENTAL_H}"

    COMMENT
        "Embedding FINAL fundamental_v4.csv into EXE"
)

add_custom_target(
    EmbeddedFundamentalData
    DEPENDS
        "${EMBEDDED_FUNDAMENTAL_CPP}"
)

add_dependencies(
    FundamentalUpdater_rev5
    EmbeddedFundamentalData
)

target_sources(
    FundamentalUpdater_rev5
    PRIVATE
        "${EMBEDDED_FUNDAMENTAL_CPP}"
)
# ============================================================
# INSTALL RUNTIME DATA
#
# Copy required runtime files beside EXE
# ============================================================

set(RUNTIME_FUNDAMENTAL_JSON
    "${CMAKE_SOURCE_DIR}/Data/Fundamental/JSON"
)

set(RUNTIME_FUNDAMENTAL_DIR
    "${CMAKE_SOURCE_DIR}/Data/Fundamental"
)

# ------------------------------------------------------------
# Create install layout
# ------------------------------------------------------------

install(
    TARGETS
        FundamentalUpdater_rev5
        FundamentalCapture
        FundamentalCsvConverter
    RUNTIME DESTINATION .
)

# ------------------------------------------------------------
# Fundamental JSON
# ------------------------------------------------------------

install(
    DIRECTORY
        "${RUNTIME_FUNDAMENTAL_JSON}/"
    DESTINATION
        "Data/Fundamental/JSON"
)

# ------------------------------------------------------------
# Excel workflow
#
# Set_Amibroker_ok.xlsm and run_v4_excel.ps1 are embedded
# inside FundamentalUpdater_rev5.exe.
#
# Do NOT install either file separately.
# ------------------------------------------------------------