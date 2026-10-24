#include "game.hpp"
#include "settings.hpp"
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <utility>

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
    for (int level = 1; level <= 4; ++level) {
        size.difficulty = level;
        std::ostringstream output; size.write(output);
        WindowSize loaded;
        std::istringstream input(output.str()); loaded.read(input);
        check(loaded.difficulty == level && loaded.width == 1234, "Persist every difficulty with window size");
    }
    for (const char* data : {"800 600 3 0", "800 600 3 5", "800 600 3 invalid", "800 600 3", "800 600 2"}) {
        WindowSize loaded;
        std::istringstream input(data); loaded.read(input);
        check(loaded.difficulty == 2 && loaded.width == 800, "Invalid or missing difficulty defaults to medium");
    }
    WindowSize repaired;
    std::istringstream legacyMinimum("320 240"); repaired.read(legacyMinimum);
    check(repaired.width == 960 && repaired.height == 600, "Repair legacy minimum size");
    std::istringstream intentionalMinimum("320 240 2"); repaired.read(intentionalMinimum);
    check(repaired.width == 320 && repaired.height == 240, "Preserve intentional minimum in new format");
    for (int level = 1; level <= 4; ++level) {
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
    for (int ordinary = 1; ordinary <= 3; ++ordinary) {
        Game transition;
        transition.setDifficulty(ordinary);
        transition.toggleAuto();
        transition.leftScore = 7; transition.rightScore = 8;
        transition.vx = 780; transition.serveTime = 0;
        transition.setDifficulty(4);
        check(transition.difficulty == 4 && transition.automatic && transition.serveTime > 0
              && !transition.leftScore && !transition.rightScore && transition.x == transition.width / 2
              && std::abs(std::hypot(transition.vx, transition.vy) - 370) < .01f,
              "Entering imposible resets match and preserves auto");
        transition.leftScore = 2; transition.vx = 50000; transition.serveTime = 0;
        transition.setDifficulty(4);
        check(transition.leftScore == 2 && transition.vx == 50000 && transition.serveTime == 0,
              "Selecting imposible again preserves rally");
        transition.setDifficulty(ordinary);
        check(transition.difficulty == ordinary && transition.automatic && transition.serveTime > 0
              && !transition.leftScore && std::abs(std::hypot(transition.vx, transition.vy) - 370) < .01f
              && transition.leftMotion.velocity == 0 && transition.rightMotion.velocity == 0,
              "Leaving imposible restores ordinary speed and bot motion");
        transition.bounce(transition.y, 1);
        check(std::hypot(transition.vx, transition.vy) <= 780.01f, "Ordinary speed ceiling restored");
    }
    Game angled;
    angled.setDifficulty(4); angled.random.seed(456); angled.serveTime = 0;
    bool shotUp = false, shotDown = false;
    for (bool left : {false, true}) {
        for (float intercept : {8.f, 56.f, 70.f, 300.f, 530.f, 544.f, 592.f}) {
            for (int shot = 0; shot < 40; ++shot) {
                auto& motion = left ? angled.leftMotion : angled.rightMotion;
                motion = {};
                angled.x = angled.width / 2; angled.y = intercept;
                angled.vx = left ? -1000 : 1000; angled.vy = 0;
                float position = 300, timer = 0, aim = 300;
                angled.moveBot(position, timer, aim, motion, left, 1.f / 240);
                float offset = motion.shotOffset;
                float firstAim = aim;
                angled.moveBot(position, timer, aim, motion, left, 0);
                check(motion.shotOffset == offset && aim == firstAim,
                      "Imposible shot choice stays stable during incoming flight");
                check(position >= Game::paddleHeight / 2 && position <= angled.height - Game::paddleHeight / 2,
                      "Angled bot respects paddle boundaries");
                check(std::abs(intercept - position) >= .3f * Game::paddleHeight / 2 - .001f
                      && std::abs(intercept - position) <= Game::paddleHeight / 2,
                      "Angled bot catches safely off center even near walls");
                angled.bounce(position, left ? 1 : -1);
                check(std::abs(angled.vy) > 200, "Imposible produces a substantial vertical shot");
                if (intercept == 300) {
                    shotUp = shotUp || angled.vy < 0;
                    shotDown = shotDown || angled.vy > 0;
                }
            }
        }
    }
    check(shotUp && shotDown, "Imposible varies shots upward and downward");
    Game extreme;
    extreme.setDifficulty(4); extreme.toggleAuto(); extreme.random.seed(789);
    extreme.serveTime = 0;
    extreme.vx = 700; extreme.vy = 100;
    extreme.bounce(extreme.y, 1);
    extreme.bounce(extreme.y, 1);
    extreme.bounce(extreme.y, 1);
    check(std::hypot(extreme.vx, extreme.vy) > 780, "Imposible accelerates beyond ordinary ceiling");
    for (int i = 0; i < 500; ++i) extreme.bounce(extreme.y, 1);
    check(std::abs(std::hypot(extreme.vx, extreme.vy) - Game::impossibleMaxSpeed) < 1,
          "Technical speed ceiling prevents runaway physics cost");
    for (auto dimensions : {std::pair<float, float>{320, 240}, {960, 600}, {16384, 16384}}) {
        extreme.resize(dimensions.first, dimensions.second);
        extreme.x = extreme.width / 2; extreme.y = extreme.height / 2;
        extreme.vx = 60000; extreme.vy = -80000;
        for (int i = 0; i < 20000; ++i) {
            extreme.step(1.f / 240);
            check(std::isfinite(extreme.x) && std::isfinite(extreme.y)
                  && std::isfinite(extreme.vx) && std::isfinite(extreme.vy), "Finite extreme-speed physics");
            check(extreme.y >= Game::radius && extreme.y <= extreme.height - Game::radius,
                  "Extreme ball stays within walls");
            check(extreme.x >= Game::inset + Game::paddleWidth + Game::radius - .01f
                  && extreme.x <= extreme.width - Game::inset - Game::paddleWidth - Game::radius + .01f,
                  "Extreme ball cannot tunnel through perfect auto paddles");
            check(!extreme.leftScore && !extreme.rightScore, "Perfect auto bots never miss at technical ceiling");
        }
    }
    Game swept;
    swept.setDifficulty(4); swept.serveTime = 0;
    swept.x = 100; swept.y = 120; swept.vx = -50000; swept.vy = 0;
    swept.player = swept.target = 120;
    swept.step(1.f / 240);
    check(swept.vx > 0 && !swept.rightScore, "Swept paddle hit despite crossing goal in one tick");
    swept.x = 100; swept.y = 200; swept.vx = -50000; swept.vy = 0;
    swept.player = swept.target = 56;
    swept.step(1.f / 240);
    check(swept.rightScore == 1 && swept.serveTime > 0, "Fast missed paddle still scores");
    swept.serveTime = 0; swept.x = 480; swept.y = 20; swept.vx = 0; swept.vy = -100000;
    float expected = std::fmod(20 - Game::radius + swept.vy * (1.f / 240 * 1.5f),
                               2 * (swept.height - 2 * Game::radius));
    if (expected < 0) expected += 2 * (swept.height - 2 * Game::radius);
    expected = Game::radius + std::min(expected, 2 * (swept.height - 2 * Game::radius) - expected);
    swept.step(1.f / 240);
    check(std::abs(swept.y - expected) < .01f, "Multiple wall reflections use full remaining time");
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
