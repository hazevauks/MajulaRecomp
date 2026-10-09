# Dark Souls II (Xbox 360): static recompilation host application.
#
# The game code is not in this repository: run the code generator first (tools/codegen.ps1), which writes generated/
# from your own Default.xex together with the CMake glue (generated/rexglue.cmake, made by `rexglue codegen`).
#
# Stage 1, Windows PC:
#   cmake --preset win-pc && cmake --build --preset win-pc        (from app/, inside tools\win\env.ps1)
# Stage 2, Nintendo Switch (devkitA64), later:
#   cmake -S app -B out/nx -G Ninja -DCMAKE_BUILD_TYPE=Release \
#     -DCMAKE_TOOLCHAIN_FILE=tools/switch/cmake/switch-devkitA64.cmake -DREXGLUE_SWITCH_NVK_SDK=<mesa sdk>

cmake_minimum_required(VERSION 3.25)
project(ds2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The SDK is built from source in this repository unless another tree is given.
set(REXSDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../sdk" CACHE PATH "Path to the ReXGlue SDK source tree")

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/rexglue.cmake")
    message(FATAL_ERROR "app/generated/rexglue.cmake is missing: run tools/codegen.ps1 first")
endif()
include(generated/rexglue.cmake)

# The build re-runs the generator by itself whenever an input in codegen.d changes (manifest, toml, xex, the generator
# binary). That rewrites app/generated and would silently drop the codegen flavour's post-codegen steps (no-volatile
# pch, direct calls, localized natives): they are appended to the same command. The Switch cross build has no
# generator target, so this only applies on the PC.
if(TARGET ds2_codegen)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_custom_command(OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/codegen.build.stamp" APPEND
        COMMAND "${CMAKE_COMMAND}" -E env "REXSDK_DIR=${REXSDK_DIR}"
                "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/post_codegen.py")
endif()

set(DS2_SOURCES
    src/main.cpp
    # Windows: logs fatal host exceptions the SDK does not report (illegal instruction, ...) with module + offset.
    src/ds2_crash_log.cpp
)

if(WIN32)
    add_executable(ds2 WIN32 ${DS2_SOURCES})
else()
    add_executable(ds2 ${DS2_SOURCES})
endif()

# Link the SDK. Stage 1 draws with ReXGlue's Xenos GPU emulation; the native renderer replaces it later.
rexglue_setup_target(ds2 GPU_PLUGINS xenos)

# Settings template next to the executable, only when there is none yet (the copy there is the user's to edit).
if(NOT REXGLUE_PLATFORM_SWITCH)
    add_custom_command(TARGET ds2 POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/ds2.toml -DDST=$<TARGET_FILE_DIR:ds2>/ds2.toml
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_if_missing.cmake"
        VERBATIM)
endif()

# The SDK compiles itself with these (directory scope), but they do not reach the generated game code, which is
# built in this directory. GCC's default for GNU C++ is -ffp-contract=fast: it fuses separate PPC fmul/fadd into
# FMA and results drift from the Xbox 360; strict aliasing is unsafe for recompiled code. Every target that holds
# generated code needs them. Clang (the PC build) gets them too, so the PC rounds exactly like the console.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    foreach(target ds2 ds2_recomp)
        if(TARGET ${target})
            target_compile_options(${target} PRIVATE -ffp-contract=off -fno-strict-aliasing)
        endif()
    endforeach()
endif()

if(REXGLUE_PLATFORM_SWITCH)
    # ds2_register.cpp is one generated function of tens of thousands of SetFunction calls that runs once at startup;
    # GCC -O3 spends many minutes on it alone (sa2-nx: 771-927 s). -O0 compiles it in seconds and changes nothing at
    # run time but a few microseconds of startup.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/ds2_register.cpp")
        set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/ds2_register.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -O0)
    endif()
    # The homebrew menu's entry. LTO, PGO, function ordering and the built-in settings come with the Switch stage
    # (sa2-nx app/CMakeLists.txt has the full set).
    rexglue_switch_add_nro(ds2
        NAME "Dark Souls II"
        AUTHOR "hazevauks"
        VERSION "0.0.1")
endif()
