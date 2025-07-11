# 定义需要的cmake版本
cmake_minimum_required(VERSION 3.10)
add_compile_definitions(APP_VERSION="${VERSION}")

# 显示编译过程
set(CMAKE_VERBOSE_MAKEFILE ON)

# 设置工程名字
project(dde-grand-search)

# Install settings
include(GNUInstallDirs)
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX /usr)
endif ()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DQT_DEBUG")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wl,--as-need")

#安全加固
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-all -z now")
if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "mips")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -z noexecstack -z relro")
endif()

# Find Qt version - auto detect Qt6 first, fallback to Qt5
if (NOT DEFINED QT_VERSION_MAJOR)
    # Try to find Qt6 first
    find_package(Qt6 QUIET COMPONENTS Core)
    if (Qt6_FOUND)
        set(QT_VERSION_MAJOR 6)
        message("   >>> Auto detected Qt version: 6")
    else()
        # Fallback to Qt5
        find_package(Qt5 QUIET COMPONENTS Core)
        if (Qt5_FOUND)
            set(QT_VERSION_MAJOR 5)
            message("   >>> Auto detected Qt version: 5")
        else()
            message(FATAL_ERROR "Neither Qt6 nor Qt5 found!")
        endif()
    endif()
else()
    message("   >>> Using specified Qt version: ${QT_VERSION_MAJOR}")
endif()

if (QT_VERSION_MAJOR MATCHES 6)
    set(DTK_VERSION_MAJOR 6)
else()
    set(DTK_VERSION_MAJOR "")
endif()
message("   >>> Found Qt version: ${QT_VERSION_MAJOR}")
message("   >>> Build with DTK: ${DTK_VERSION_MAJOR}")

set(QT_NS Qt${QT_VERSION_MAJOR})
set(DTK_NS Dtk${DTK_VERSION_MAJOR})

#tar -czvf antlr4.tar.gz ./antlr4
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/antlr4")
    message("find antlr4")
else()
    message("decompression antlr4")
    execute_process(
        COMMAND tar -xzvf antlr4.tar.gz
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty
        RESULT_VARIABLE ANTLR_UNZIP_RESULT
    )

    if (NOT ANTLR_UNZIP_RESULT EQUAL "0")
        message(FATAL_ERROR "tar antlr4.tar.gz failed: ${ANTLR_UNZIP_RESULT}")
    endif()
endif()

configure_file(${CMAKE_SOURCE_DIR}/src/environments.h.in ${CMAKE_SOURCE_DIR}/src/environments.h @ONLY)

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/3rdparty)
include_directories(${CMAKE_SOURCE_DIR}/src/tools)

# 平台库目录
set(LIB_PLATFORM_DIR "${CMAKE_INSTALL_PREFIX}/lib/${CMAKE_LIBRARY_ARCHITECTURE}")
message("LIB_PLATFORM_DIR: ${LIB_PLATFORM_DIR}")

#全局文件
FILE(GLOB GLOBAL_SRC "./src/global/*.h" "./src/global/*.cpp" "./src/global/accessibility/*.h")

# 国际化
if (NOT (${CMAKE_BUILD_TYPE} MATCHES "Debug"))
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast")

    # generate qm
    execute_process(COMMAND bash "translate_generation.sh"
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif ()

# 定义前端可执行程序名称
set(SEARCH_BIN_NAME dde-grand-search)

# 定义前端应用库目录
set(SEARCH_LIB_BASE_DIR "${LIB_PLATFORM_DIR}/${SEARCH_BIN_NAME}")

# 定义前端插件目录
set(SEARCH_PLUGIN_BASE_DIR "${SEARCH_LIB_BASE_DIR}/plugins")

# 定义前端预览插件目录
set(PLUGIN_PREVIEW_DIR "${SEARCH_PLUGIN_BASE_DIR}/preview")
add_definitions(-DPLUGIN_PREVIEW_DIR="${PLUGIN_PREVIEW_DIR}")

# 定义后端库名称
set(DAEMON_LIB_NAME libdde-grand-search-daemon)

# 定义后端可执行程序名称
set(DAEMON_BIN_NAME dde-grand-search-daemon)

# 定义后端应用库目录
set(DAEMON_LIB_BASE_DIR "${LIB_PLATFORM_DIR}/${DAEMON_BIN_NAME}")

# 定义后端插件目录
set(DAEMON_PLUGIN_DIR "${DAEMON_LIB_BASE_DIR}/plugins")

# 定义3rdparty目录
set(PROJECT_3RDPARTY_PATH "${CMAKE_SOURCE_DIR}/3rdparty")

# 定义后端扩展搜索插件目录
set(PLUGIN_SEARCHER_DIR "${DAEMON_PLUGIN_DIR}/searcher")
add_definitions(-DPLUGIN_SEARCHER_DIR="${PLUGIN_SEARCHER_DIR}")

# 获取系统版本
if (NOT DEFINED BUILD_OS_VERSION OR BUILD_OS_VERSION STREQUAL "")
    # 读取文件并提取Version值
    file(STRINGS "/etc/os-version" FILE_CONTENTS REGEX "^MajorVersion=")

    # 遍历文件内容，查找Version行
    foreach(LINE ${FILE_CONTENTS})
        # 检查是否是Version行
        if(LINE MATCHES "Version=([0-9]+)")
            # 提取版本号
            set(BUILD_OS_VERSION "${CMAKE_MATCH_1}")
        endif()
    endforeach()

    message("Get BUILD_OS_VERSION from os-version ${BUILD_OS_VERSION}")
else()
    message("catch BUILD_OS_VERSION ${BUILD_OS_VERSION}")
endif()

add_subdirectory(src/grand-search)
add_subdirectory(src/dde-grand-search-daemon)
add_subdirectory(src/grand-search-dock-plugin)
add_subdirectory(src/preview-plugin)
add_subdirectory(src/tools/semanticparser)
add_subdirectory(src/tools/luceneengine)
add_subdirectory(3rdparty/antlr4/antlr4-runtime)

# shell-plugin仅在Qt6下被include
if (${BUILD_OS_VERSION} GREATER_EQUAL "25" AND QT_VERSION_MAJOR EQUAL 6)
    add_subdirectory(src/grand-search-shell-plugin)
endif()
