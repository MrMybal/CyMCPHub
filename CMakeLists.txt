# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only

cmake_minimum_required(VERSION 3.24)
project(CyMCPHub VERSION 0.3.0 LANGUAGES CXX)

if(NOT WIN32 AND NOT APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "CyMCPHub targets Windows, Linux and macOS.")
endif()

include(FetchContent)
find_package(Git REQUIRED)
if(APPLE)
    include(CheckCXXSourceCompiles)
    set(thread_check "#include <thread>\nint main() { std::jthread worker([](std::stop_token) {}); }")
    set(CMAKE_REQUIRED_FLAGS "-std=c++20")
    check_cxx_source_compiles("${thread_check}" CYMCP_HAS_JTHREAD)
    if(NOT CYMCP_HAS_JTHREAD)
        string(APPEND CMAKE_REQUIRED_FLAGS " -fexperimental-library")
        check_cxx_source_compiles("${thread_check}" CYMCP_HAS_EXPERIMENTAL_JTHREAD)
        if(NOT CYMCP_HAS_EXPERIMENTAL_JTHREAD)
            message(FATAL_ERROR "Use a recent Xcode toolchain with std::jthread support.")
        endif()
        add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-fexperimental-library>")
        add_link_options("$<$<LINK_LANGUAGE:CXX>:-fexperimental-library>")
    endif()
    unset(CMAKE_REQUIRED_FLAGS)
endif()
set(CYUI_MODULES DocumentViews)
set(CYUI_BUILD_DEMO OFF CACHE BOOL "")
set(CYUI_BUILD_AUDIO OFF CACHE BOOL "")
set(CYUI_BUILD_GALLERY OFF CACHE BOOL "")
set(CYMCP_BUILD_AI ON)
set(CYMCP_BUILD_CHAT_UI ON)
set(CYMCP_BUILD_LOCAL OFF CACHE BOOL "")
set(CYMCP_BUILD_TESTS OFF CACHE BOOL "")
set(CYMCP_BUILD_EXAMPLES OFF CACHE BOOL "")

FetchContent_Declare(CyUI GIT_REPOSITORY https://github.com/MrMybal/CyUI.git
    GIT_TAG 6ef01cfcdb197f3a89d64618ce5ef8818c704728)
FetchContent_Declare(CyMCPSDK GIT_REPOSITORY https://github.com/MrMybal/CyMCPSDK.git
    GIT_TAG 34d4bcae126bc83d1a2925b5047dac36e3d09d92
    PATCH_COMMAND ${CMAKE_COMMAND} "-DSDK_SOURCE=<SOURCE_DIR>" "-DGIT=${GIT_EXECUTABLE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patch-sdk.cmake")
FetchContent_Declare(tomlplusplus GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG 30172438cee64926dc41fdd9c11fb3ba5b2ba9de)
FetchContent_MakeAvailable(CyUI CyMCPSDK tomlplusplus)

add_library(hub_catalog STATIC src/catalog.cpp src/sdk_scan.cpp src/mcp_actions.cpp src/journal.cpp
    src/hub_service.cpp src/resident_service.cpp src/skills.cpp src/platform_common.cpp)
target_compile_definitions(hub_catalog PRIVATE CYMCP_HUB_VERSION="${PROJECT_VERSION}")
target_link_libraries(hub_catalog PUBLIC CyMCP::MCP PRIVATE tomlplusplus::tomlplusplus)
if(WIN32)
    target_sources(hub_catalog PRIVATE src/windows_inventory.cpp)
    target_link_libraries(hub_catalog PRIVATE iphlpapi ws2_32 wbemuuid ole32 oleaut32 shell32 advapi32 crypt32)
else()
    target_sources(hub_catalog PRIVATE src/posix_inventory.cpp)
endif()

add_executable(CyMCPHubMcp src/mcp_main.cpp)
target_link_libraries(CyMCPHubMcp PRIVATE hub_catalog)

set(imgui_dir "${cyui_SOURCE_DIR}/third_party/imgui")
set(branding_dir "${CMAKE_CURRENT_SOURCE_DIR}/assets/branding")
add_executable(CyMCPHub WIN32 MACOSX_BUNDLE src/ui.cpp src/action_panel.cpp src/assistant_panel.cpp src/skill_panel.cpp)
target_link_libraries(CyMCPHub PRIVATE hub_catalog CyUI::Core CyMCP::ChatUI)
if(WIN32)
    enable_language(RC)
    target_sources(CyMCPHub PRIVATE src/ui_main.cpp src/app_icon.rc
        "${imgui_dir}/backends/imgui_impl_win32.cpp" "${imgui_dir}/backends/imgui_impl_dx11.cpp")
    target_include_directories(CyMCPHub PRIVATE "${imgui_dir}/backends" "${branding_dir}")
    set_source_files_properties(src/app_icon.rc PROPERTIES OBJECT_DEPENDS "${branding_dir}/cymcphub.ico")
    target_link_libraries(CyMCPHub PRIVATE d3d11 dxgi d3dcompiler dwmapi shell32)
