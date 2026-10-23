#include "game.hpp"
#include "settings.hpp"
#include "hyprland.hpp"
#include <fstream>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

struct Text {
    SDL_Renderer* renderer;
    FT_Library library{};
    FT_Face face{};
    struct Entry { SDL_Texture* texture; float w, h; };
    std::map<std::string, Entry> cache;
    explicit Text(SDL_Renderer* r) : renderer(r) {
        const char* base = SDL_GetBasePath();
        if (!base) throw std::runtime_error(SDL_GetError());
        std::string path = std::string(base) + "assets/Silkscreen-Regular.ttf";
        if (FT_Init_FreeType(&library) || FT_New_Face(library, path.c_str(), 0, &face))
            throw std::runtime_error("Cannot load font: " + path);
    }
    ~Text() {
        for (auto& item : cache) SDL_DestroyTexture(item.second.texture);
        FT_Done_Face(face); FT_Done_FreeType(library);
    }
    void draw(const std::string& value, float cx, float top, int size, float scale, Uint8 shade = 240) {
        int px = std::max(1, static_cast<int>(std::round(size * scale)));
        std::string key = value + ":" + std::to_string(px);
        if (!cache.count(key)) {
            // Keep score textures bounded even during very long sessions.
            if (cache.size() > 64) {
                for (auto& item : cache) SDL_DestroyTexture(item.second.texture);
                cache.clear();
            }
            FT_Set_Pixel_Sizes(face, 0, px);
            int w = 0;
            for (unsigned char c : value) {
                FT_Load_Char(face, c, FT_LOAD_DEFAULT);
                w += static_cast<int>(face->glyph->advance.x >> 6);
            }
            int h = px * 2;
            SDL_Surface* surface = SDL_CreateSurface(std::max(w, 1), h, SDL_PIXELFORMAT_RGBA32);
            if (!surface) throw std::runtime_error(SDL_GetError());
            SDL_ClearSurface(surface, 0, 0, 0, 0);
            int pen = 0;
            int baseline = static_cast<int>(face->size->metrics.ascender >> 6);
            for (unsigned char c : value) {
                FT_Load_Char(face, c, FT_LOAD_RENDER | FT_LOAD_TARGET_MONO);
                auto g = face->glyph;
                for (unsigned row = 0; row < g->bitmap.rows; ++row)
                    for (unsigned col = 0; col < g->bitmap.width; ++col) {
                        int x = pen + g->bitmap_left + static_cast<int>(col);
                        int y = baseline - g->bitmap_top + static_cast<int>(row);
                        if (x < 0 || x >= w || y < 0 || y >= h) continue;
                        if (g->bitmap.buffer[row * g->bitmap.pitch + col / 8] & (0x80 >> (col % 8))) {
                            auto p = static_cast<Uint8*>(surface->pixels) + y * surface->pitch + x * 4;
                            p[0] = p[1] = p[2] = p[3] = 255;
                        }
                    }
                pen += static_cast<int>(g->advance.x >> 6);
            }
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_DestroySurface(surface);
            if (!texture) throw std::runtime_error(SDL_GetError());
            SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
            cache.emplace(key, Entry{texture, static_cast<float>(w), static_cast<float>(h)});
        }
        auto& e = cache.at(key);
        SDL_SetTextureColorMod(e.texture, shade, shade, shade);
        SDL_FRect rect{cx - e.w / scale / 2, top, e.w / scale, e.h / scale};
        SDL_RenderTexture(renderer, e.texture, nullptr, &rect);
    }
};

static void circle(SDL_Renderer* r, float x, float y, float radius) {
    constexpr int segments = 48;
    SDL_Vertex vertices[segments + 2];
    int indices[segments * 3];
    vertices[0] = {{x, y}, { .94f, .94f, .94f, 1 }, {0, 0}};
    for (int i = 0; i <= segments; ++i) {
        float a = i * 6.2831853f / segments;
        vertices[i + 1] = {{x + radius * std::cos(a), y + radius * std::sin(a)}, {.94f, .94f, .94f, 1}, {0, 0}};
        if (i < segments) { indices[i*3] = 0; indices[i*3+1] = i+1; indices[i*3+2] = i+2; }
    }
    SDL_RenderGeometry(r, nullptr, vertices, segments+2, indices, segments*3);
}

