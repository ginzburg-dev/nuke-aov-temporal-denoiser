cmake_minimum_required(VERSION 3.20)

project(
    NukeAovTemporalDenoiser
    DESCRIPTION "AOV-guided spatiotemporal denoising research for Foundry Nuke"
    LANGUAGES CXX
)

option(NTD_BUILD_TESTS "Build the dependency-free core test suite" ON)
option(NTD_BUILD_EXAMPLES "Build the synthetic regression example" ON)
option(NTD_BUILD_NUKE_PLUGIN "Build the Foundry Nuke adapter" OFF)
option(NTD_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

function(ntd_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(NTD_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(
            ${target}
            PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow
        )
        if(NTD_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(
    ntd_core
    src/GuidedWeight.cpp
    src/TemporalMatcher.cpp
)
add_library(ntd::core ALIAS ntd_core)

target_compile_features(ntd_core PUBLIC cxx_std_17)
set_target_properties(ntd_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
ntd_enable_warnings(ntd_core)
target_include_directories(ntd_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(NTD_BUILD_TESTS)
    include(CTest)
    add_executable(ntd_core_tests tests/CoreTests.cpp)
    target_link_libraries(ntd_core_tests PRIVATE ntd::core)
    ntd_enable_warnings(ntd_core_tests)
    add_test(NAME ntd.core COMMAND ntd_core_tests)
    add_test(
        NAME ntd.indentation
        COMMAND
            "${CMAKE_COMMAND}"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckIndentation.cmake"
    )
endif()

if(NTD_BUILD_EXAMPLES)
    add_executable(ntd_synthetic_demo examples/SyntheticDemo.cpp)
    target_link_libraries(ntd_synthetic_demo PRIVATE ntd::core)
    ntd_enable_warnings(ntd_synthetic_demo)
    if(NTD_BUILD_TESTS)
        add_test(
            NAME ntd.synthetic
            COMMAND
                ntd_synthetic_demo
                "${CMAKE_CURRENT_BINARY_DIR}/synthetic-demo.svg"
        )
        add_test(
            NAME ntd.synthetic_asset
            COMMAND
                "${CMAKE_COMMAND}" -E compare_files
                "${CMAKE_CURRENT_BINARY_DIR}/synthetic-demo.svg"
                "${CMAKE_CURRENT_SOURCE_DIR}/docs/synthetic-demo.svg"
        )
        set_tests_properties(ntd.synthetic_asset PROPERTIES DEPENDS ntd.synthetic)
    endif()
endif()

if(NTD_BUILD_NUKE_PLUGIN)
    if(NOT DEFINED NUKE_SDK_ROOT)
        message(
            FATAL_ERROR
            "Set NUKE_SDK_ROOT to a Nuke installation containing include/ and lib/"
        )
    endif()

    find_path(
        NUKE_INCLUDE_DIR
        DDImage/Iop.h
        HINTS "${NUKE_SDK_ROOT}/include"
        REQUIRED
    )
    find_library(
        NUKE_DDIMAGE_LIBRARY
        DDImage
        HINTS "${NUKE_SDK_ROOT}" "${NUKE_SDK_ROOT}/lib"
        REQUIRED
    )

    add_library(GinzburgTemporalDenoiser MODULE src/nuke/NukeTemporalDenoiser.cpp)
    target_compile_features(GinzburgTemporalDenoiser PRIVATE cxx_std_17)
    target_include_directories(
        GinzburgTemporalDenoiser PRIVATE "${NUKE_INCLUDE_DIR}"
    )
    target_link_libraries(
        GinzburgTemporalDenoiser PRIVATE ntd::core "${NUKE_DDIMAGE_LIBRARY}"
    )
    ntd_enable_warnings(GinzburgTemporalDenoiser)
    set_target_properties(GinzburgTemporalDenoiser PROPERTIES PREFIX "")
    if(APPLE)
        set_target_properties(GinzburgTemporalDenoiser PROPERTIES SUFFIX ".dylib")
    endif()
endif()
