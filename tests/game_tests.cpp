#include "game.hpp"
#include "settings.hpp"
#include <sstream>
#include <iostream>
#include <stdexcept>

void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main() {
    Game g;
    g.random.seed(42);
    g.serveTime = 0;
    g.x = 9; g.y = 100; g.vx = -370; g.vy = 0;
    g.step(1.f/240);
    check(g.rightScore == 1 && g.leftScore == 0, "Left edge must score for bot");
    g.serveTime = 0; g.x = g.width - 9; g.vx = 370;
    g.step(1.f/240);
    check(g.leftScore == 1, "Right edge must score for player");
    g.serveTime = 0; g.x = 59; g.y = g.player; g.vx = -780; g.vy = 0;
    g.step(1.f/240);
    check(g.vx > 0 && g.rightScore == 1, "Fast ball must bounce on paddle");
    g.y = 8.5f; g.vy = -500; g.x = 400;
    g.step(1.f/240);
    check(g.vy > 0 && g.y >= 8, "Top reflection");
    float x = g.x, vx = g.vx;
    g.setDifficulty(3);
    check(g.x == x && g.vx == vx && g.leftScore == 1, "Difficulty must preserve rally");
    g.reset();
    check(g.difficulty == 3 && !g.leftScore && !g.rightScore, "Reset preserves difficulty");
    g.scroll(.25f);
    check(g.target < g.player, "Fractional trackpad input");
    g.scroll(1000); check(g.target == 56, "Clamp scroll at top");
    g.serveTime = 0; g.x = 900; g.y = 550;
    g.resize(400, 300);
    check(g.x < 392 && g.y <= 292 && !g.leftScore, "Resize must not score");
    check(Game::paddleHeight == 112 && Game::radius == 8, "Constant geometry");
    WindowSize size;
    std::istringstream valid("1234 789"); size.read(valid);
    std::ostringstream stored; size.write(stored);
    WindowSize restored; std::istringstream saved(stored.str()); restored.read(saved);
    check(restored.width == 1234 && restored.height == 789, "Window size round trip");
    std::istringstream invalid("-1 999999"); restored.read(invalid);
    check(restored.width == 1234, "Ignore invalid settings");
    check(restored.difficulty == 2, "Legacy settings default to medium difficulty");
    for (int level = 1; level <= 3; ++level) {
        size.difficulty = level;
        std::ostringstream output; size.write(output);
        WindowSize loaded;
        std::istringstream input(output.str()); loaded.read(input);
        check(loaded.difficulty == level && loaded.width == 1234, "Persist every difficulty with window size");
    }
    for (const char* data : {"800 600 3 0", "800 600 3 4", "800 600 3 invalid", "800 600 3", "800 600 2"}) {
        WindowSize loaded;
        std::istringstream input(data); loaded.read(input);
        check(loaded.difficulty == 2 && loaded.width == 800, "Invalid or missing difficulty defaults to medium");
    }
    WindowSize repaired;
    std::istringstream legacyMinimum("320 240"); repaired.read(legacyMinimum);
    check(repaired.width == 960 && repaired.height == 600, "Repair legacy minimum size");
    std::istringstream intentionalMinimum("320 240 2"); repaired.read(intentionalMinimum);
    check(repaired.width == 320 && repaired.height == 240, "Preserve intentional minimum in new format");
    for (int level = 1; level <= 3; ++level) {
        Game left, right;
        left.setDifficulty(level); right.setDifficulty(level);
        left.random.seed(123); right.random.seed(123);
        left.serveTime = right.serveTime = 0;
        left.vx = -400; right.vx = 400;
        left.x = 300; right.x = right.width - 300;
        left.y = right.y = 150; left.vy = right.vy = 220;
        float lp = 300, rp = 300, lt = 0, rt = 0, la = 0, ra = 0;
        for (int n = 0; n < 100; ++n) {
            left.moveBot(lp, lt, la, left.leftMotion, true, 1.f/240);
            right.moveBot(rp, rt, ra, right.rightMotion, false, 1.f/240);
            check(std::abs(lp-rp) < .001f, "Both bots must have identical mirrored behavior");
        }
    }
    for (int level = 1; level <= 3; ++level) {
        Game smooth;
        smooth.setDifficulty(level);
        float pos = 80, timer = 0, aim = 300, previousVelocity = 0;
        float limits[] = {1000, 1500, 2200};
        for (int n = 0; n < 2400; ++n) {
            smooth.moveBot(pos, timer, aim, smooth.rightMotion, false, 1.f/240);
            check(std::abs(smooth.rightMotion.velocity - previousVelocity) <= limits[level-1]/240 + .01f,
                  "Bot acceleration must be bounded");
            previousVelocity = smooth.rightMotion.velocity;
        }
        check(std::abs(pos - 300) < .2f && smooth.rightMotion.velocity == 0, "Idle bot settles without jitter");
        smooth.serveTime = 0; smooth.vx = 400; smooth.vy = 0; smooth.y = 200;
        smooth.moveBot(pos, timer, aim, smooth.rightMotion, false, 1.f/240);
        float error = smooth.rightMotion.error;
        for (int n = 0; n < 1000; ++n) {
            smooth.moveBot(pos, timer, aim, smooth.rightMotion, false, 1.f/240);
            check(smooth.rightMotion.error == error, "Aiming error stays stable during a flight");
        }
    }
    g.toggleAuto();
    float oldTarget = g.target;
    g.scroll(5); check(g.target == oldTarget, "Auto ignores manual scroll");
    g.reset(); check(g.automatic && g.difficulty == 3, "Reset preserves auto and difficulty");
    g.player = 180; g.toggleAuto();
    check(g.target == 180 && !g.automatic, "Manual control resumes without a jump");
    g.toggleAuto();
    for (int i = 0; i < 100000; ++i) {
        if (i % 500 == 0) g.setDifficulty(1 + (i/500) % 3);
        g.step(1.f/240);
        check(std::isfinite(g.x) && std::isfinite(g.y), "Stable long-running physics");
        check(g.y >= 8 && g.y <= g.height-8, "Ball stays within vertical boundaries");
    }
    std::cout << "Physics, scoring, difficulty, restart, trackpad and resize: OK\n";
}
