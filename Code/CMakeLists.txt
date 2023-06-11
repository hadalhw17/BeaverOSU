cmake_minimum_required(VERSION 3.20.4)
set(PROJECT_NAME RavenOSU)
project(${PROJECT_NAME} VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(../../RavenRenderer ../../RavenRenderer/solutions)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${RAVEN_OUTPUT_LIB})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${RAVEN_OUTPUT_LIB})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${RAVEN_OUTPUT_BIN})

file(GLOB OSU
    RavenOSU.hpp
    Rendering.cpp
)
source_group(OSU FILES ${OSU})

file(GLOB UI
    UI/UICommon.hpp
    UI/MainMenu.cpp
    UI/PlayHUD.cpp
    UI/SplashScreen.cpp
)
source_group(UI FILES ${UI})

add_executable(GameOSU main.cpp ${OSU} ${UI})
target_link_libraries(GameOSU PRIVATE RavenEngine RavenVFX RavenUI RavenApp raven_warnings raven_compiler_settings RavenAudio)

target_include_directories(GameOSU 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

get_filename_component(PROJECT_PATH
                       "${CMAKE_CURRENT_SOURCE_DIR}/.."
                       ABSOLUTE)

set_target_properties(GameOSU PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "--engine  ${RAVEN_SOURCE_DIR} --project ${PROJECT_PATH}")

set_target_properties(GameOSU PROPERTIES
    FOLDER Launchers)

