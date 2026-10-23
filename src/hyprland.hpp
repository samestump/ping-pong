#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <iostream>
#ifdef __linux__
#include <unistd.h>
#endif

// Target only this process's game window; never change compositor configuration.
inline bool restoreHyprlandSize(SDL_Window* window, int width, int height) {
#ifdef __linux__
    if (!SDL_getenv("HYPRLAND_INSTANCE_SIGNATURE") ||
        std::string(SDL_GetCurrentVideoDriver()) != "wayland") return false;
    std::string selector = "pid:" + std::to_string(getpid());
    std::string batch = "dispatch hl.dsp.window.float({action='enable',window='" + selector +
        "'}); dispatch hl.dsp.window.resize({x=" + std::to_string(width) + ",y=" +
        std::to_string(height) + ",relative=false,window='" + selector + "'})";
    const char* args[] = {"hyprctl", "--batch", batch.c_str(), nullptr};
    SDL_Process* process = SDL_CreateProcess(args, true);
    if (!process) return false;
    int exitCode = -1;
    Uint64 deadline = SDL_GetTicks() + 1500;
    while (!SDL_WaitProcess(process, false, &exitCode)) {
        if (SDL_GetTicks() >= deadline) {
            SDL_KillProcess(process, true);
            SDL_WaitProcess(process, true, &exitCode);
            SDL_DestroyProcess(process);
            return false;
        }
        SDL_PumpEvents();
        SDL_Delay(5);
    }
    size_t length = 0;
    void* bytes = SDL_ReadProcess(process, &length, &exitCode);
    std::string response = bytes ? std::string(static_cast<char*>(bytes), length) : "";
    SDL_free(bytes);
    SDL_DestroyProcess(process);
    if (exitCode != 0) return false;
    deadline = SDL_GetTicks() + 500;
    do {
        SDL_PumpEvents();
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        if (w == width && h == height) return true;
        SDL_Delay(5);
    } while (SDL_GetTicks() < deadline);
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    std::cerr << "Hyprland size restore: requested " << width << 'x' << height
              << ", received " << w << 'x' << h << "; " << response << '\n';
#else
    (void)window; (void)width; (void)height;
#endif
    return false;
}
