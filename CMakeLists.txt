cmake_minimum_required(VERSION 3.18)

add_subdirectory(tools)

set(LLVM_MOS_PLATFORM rp6502)
find_package(llvm-mos-sdk REQUIRED)

project(RPGalaxy C CXX ASM)

# Define the option (Default is ON/Native) 
option(USE_NATIVE_OPL2 "Use the RIA native OPL2 support" ON)

if(USE_NATIVE_OPL2)
    add_definitions(-DUSE_NATIVE_OPL2)
    message(STATUS "Targeting: Native RIA OPL2")
else()
    message(STATUS "Targeting: FPGA TinyFPGA Sound Card")
endif()

add_executable(RPGalaxy)
rp6502_executable(RPGalaxy
    DATA file
    RESET file
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.hlp
)
target_sources(RPGalaxy PRIVATE
    src/main.c
    src/opl.c
    src/instruments.c
    src/graphics.c
    src/galaxy.c
)

target_link_libraries(RPGalaxy PRIVATE m)
