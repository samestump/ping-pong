#pragma once
#include <istream>
#include <ostream>
#include <fstream>
#include <cstdio>
#include <string>

struct WindowSize {
    int width = 960, height = 600;
    int difficulty = 2;
    bool update(int w, int h) {
        if (w < 320 || h < 240 || w > 16384 || h > 16384) return false;
        if (width == w && height == h) return false;
        width = w; height = h;
        return true;
    }
    void read(std::istream& input) {
        int w, h;
        if (input >> w >> h && w >= 320 && h >= 240 && w <= 16384 && h <= 16384) {
            int version = 1;
            input >> version;
            // Repair the minimum-size value left by the old close-time saver.
            // Version 2 can still intentionally remember a 320x240 window.
            if (version == 1 && w == 320 && h == 240) return;
            width = w; height = h;
            int level;
            if (version >= 3 && input >> level && level >= 1 && level <= 3)
                difficulty = level;
        }
    }
    void write(std::ostream& output) const {
        output << width << ' ' << height << " 3 " << difficulty << '\n';
    }
    bool save(const std::string& path) const {
        std::string temporary = path + ".tmp";
        {
            std::ofstream output(temporary);
            write(output);
            output.close();
            if (!output) return false;
        }
        return std::rename(temporary.c_str(), path.c_str()) == 0;
    }
};
