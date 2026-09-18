cmake_minimum_required(VERSION 3.24)
project(TAS VERSION 1.0.0 LANGUAGES CXX)

if(NOT MSVC)
    message(FATAL_ERROR "TAS requires MSVC")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BAKKESMOD_SDK "${CMAKE_SOURCE_DIR}/vendor/BakkesModSDK" CACHE PATH "BakkesMod SDK root")
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/vendor/imgui")
option(TAS_BUILD_TESTS "Build TAS core tests" ON)

add_library(TAS SHARED
    src/Plugin.cpp
    src/PluginActions.cpp
    src/TasSession.cpp
    src/JsonCodec.cpp
    src/JsonStore.cpp
    src/WorldState.cpp
    src/Gui.cpp
    src/GuiControls.cpp
    src/GuiFiles.cpp
    src/GuiSettings.cpp
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/imgui_stdlib.cpp
)

target_include_directories(TAS PRIVATE
    src
    vendor
    vendor/imgui
    "${BAKKESMOD_SDK}/include"
)

target_link_libraries(TAS PRIVATE
    "${BAKKESMOD_SDK}/lib/pluginsdk.lib"
    user32.lib
)
target_compile_definitions(TAS PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_compile_options(TAS PRIVATE /W4 /permissive- /EHsc)
set_property(TARGET TAS PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set_target_properties(TAS PROPERTIES
    OUTPUT_NAME "TAS"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist"
    PDB_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist"
)

foreach(configuration Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${configuration}" configuration_upper)
    set_target_properties(TAS PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${configuration_upper} "${CMAKE_SOURCE_DIR}/dist"
        PDB_OUTPUT_DIRECTORY_${configuration_upper} "${CMAKE_SOURCE_DIR}/dist"
    )
endforeach()

if(TAS_BUILD_TESTS)
    enable_testing()
    add_executable(TASCoreTests
        tests/TasSessionTests.cpp
        src/TasSession.cpp
    )
    target_include_directories(TASCoreTests PRIVATE
        src
        "${BAKKESMOD_SDK}/include"
    )
    target_compile_definitions(TASCoreTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(TASCoreTests PRIVATE /W4 /permissive- /EHsc)
    set_property(TARGET TASCoreTests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_test(NAME TasSessionHistory COMMAND TASCoreTests)
endif()
