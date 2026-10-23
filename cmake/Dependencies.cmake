include(FetchContent)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

# Static libraries make the macOS bundle independent of the builder's Homebrew.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
foreach(dependency ZLIB BZIP2 PNG HARFBUZZ BROTLI)
  # Silkscreen is an uncompressed TTF and needs none of these optional libraries.
  set(FT_DISABLE_${dependency} ON CACHE BOOL "" FORCE)
endforeach()
FetchContent_Declare(sdl3
  URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.0/SDL3-3.4.0.tar.gz
  URL_HASH SHA256=082cbf5f429e0d80820f68dc2b507a94d4cc1b4e70817b119bbb8ec6a69584b8)
FetchContent_Declare(freetype
  URL https://github.com/freetype/freetype/archive/refs/tags/VER-2-14-1.tar.gz
  URL_HASH SHA256=44bd69d1f0750603410cfd4f26f1c5523c5a3a087a2dfa8639b757dc7c6677be)
FetchContent_MakeAvailable(sdl3 freetype)
add_library(Freetype::Freetype ALIAS freetype)
