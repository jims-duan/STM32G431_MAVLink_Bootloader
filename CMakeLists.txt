cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME G431_MAVLink_Bootloader)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)


# ========== 添加编译优化选项 ==========
# 根据不同构建类型设置不同的优化级别
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug 模式：不优化，完全可调试
    add_compile_options(
        -O1                    # 无优化
        -ggdb3                 # 详细调试信息
        -fno-omit-frame-pointer # 保留帧指针
    )
    message(STATUS "Debug mode: Full debug symbols, no optimization")
    
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    # # Release 模式：激进体积优化（适合发布）
    # add_compile_options(
    #     -Os                    # 体积优化
    #     -ffunction-sections    # 函数分段
    #     -fdata-sections        # 数据分段
    #     -flto                  # 链接时优化
    #     -fomit-frame-pointer   # 省略帧指针
    # )
    # add_link_options(
    #     -Wl,--gc-sections      # 移除未使用的段
    #     -flto                  # 链接时优化
    #     -Wl,-Os                # 链接器优化体积
    # )
    # add_compile_definitions(NDEBUG)
    # message(STATUS "Release mode: Max size optimization, no debug info")

    # Debug 模式：不优化，完全可调试
    add_compile_options(
        -O1                    # 无优化
        -ggdb3                 # 详细调试信息
        -fno-omit-frame-pointer # 保留帧指针
    )
    message(STATUS "Debug mode: Full debug symbols, no optimization")
    
endif()

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${CMAKE_CURRENT_SOURCE_DIR}/APP/src/stmflash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/APP/src/iap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/APP/src/ring_buffer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/APP/src/ymodem.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/APP/inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
