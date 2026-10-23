cmake_minimum_required(VERSION 3.20)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
endif()
project(PingPong VERSION 1.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(PING_PONG_BUNDLED_DEPS "Build pinned static SDL3 and FreeType" ${APPLE})
if(PING_PONG_BUNDLED_DEPS)
  include(cmake/Dependencies.cmake)
else()
  find_package(SDL3 3.2 REQUIRED CONFIG)
  find_package(Freetype REQUIRED)
endif()
add_executable(ping-pong src/main.cpp)
target_link_libraries(ping-pong PRIVATE SDL3::SDL3 Freetype::Freetype)
target_compile_options(ping-pong PRIVATE -Wall -Wextra -Wpedantic)
if(APPLE)
  set_target_properties(ping-pong PROPERTIES
    MACOSX_BUNDLE TRUE
    OUTPUT_NAME "Ping Pong"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.samestump.ping-pong"
    MACOSX_BUNDLE_BUNDLE_NAME "Ping Pong"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
  set(game_assets assets/Silkscreen-Regular.ttf assets/OFL.txt)
  set_source_files_properties(${game_assets} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/assets")
  target_sources(ping-pong PRIVATE ${game_assets})
  set_source_files_properties(LICENSE THIRD_PARTY.md PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/licenses")
  target_sources(ping-pong PRIVATE LICENSE THIRD_PARTY.md)
  if(PING_PONG_BUNDLED_DEPS)
    foreach(license IN ITEMS "${sdl3_SOURCE_DIR}/LICENSE.txt"
        "${freetype_SOURCE_DIR}/LICENSE.TXT" "${freetype_SOURCE_DIR}/docs/FTL.TXT")
      set_source_files_properties("${license}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/licenses")
      target_sources(ping-pong PRIVATE "${license}")
    endforeach()
    # LICENSE.txt and LICENSE.TXT collide on standard macOS filesystems.
    set_source_files_properties("${sdl3_SOURCE_DIR}/LICENSE.txt" PROPERTIES
      MACOSX_PACKAGE_LOCATION "Resources/licenses/SDL3")
    set_source_files_properties("${freetype_SOURCE_DIR}/LICENSE.TXT" "${freetype_SOURCE_DIR}/docs/FTL.TXT"
      PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/licenses/FreeType")
    add_custom_target(macos-package
      COMMAND /bin/bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/package-macos.sh"
        "$<TARGET_BUNDLE_DIR:ping-pong>" "${CMAKE_CURRENT_BINARY_DIR}/dist"
      DEPENDS ping-pong VERBATIM)
  endif()
else()
  add_custom_command(TARGET ping-pong POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:ping-pong>/assets"
    VERBATIM)
endif()
include(CTest)
if(BUILD_TESTING)
  add_executable(game-tests tests/game_tests.cpp)
  target_include_directories(game-tests PRIVATE src)
  add_test(NAME physics COMMAND game-tests)
  add_test(NAME window-smoke COMMAND ping-pong --smoke-test)
  set_tests_properties(window-smoke PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software")
  add_test(NAME window-persistence COMMAND ${CMAKE_COMMAND}
    -DGAME=$<TARGET_FILE:ping-pong> -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/window_persistence.cmake)
  set_tests_properties(window-persistence PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software")
endif()