else()
    set(SDL_SHARED OFF)
    set(SDL_STATIC ON)
    set(SDL_TESTS OFF)
    set(SDL_TEST_LIBRARY OFF)
    set(SDL_EXAMPLES OFF)
    FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG 683181b47cfabd293e3ea409f838915b8297a4fd)
    FetchContent_MakeAvailable(SDL3)
    set(sdl_backends "${cyui_SOURCE_DIR}/third_party/imgui-sdl3")
    target_sources(CyMCPHub PRIVATE src/ui_sdl.cpp src/autostart_posix.cpp
        "${sdl_backends}/imgui_impl_sdl3.cpp" "${sdl_backends}/imgui_impl_sdlrenderer3.cpp")
    target_include_directories(CyMCPHub PRIVATE "${sdl_backends}")
    target_link_libraries(CyMCPHub PRIVATE SDL3::SDL3-static)
    set(app_icon_png "${CMAKE_CURRENT_SOURCE_DIR}/.CyDocBuild/assets/cymcphub-logo.png")
    if(APPLE)
        set(app_icon_icns "${branding_dir}/cymcphub.icns")
        set_source_files_properties("${app_icon_png}" "${app_icon_icns}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
        target_sources(CyMCPHub PRIVATE "${app_icon_png}" "${app_icon_icns}")
        set_target_properties(CyMCPHub PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER com.cyberalien.cymcphub
            MACOSX_BUNDLE_BUNDLE_NAME CyMCPHub MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}" MACOSX_BUNDLE_ICON_FILE cymcphub.icns)
        add_dependencies(CyMCPHub CyMCPHubMcp)
        add_custom_command(TARGET CyMCPHub POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:CyMCPHubMcp>" "$<TARGET_FILE_DIR:CyMCPHub>/CyMCPHubMcp")
    else()
        set_property(TARGET CyMCPHub APPEND PROPERTY LINK_DEPENDS "${app_icon_png}")
        add_custom_command(TARGET CyMCPHub POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${app_icon_png}" "$<TARGET_FILE_DIR:CyMCPHub>/cymcphub-logo.png")
        install(FILES "${app_icon_png}" DESTINATION . COMPONENT Hub)
    endif()
endif()

foreach(target hub_catalog CyMCPHub CyMCPHubMcp)
    target_compile_features(${target} PRIVATE cxx_std_20)
    if(WIN32)
        target_compile_definitions(${target} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/utf-8;/W4;/permissive->")
    endif()
endforeach()

add_custom_command(TARGET CyMCPHub POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:CyMCPHub>/licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${cyui_SOURCE_DIR}/LICENSE"
        "$<TARGET_FILE_DIR:CyMCPHub>/licenses/CyUI.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${imgui_dir}/LICENSE.txt"
        "$<TARGET_FILE_DIR:CyMCPHub>/licenses/Dear-ImGui.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${cyui_SOURCE_DIR}/third_party/jetbrains-mono/OFL.txt"
        "$<TARGET_FILE_DIR:CyMCPHub>/licenses/JetBrains-Mono.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${cyui_SOURCE_DIR}/third_party/md4c/LICENSE.md"
        "$<TARGET_FILE_DIR:CyMCPHub>/licenses/MD4C.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${cyui_SOURCE_DIR}/third_party/lexilla/License.txt"
        "$<TARGET_FILE_DIR:CyMCPHub>/licenses/Lexilla.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${cyui_SOURCE_DIR}/third_party/scintilla/License.txt"
        "$<TARGET_FILE_DIR:CyMCPHub>/licenses/Scintilla.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${cymcpsdk_SOURCE_DIR}/LICENSE"
        "$<TARGET_FILE_DIR:CyMCPHub>/licenses/CyMCPSDK.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${cymcpsdk_SOURCE_DIR}/third_party/nlohmann/LICENSE.MIT"
        "$<TARGET_FILE_DIR:CyMCPHub>/licenses/nlohmann-json.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${cymcpsdk_SOURCE_DIR}/third_party/httplib/LICENSE"
        "$<TARGET_FILE_DIR:CyMCPHub>/licenses/cpp-httplib.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${cymcpsdk_SOURCE_DIR}/third_party/picosha2/LICENSE"
        "$<TARGET_FILE_DIR:CyMCPHub>/licenses/PicoSHA2.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${tomlplusplus_SOURCE_DIR}/LICENSE"
        "$<TARGET_FILE_DIR:CyMCPHub>/licenses/tomlplusplus.txt")

if(NOT WIN32)
    add_custom_command(TARGET CyMCPHub POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${sdl3_SOURCE_DIR}/LICENSE.txt" "$<TARGET_FILE_DIR:CyMCPHub>/licenses/SDL.txt")
endif()

install(TARGETS CyMCPHub RUNTIME DESTINATION . COMPONENT Hub BUNDLE DESTINATION . COMPONENT Hub)
if(APPLE)
    install(TARGETS CyMCPHubMcp RUNTIME DESTINATION CyMCPHub.app/Contents/MacOS COMPONENT Hub)
else()
    install(TARGETS CyMCPHubMcp RUNTIME DESTINATION . COMPONENT Hub)
    install(DIRECTORY "$<TARGET_FILE_DIR:CyMCPHub>/licenses" DESTINATION . COMPONENT Hub)
endif()
install(FILES LICENSE README.md README.fr.md BUILD.md VERSION.md VERSION.fr.md DESTINATION . COMPONENT Hub)
install(DIRECTORY Documentation DESTINATION . COMPONENT Hub)

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME http_compatibility COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/http_compatibility.py"
            "$<TARGET_FILE:CyMCPHubMcp>")
        set_tests_properties(http_compatibility PROPERTIES TIMEOUT 75)
    endif()
endif()
