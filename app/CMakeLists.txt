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
    # Recordings of the game image for testing (cvar ds2_video_s; tools/frames_to_png.py makes a contact sheet).
    src/ds2_capture.cpp
    # Scripted pad for unattended test runs (cvar ds2_input_script; tools/win/boot_test.ps1 -Pad).
    src/ds2_input_script.cpp
    # Stage 2, phase A: wraps the game's Direct3D candidates and logs them (cvar ds2_d3d_trace, off by default).
    src/ds2_d3d_trace.cpp
    # Stage 2: the native renderer (src/me, masseffect-nx's PM4-ring Vulkan renderer as adapted by banjo-nx and
    # sa2-nx). It replaces the emulated Xenos GPU with ds2_renderer_native = true; off by default on the PC until it
    # draws this game right.
    src/me/me_native_system.cpp
    src/me/me_ring_wait.cpp
    src/me/me_renderdoc.cpp
    src/me/native/ds2_native_draws.cpp
    src/me/native/ds2_deferred_recording.cpp
    src/me/native/ds2_native_targets.cpp
    src/me/native/ds2_native_shaders.cpp
    src/me/native/ds2_native_textures_pool.cpp
    src/me/native/ds2_shader_library.cpp
    src/me/native/ds2_native_glue.cpp
    # This game's D3D addresses (draw-time shader load, Draw*).
    src/me/ds2_me_hooks.cpp
    # Render resolution per console mode (docked / handheld, live) and MSAA: ds2_render_height_*, ds2_msaa.
    src/me/ds2_render_mode.cpp
    # Switchable enhancements: masters ds2_cpu_enhancements / ds2_noncpu_enhancements, one key each, a startup line
    # and a 10 s use report.
    src/cpu/ds2_enhancements.cpp
    src/cpu/ds2_gpu_boost.cpp
)
if(NOT REXGLUE_PLATFORM_SWITCH)
    # Switch profiler counters the native renderer calls (no-ops on PC).
    list(APPEND DS2_SOURCES src/me/me_pc_stubs.cpp)
endif()

# The codegen flavour (tools/codegen.ps1 -Flavor) names the build in the first "[enh]" log line.
set(DS2_FLAVOR_FILE "${CMAKE_CURRENT_SOURCE_DIR}/generated/codegen_flavor.txt")
set(DS2_BUILD_FLAVOR "classic codegen")
if(EXISTS "${DS2_FLAVOR_FILE}")
    file(READ "${DS2_FLAVOR_FILE}" DS2_BUILD_FLAVOR)
    string(STRIP "${DS2_BUILD_FLAVOR}" DS2_BUILD_FLAVOR)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${DS2_FLAVOR_FILE}")
endif()
set_source_files_properties(src/cpu/ds2_enhancements.cpp PROPERTIES
    COMPILE_DEFINITIONS "DS2_BUILD_FLAVOR=\"${DS2_BUILD_FLAVOR}\"")

if(WIN32)
    add_executable(ds2 WIN32 ${DS2_SOURCES})
else()
    add_executable(ds2 ${DS2_SOURCES})
endif()

# Link the SDK. Stage 1 draws with ReXGlue's Xenos GPU emulation; the native renderer replaces it later.
rexglue_setup_target(ds2 GPU_PLUGINS xenos)

# SPIR-V initializers of the native renderer's EDRAM transfer / utility shaders (src/me/native/shaders), compiled
# on the host with glslangValidator (tools/win/build_glslang.ps1 builds it from the SDK's glslang). The Switch build
# uses the same host tool: SPIR-V is platform independent.
set(DS2_GLSLANG_VALIDATOR "${CMAKE_CURRENT_SOURCE_DIR}/../out/glslang/StandAlone/glslangValidator${CMAKE_EXECUTABLE_SUFFIX}"
    CACHE FILEPATH "Host glslangValidator")
