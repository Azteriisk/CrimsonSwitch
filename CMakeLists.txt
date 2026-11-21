cmake_minimum_required(VERSION 3.20)
project(CrimsonDesertEarlySwitch LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(UNICODE _UNICODE)

include(FetchContent)

# 1. MinHook
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG v1.3.3
)
FetchContent_GetProperties(minhook)
if(NOT minhook_POPULATED)
    FetchContent_Populate(minhook)
endif()

add_library(minhook STATIC
    ${minhook_SOURCE_DIR}/src/buffer.c
    ${minhook_SOURCE_DIR}/src/hook.c
    ${minhook_SOURCE_DIR}/src/trampoline.c
    ${minhook_SOURCE_DIR}/src/hde/hde64.c
    ${minhook_SOURCE_DIR}/src/hde/hde32.c
)
target_compile_options(minhook PRIVATE -include intrin.h)
target_include_directories(minhook PUBLIC
    ${minhook_SOURCE_DIR}/include
)

include_directories(
    include
    ${minhook_SOURCE_DIR}/include
)

set(CORE_SOURCES
    src/Logger.cpp
    src/Config.cpp
    src/PatternScan.cpp
    src/SwitchManager.cpp
    src/EarlyUnlockHook.cpp
)

# Automated Test Harness
add_executable(crimson_switch_harness
    ${CORE_SOURCES}
    src/main.cpp
)
target_link_libraries(crimson_switch_harness PRIVATE minhook)

# Injected Mod DLL (CrimsonSwitch.dll)
add_library(CrimsonSwitch SHARED
    ${CORE_SOURCES}
    src/dllmain.cpp
)
target_link_libraries(CrimsonSwitch PRIVATE minhook)

# Standalone Launcher
add_executable(CrimsonSwitchLauncher
    src/launcher.cpp
)

if (MSVC)
    target_compile_options(crimson_switch_harness PRIVATE /W4 /permissive-)
    target_compile_options(CrimsonSwitch PRIVATE /W4 /permissive-)
    target_compile_options(CrimsonSwitchLauncher PRIVATE /W4 /permissive-)
else()
    target_compile_options(crimson_switch_harness PRIVATE -Wall -Wextra -pedantic -Wno-unused-parameter)
    target_compile_options(CrimsonSwitch PRIVATE -Wall -Wextra -pedantic -Wno-unused-parameter)
    target_compile_options(CrimsonSwitchLauncher PRIVATE -Wall -Wextra -pedantic -Wno-unused-parameter)
endif()
