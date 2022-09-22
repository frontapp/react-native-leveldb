cmake_minimum_required(VERSION 3.9.0)
project(react-native-leveldb)

set (PACKAGE_NAME "react-native-leveldb")
set (BUILD_DIR ${CMAKE_SOURCE_DIR}/build)
set (RN_SO_DIR ${NODE_MODULES_DIR}/react-native/ReactAndroid/src/main/jni/first-party/react/jni)

set (LEVELDB_BUILD_TESTS OFF CACHE INTERNAL "Really don't build LevelDB tests") # FORCE implied by INTERNAL
set (LEVELDB_BUILD_BENCHMARKS OFF CACHE INTERNAL "Really don't build LevelDB benchmarks")
set (LEVELDB_INSTALL OFF CACHE INTERNAL "Really don't install LevelDB")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

add_subdirectory(../cpp/leveldb leveldb)

add_library(${PACKAGE_NAME}  # Library name
        SHARED  # Sets the library as a shared library.
        ../cpp/react-native-leveldb.cpp
        ../cpp/packer.cpp
        ../cpp/mpack.c
        cpp-adapter.cpp
        )

file (GLOB LIBFBJNI_INCLUDE_DIR "${BUILD_DIR}/fbjni-*-headers.jar/")

target_include_directories(
        ${PACKAGE_NAME}
        PRIVATE
        # --- fbjni ---
        "${LIBFBJNI_INCLUDE_DIR}"
        ../cpp/leveldb
        "${NODE_MODULES_DIR}/react-native/React"
        "${NODE_MODULES_DIR}/react-native/React/Base"
        "${NODE_MODULES_DIR}/react-native/ReactCommon/jsi"
)

file (GLOB LIBRN_DIR "${BUILD_DIR}/react-native-0*/jni/${ANDROID_ABI}")


find_library(
        REACT_NATIVE_JNI_LIB
        reactnativejni
        PATHS ${LIBRN_DIR}
        NO_CMAKE_FIND_ROOT_PATH
)

# RN 0.66 distributes libjsi.so, can be used instead of compiling jsi.cpp manually.
find_library(
        JSI_LIB
        jsi
        PATHS ${LIBRN_DIR}
        NO_CMAKE_FIND_ROOT_PATH
)

find_library(
        LOG_LIB
        log
)

set_target_properties(
        ${PACKAGE_NAME} PROPERTIES
        CXX_STANDARD 14
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(
        ${PACKAGE_NAME}
        leveldb
        ${LOG_LIB}
        ${JSI_LIB}
        ${REACT_NATIVE_JNI_LIB}
        android
)
