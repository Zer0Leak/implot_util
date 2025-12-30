cmake_minimum_required(VERSION 3.22)

if(PROJECT_IS_TOP_LEVEL)
    # ---------- (optional) force Clang: do this BEFORE project() ----------
    if(NOT DEFINED CMAKE_C_COMPILER OR NOT DEFINED CMAKE_CXX_COMPILER)
        set(CMAKE_C_COMPILER clang)
        set(CMAKE_CXX_COMPILER clang++)
    endif()

    # ===== Global defaults =====
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
    endif()

    # --- Global Linker Optimization ---
    if(UNIX AND NOT APPLE)
        if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.29")
            set(CMAKE_LINKER_TYPE MOLD)
            message(STATUS "Global Linker: Using MOLD (via CMAKE_LINKER_TYPE)")
        else()
            find_program(MOLD_LINKER mold)
            if(MOLD_LINKER)
                # FORCE the linker choice in the global flag strings
                set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=mold")
                set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=mold")
                message(STATUS "Global Linker: Forced MOLD via CMAKE_EXE_LINKER_FLAGS")
            else()
                find_program(LLD_LINKER ld.lld lld)
                if(LLD_LINKER)
                    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
                    message(STATUS "Global Linker: Forced LLD")
                endif()
            endif()
        endif()
    endif()

    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

endif()


# ---------- single place to rename your target ----------
set(TARGET_NAME implot_util)

project(${TARGET_NAME} LANGUAGES CXX)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")


# ===== Target =====
option(IMPLOT_UTIL_BUILD_SHARED "Build implot_util as a shared library" OFF)

if(IMPLOT_UTIL_BUILD_SHARED)
    add_library(${TARGET_NAME} SHARED)
else()
    add_library(${TARGET_NAME} STATIC)
endif()

target_sources(${TARGET_NAME} PRIVATE
    src/implot_util.cpp
    src/implot_engine.cpp
    src/vulkan_helper.cpp
)





target_compile_options(${TARGET_NAME} PRIVATE
    $<$<CONFIG:Debug>:-O0;-g;-fno-omit-frame-pointer>
    $<$<CONFIG:Debug>:-gsplit-dwarf>
    $<$<CONFIG:Release>:-O3;-DNDEBUG;-march=native;-mtune=native>
    $<$<CONFIG:RelWithDebInfo>:-O3;-g;-DNDEBUG;-fno-omit-frame-pointer>
    $<$<CONFIG:MinSizeRel>:-Os;-DNDEBUG>
)

target_link_options(${TARGET_NAME} PRIVATE
    $<$<CONFIG:Debug>:-Wl,--gdb-index> # Linker creates the GDB index
)

# Enable Link Time Optimization (IPO/LTO) for Release when supported
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported)
    set_property(TARGET ${TARGET_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    # Also pass explicit -flto for compilers that honor it (GCC/Clang)
    # target_link_options(${TARGET_NAME} PRIVATE $<$<CONFIG:Release>:-flto>)
else()
    message(STATUS "IPO/LTO not supported: ${ipo_error}")
endif()



# ===== Warnings =====
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()



target_include_directories(${TARGET_NAME}
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)


# set_target_properties(${TARGET_NAME} PROPERTIES
#    CXX_STANDARD 23
#    CXX_STANDARD_REQUIRED YES
#    CXX_EXTENSIONS NO
# )
target_compile_features(${TARGET_NAME} PUBLIC cxx_std_23)


if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Only apply if Clang is older than version 19.0
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.0")
        # lies to libstdc++ that clang fully support c++ 202002. I hope it is safe. I want std::expect
        # message(STATUS "Patching Clang < 19 for libstdc++ std::expect compatibility")
        # add_compile_options(-Wno-builtin-macro-redefined -D__cpp_concepts=202002L)
    endif()
endif()

# ===== Dependencies =====

if((NOT TARGET ImGui::imgui AND NOT TARGET imgui) OR (NOT TARGET ImPlot::implot AND NOT TARGET implot))

    include(FetchContent)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW REQUIRED IMPORTED_TARGET glfw3)

    find_package(Vulkan REQUIRED)

endif()

if(NOT TARGET ImGui::imgui AND NOT TARGET imgui)

    # --- ImGui ---
    FetchContent_Declare(
        imgui_src
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.5
    )
    FetchContent_MakeAvailable(imgui_src)

    add_library(imgui STATIC
        ${imgui_src_SOURCE_DIR}/imgui.cpp
        ${imgui_src_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_src_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_src_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_src_SOURCE_DIR}/imgui_demo.cpp

        # Backends
        ${imgui_src_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_src_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    )

    # Provide a stable, namespaced target for consumers
    add_library(ImGui::imgui ALIAS imgui)

    target_include_directories(imgui PUBLIC
        ${imgui_src_SOURCE_DIR}
        ${imgui_src_SOURCE_DIR}/backends
    )

    target_link_libraries(imgui PUBLIC
        PkgConfig::GLFW
        Vulkan::Vulkan
    )

elseif(TARGET imgui AND NOT TARGET ImGui::imgui)
    # If someone else defines plain 'imgui', give it the namespaced alias
    add_library(ImGui::imgui ALIAS imgui)
endif()


if(NOT TARGET ImPlot::implot AND NOT TARGET implot)

    # --- ImPlot ---
    FetchContent_Declare(
        implot_src
        GIT_REPOSITORY https://github.com/epezent/implot.git
        GIT_TAG v0.17
    )
    FetchContent_MakeAvailable(implot_src)

    add_library(implot STATIC
        ${implot_src_SOURCE_DIR}/implot.cpp
        ${implot_src_SOURCE_DIR}/implot_items.cpp
        ${implot_src_SOURCE_DIR}/implot_demo.cpp
    )

    # Provide a stable, namespaced target for consumers
    add_library(ImPlot::implot ALIAS implot)

    target_include_directories(implot PUBLIC ${implot_src_SOURCE_DIR})
    target_link_libraries(implot PUBLIC ImGui::imgui)

elseif(TARGET implot AND NOT TARGET ImPlot::implot)
    add_library(ImPlot::implot ALIAS implot)
endif()

target_link_libraries(${TARGET_NAME} PUBLIC ImPlot::implot)

target_precompile_headers(${TARGET_NAME} PRIVATE
    <vector>
    <string>
    <format>
    <imgui.h>
    <implot.h>
    <imgui_impl_glfw.h>
    <imgui_impl_vulkan.h>
)

# ===== Usage hint =====
#
# cmake --preset ram-debug  -DENABLE_ARRAYFIRE=OFF
# cmake --preset ram-release -DENABLE_DLIB=ON
#
# export BUILD_FOLDER=/mnt/ram/Week01/build  # to build in RAM FS
#
# For Debug
# cmake -S . -B "$BUILD_FOLDER/Debug" -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
# cmake --build "$BUILD_FOLDER/Debug"
#
# For Release
# cmake -S . -B "${BUILD_FOLDER}/Release" -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
# cmake --build "${BUILD_FOLDER}/Release"
#
# Or use Presets (defined in CMakePresets.json):
#
# Debug
# cmake --preset ram-debug
# cmake --build --preset build-ram-debug
#
# Release
# cmake --preset ram-release
# cmake --build --preset build-ram-release