set(DS2_GENERATED_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated-shaders")
set(DS2_EDRAM_SHADER_INCLUDES)
foreach(_shader ds2_edram_7e3_to_rgba8 ds2_edram_rgba8_to_7e3 ds2_edram_16f_to_16f me_resolve_exp_bias me_edram_depth_to_rgba8 me_edram_depth_to_16f me_edram_import me_edram_color_to_depth me_edram_depth_to_depth me_edram_depth_to_stencil me_edram_color_to_stencil me_edram_depth_to_depth_msaa2 me_edram_depth_msaa2_to_depth_1x me_edram_depth_to_stencil_msaa2 me_edram_depth_msaa2_to_stencil_1x me_edram_raw64_to_rgba8 me_edram_rgba8_to_raw64 me_edram_raw64_to_16f me_edram_16f_to_raw64 me_edram_raw64_to_raw64 me_edram_depth_to_raw64 me_edram_raw64_to_depth me_depth_resolve_guestspace me_depth_resolve_guestspace_msaa2 me_edram_stencil_to_buffer me_edram_stencil_msaa2_to_buffer me_edram_color_stencil_to_buffer me_edram_color_to_color me_edram_r64_to_r64 ds2_resolve_copy ds2_depth_resolve_ms)
    set(_extension comp)
    if(_shader STREQUAL "me_edram_import")
        set(_extension vert)
    elseif(_shader MATCHES "^me_edram_(color_to_depth|depth_to_depth|depth_to_stencil|color_to_stencil|depth_to_depth_msaa2|depth_msaa2_to_depth_1x|depth_to_stencil_msaa2|depth_msaa2_to_stencil_1x|raw64_to_depth|color_to_color|r64_to_r64)$" OR _shader STREQUAL "ds2_resolve_copy" OR
           _shader STREQUAL "ds2_depth_resolve_ms")
        set(_extension frag)
    endif()
    set(_source "${CMAKE_CURRENT_SOURCE_DIR}/src/me/native/shaders/${_shader}.${_extension}")
    set(_output "${DS2_GENERATED_SHADER_DIR}/${_shader}.inc")
    set(_target_flags)
    if(_shader MATCHES "^me_edram_(stencil_to_buffer|color_stencil_to_buffer|stencil_msaa2_to_buffer|depth_to_depth_msaa2|depth_msaa2_to_depth_1x|depth_to_stencil_msaa2|depth_msaa2_to_stencil_1x)$" OR _shader STREQUAL "me_depth_resolve_guestspace_msaa2")
        set(_target_flags --target-env vulkan1.2)
        set(_output "${DS2_GENERATED_SHADER_DIR}/${_shader}_generated.inc")
    endif()
    add_custom_command(OUTPUT "${_output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DS2_GENERATED_SHADER_DIR}"
        COMMAND "${DS2_GLSLANG_VALIDATOR}" -V ${_target_flags} -x -o "${_output}" "${_source}"
        DEPENDS "${_source}" VERBATIM)
    list(APPEND DS2_EDRAM_SHADER_INCLUDES "${_output}")
endforeach()
add_custom_target(ds2_edram_shaders DEPENDS ${DS2_EDRAM_SHADER_INCLUDES})
add_dependencies(ds2 ds2_edram_shaders)
target_include_directories(ds2 PRIVATE
    "${DS2_GENERATED_SHADER_DIR}"
    src
    src/me
    src/me/native
    src/me/native/shaders
    "${REXSDK_DIR}/src/core"
    # Precompiled SPIR-V of the SDK presenter (vulkan_spirv/guest_output_*): the renderer draws into its output.
    "${REXSDK_DIR}/src/ui/shaders")

# CPU-only SDK sources the native renderer reuses (draw-extent estimation, shader microcode interpretation): the
# GPU plugin's private symbols cannot satisfy application-side use.
target_sources(ds2 PRIVATE
    "${REXSDK_DIR}/src/graphics/register_file.cpp"
    "${REXSDK_DIR}/src/graphics/format/ucode.cpp"
    "${REXSDK_DIR}/src/graphics/pipeline/shader/interpreter.cpp")

# The renderer includes xxhash.h and renderdoc_app.h directly; when the SDK is built from source they are private to
# its libraries.
foreach(_dep xxhash renderdoc)
    if(TARGET ${_dep})
        target_include_directories(ds2 PRIVATE $<TARGET_PROPERTY:${_dep},INTERFACE_INCLUDE_DIRECTORIES>)
    endif()
endforeach()

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
    # The icon is not in the repository (game artwork): assets/icon.jpg, a 256x256 baseline JPEG, when it is there;
    # libnx's default icon otherwise.
    set(DS2_NRO_ICON "")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon.jpg")
        set(DS2_NRO_ICON ICON "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon.jpg")
    endif()
    rexglue_switch_add_nro(ds2
        NAME "Dark Souls II"
        AUTHOR "hazevauks"
        VERSION "0.0.2"
        ${DS2_NRO_ICON})
endif()