int main(int argc, char** argv) {
    bool windowTest = argc == 5 && std::string(argv[1]) == "--window-state-test";
    bool smoke = windowTest || (argc > 1 && std::string(argv[1]) == "--smoke-test");
    bool screenshotPause = argc > 2 && std::string(argv[1]) == "--screenshot-pause";
    bool screenshot = screenshotPause || (argc > 2 && std::string(argv[1]) == "--screenshot");
    if (argc > 1 && !smoke && !screenshot) {
        std::cout << "Ping Pong: wheel / touchpad = move, 1-3 = difficulty, Esc = pause, A = auto, R = restart, Q = quit\n";
        return 0;
    }
    // Prefer native Wayland when it is available; keep SDL's X11 fallback.
#ifdef __linux__
    if (!SDL_getenv("SDL_VIDEO_DRIVER") && SDL_getenv("WAYLAND_DISPLAY"))
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO)) { std::cerr << SDL_GetError() << '\n'; return 1; }
    WindowSize savedSize;
    std::string settingsPath;
    if (windowTest) {
        settingsPath = argv[2];
        std::ifstream input(settingsPath);
        savedSize.read(input);
    }
    if (!smoke && !screenshot) {
        if (char* folder = SDL_GetPrefPath("ping-pong", "ping-pong")) {
            settingsPath = std::string(folder) + "window-size.txt";
            SDL_free(folder);
            std::ifstream input(settingsPath);
            savedSize.read(input);
        }
    }
    SDL_Window* window = SDL_CreateWindow("Ping Pong", savedSize.width, savedSize.height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
    if (window) {
        SDL_SetWindowMinimumSize(window, 320, 240);
        SDL_SetWindowSize(window, savedSize.width, savedSize.height);
        if (!screenshot) {
            SDL_ShowWindow(window);
            SDL_SyncWindow(window);
        }
    }
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) { std::cerr << SDL_GetError() << '\n'; SDL_DestroyWindow(window); SDL_Quit(); return 1; }
    // Commit the first buffer before requesting the remembered floating geometry.
    // Wayland can configure a different initial size while mapping the surface.
    if (!screenshot) {
        SDL_SetRenderDrawColor(renderer, 29, 29, 29, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
        SDL_SetWindowSize(window, savedSize.width, savedSize.height);
        SDL_SyncWindow(window);
        if (!smoke || windowTest) restoreHyprlandSize(window, savedSize.width, savedSize.height);
    }
    SDL_SetRenderVSync(renderer, 1);
    int result = 0;
    try {
        if (windowTest) {
            int w = 0, h = 0;
            SDL_GetWindowSize(window, &w, &h);
            if (w != std::stoi(argv[3]) || h != std::stoi(argv[4]))
                throw std::runtime_error("Saved window dimensions were not restored");
        }
        Text text(renderer);
        Game game;
        game.reset();
        bool running = true, focused = true, minimized = false, manualPause = screenshotPause;
        if (screenshotPause) game.toggleAuto();
        if (SDL_GetMouseFocus() == window) SDL_HideCursor();
        double resizeUntil = 0, last = SDL_GetTicksNS() / 1e9, accumulator = 0;
        bool sizeDirty = false;
        auto saveSize = [&] {
            if (sizeDirty && !settingsPath.empty()) {
                if (!savedSize.save(settingsPath)) std::cerr << "Could not save window size: " << settingsPath << '\n';
                else sizeDirty = false;
            }
        };
        float scale = 1;
        auto resize = [&] {
            int w, h;
            SDL_GetRenderOutputSize(renderer, &w, &h);
            scale = SDL_GetWindowDisplayScale(window);
            if (scale <= 0) scale = 1;
            SDL_SetRenderScale(renderer, scale, scale);
            game.resize(w / scale, h / scale);
        };
        resize();
        int frame = 0;
        float targetBeforeScroll = 0;
        bool scrollPassed = false;
        float pausedX = 0, pausedY = 0;
        while (running) {
            double now = SDL_GetTicksNS() / 1e9;
            double elapsed = std::min(now - last, .05);
            last = now;
            if (smoke) {
                SDL_Event e{};
                if (frame == 2 || frame == 4) {
                    e.type = SDL_EVENT_KEY_DOWN;
                    e.key.scancode = frame == 2 ? SDL_SCANCODE_3 : SDL_SCANCODE_R;
                    SDL_PushEvent(&e);
                }
                if (frame == 6) { targetBeforeScroll = game.target; e.type = SDL_EVENT_MOUSE_WHEEL; e.wheel.y = 1.25f; SDL_PushEvent(&e); }
                if (frame == 7) scrollPassed = game.target < targetBeforeScroll;
                if (frame == 10 || frame == 16 || frame == 11 || frame == 18) {
                    e.type = SDL_EVENT_KEY_DOWN;
                    e.key.scancode = (frame == 10 || frame == 16) ? SDL_SCANCODE_ESCAPE : SDL_SCANCODE_A;
                    SDL_PushEvent(&e);
                }
                if (frame == 8) {
                    SDL_SetWindowSize(window, 1100, 700);
                    if (windowTest) restoreHyprlandSize(window, 1100, 700);
                }
                if (frame == 30) { e.type = SDL_EVENT_KEY_DOWN; e.key.scancode = SDL_SCANCODE_Q; SDL_PushEvent(&e); }
            }
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (!running) break;
                switch (event.type) {
                case SDL_EVENT_QUIT: running = false; break;
                case SDL_EVENT_KEY_DOWN:
                    if (event.key.repeat) break;
                    if (event.key.scancode == SDL_SCANCODE_Q) running = false;
                    if (event.key.scancode == SDL_SCANCODE_R) game.reset();
                    if (event.key.scancode == SDL_SCANCODE_ESCAPE) { manualPause = !manualPause; accumulator = 0; elapsed = 0; }
                    if (event.key.scancode == SDL_SCANCODE_A) game.toggleAuto();
                    if (event.key.scancode >= SDL_SCANCODE_1 && event.key.scancode <= SDL_SCANCODE_3)
                        game.setDifficulty(event.key.scancode - SDL_SCANCODE_1 + 1);
                    break;
                case SDL_EVENT_MOUSE_WHEEL:
                    // SDL already applies the desktop's natural-scrolling setting.
                    if (!manualPause && focused && !minimized) game.scroll(event.wheel.y);
                    break;
                case SDL_EVENT_WINDOW_RESIZED:
                    // Cache compositor-confirmed logical dimensions while the window is alive.
                    // Never query its geometry during close/teardown: it may already be unmapped.
                    if (!(SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED))
                        sizeDirty = savedSize.update(event.window.data1, event.window.data2) || sizeDirty;
                    resize(); resizeUntil = now + .18; break;
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
                    resize(); resizeUntil = now + .18; break;
                case SDL_EVENT_WINDOW_MOUSE_ENTER: SDL_HideCursor(); break;
                case SDL_EVENT_WINDOW_MOUSE_LEAVE: SDL_ShowCursor(); break;
                case SDL_EVENT_WINDOW_FOCUS_LOST: focused = false; SDL_ShowCursor(); break;
                case SDL_EVENT_WINDOW_FOCUS_GAINED: focused = true; if (SDL_GetMouseFocus() == window) SDL_HideCursor(); break;
                case SDL_EVENT_WINDOW_MINIMIZED: minimized = true; break;
                case SDL_EVENT_WINDOW_RESTORED: minimized = false; break;
                default: break;
                }
            }
            bool paused = manualPause || now < resizeUntil || !focused || minimized;
            if (now >= resizeUntil) saveSize();
            if (paused) accumulator = 0;
            else {
                accumulator += elapsed;
                while (accumulator >= 1.0 / 240) { game.step(1.f / 240); accumulator -= 1.0 / 240; }
            }
            if (smoke && frame == 10) { pausedX = game.x; pausedY = game.y; }
            if (smoke && frame == 14 && (!manualPause || !game.automatic || game.x != pausedX || game.y != pausedY))
                throw std::runtime_error("Escape pause / auto test failed");
            SDL_SetRenderDrawColor(renderer, 29, 29, 29, 255);
            SDL_RenderClear(renderer);
            SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
            SDL_FRect left{Game::inset, game.player - Game::paddleHeight/2, Game::paddleWidth, Game::paddleHeight};
            SDL_FRect right{game.width - Game::inset - Game::paddleWidth, game.bot - Game::paddleHeight/2, Game::paddleWidth, Game::paddleHeight};
            SDL_RenderFillRect(renderer, &left); SDL_RenderFillRect(renderer, &right);
            circle(renderer, game.x, game.y, Game::radius);
            text.draw(std::to_string(game.leftScore) + " - " + std::to_string(game.rightScore), game.width/2, 18, 40, scale);
            const char* levels[] = {"1 EASY", "2 MEDIUM", "3 HARD"};
            text.draw(levels[game.difficulty-1], game.width/2, 70, 12, scale, 125);
            if (game.automatic) text.draw("AUTO", game.width/2, game.height - 28, 12, scale, 105);
            if (manualPause) {
                float top = std::max(92.f, game.height/2 - 55);
                SDL_SetRenderDrawColor(renderer, 29, 29, 29, 255);
                SDL_FRect menuBacking{game.width/2 - 130, top - 4, 260, 110};
                SDL_RenderFillRect(renderer, &menuBacking);
                text.draw("PAUSED", game.width/2, top, 16, scale, 145);
                const char* help[] = {"SCROLL  MOVE", "1-3  DIFFICULTY", "A  AUTO   R  RESTART", "Q  QUIT", "ESC  RESUME"};
                for (int i = 0; i < 5; ++i)
                    text.draw(help[i], game.width/2, top + 28 + i * 16, 10, scale, 115);
            } else if (paused && !screenshot) text.draw("PAUSED", game.width/2, game.height/2 + 30, 16, scale, 145);
            if (screenshot) {
                SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr);
                if (!shot || !SDL_SaveBMP(shot, argv[2])) throw std::runtime_error(SDL_GetError());
                SDL_DestroySurface(shot); running = false;
            }
            SDL_RenderPresent(renderer);
            ++frame;
            if (smoke && frame == 31) {
                int outputW = 0, outputH = 0;
                SDL_GetRenderOutputSize(renderer, &outputW, &outputH);
                // Tiling compositors can override the requested window dimensions.
                if (game.difficulty != 3 || game.leftScore || game.rightScore || (!windowTest && !scrollPassed) || manualPause || game.automatic
                    || std::abs(game.width - outputW / scale) > 1 || std::abs(game.height - outputH / scale) > 1)
                    throw std::runtime_error("Input / resize smoke test failed");
                std::cout << "SDL input, resize, restart and quit: OK (" << SDL_GetCurrentVideoDriver() << ", "
                          << outputW << 'x' << outputH << ", scale " << scale << ")\n";
            }
            SDL_Delay(4);
        }
        saveSize();
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; result = 1; }
    SDL_ShowCursor();
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    return result;
}
