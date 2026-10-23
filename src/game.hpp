#pragma once
#include <algorithm>
#include <cmath>
#include <random>

struct Game {
    static constexpr float paddleWidth = 12, paddleHeight = 112, inset = 38, radius = 8;
    float width = 960, height = 600;
    float player = 300, bot = 300, target = 300;
    float x = 480, y = 300, vx = 350, vy = 120;
    float serveTime = .75f, thinkTime = 0, botTarget = 300;
    struct BotMotion { float velocity = 0, error = 0; bool tracking = false; };
    BotMotion leftMotion, rightMotion;
    bool automatic = false;
    float playerThinkTime = 0, playerBotTarget = 300;
    int leftScore = 0, rightScore = 0, difficulty = 2;
    std::mt19937 random{std::random_device{}()};

    float clampPaddle(float value) const {
        return std::clamp(value, paddleHeight / 2, height - paddleHeight / 2);
    }
    void resize(float w, float h) {
        width = std::max(w, 320.f); height = std::max(h, 240.f);
        player = clampPaddle(player); bot = clampPaddle(bot); target = clampPaddle(target);
        x = std::clamp(x, radius + 1, width - radius - 1);
        y = std::clamp(y, radius, height - radius);
        if (serveTime > 0) { x = width / 2; y = height / 2; }
        thinkTime = playerThinkTime = 0;
        leftMotion.velocity = rightMotion.velocity = 0;
    }
    void serve(int direction) {
        x = width / 2; y = height / 2;
        float angle = std::uniform_real_distribution<float>(-.48f, .48f)(random);
        vx = direction * 370 * std::cos(angle); vy = 370 * std::sin(angle);
        serveTime = .75f; thinkTime = playerThinkTime = 0;
        leftMotion.tracking = rightMotion.tracking = false;
    }
    void reset() {
        leftScore = rightScore = 0;
        player = bot = target = botTarget = playerBotTarget = height / 2;
        leftMotion = {}; rightMotion = {};
        serve(std::uniform_int_distribution<int>(0, 1)(random) ? 1 : -1);
    }
    void toggleAuto() { automatic = !automatic; target = player; playerThinkTime = 0; leftMotion = {}; }
    void scroll(float amount) { if (!automatic) target = clampPaddle(target - amount * 38); }
    void setDifficulty(int level) { difficulty = std::clamp(level, 1, 3); thinkTime = playerThinkTime = 0;
        leftMotion.tracking = rightMotion.tracking = false; }
    float predictedY(bool left = false) const {
        float face = left ? inset + paddleWidth + radius : width - inset - paddleWidth - radius;
        float t = std::abs(vx) > .001f ? std::max(0.f, (face - x) / vx) : 0;
        float span = height - 2 * radius;
        float p = std::fmod(y - radius + vy * t, span * 2);
        if (p < 0) p += span * 2;
        return radius + (p <= span ? p : span * 2 - p);
    }
    void bounce(float center, int direction) {
        float offset = std::clamp((y - center) / (paddleHeight / 2 + radius), -1.f, 1.f);
        float speed = std::min(780.f, std::hypot(vx, vy) * 1.045f);
        vx = direction * speed * std::cos(offset * 1.05f);
        vy = speed * std::sin(offset * 1.05f);
    }
    void moveBot(float& position, float& timer, float& aim, BotMotion& motion, bool left, float dt) {
        bool approaching = (left ? vx < 0 : vx > 0) && serveTime <= 0;
        // One aiming error per incoming flight, not a new random target every reaction tick.
        if (approaching && !motion.tracking) {
            const float error[] = {65, 28, 9};
            motion.error = std::uniform_real_distribution<float>(-error[difficulty-1], error[difficulty-1])(random);
        }
        motion.tracking = approaching;
        timer -= dt;
        if (timer <= 0) {
            const float reaction[] = {.26f, .13f, .055f};
            timer = reaction[difficulty - 1];
            aim = approaching ? (difficulty == 1 ? y : predictedY(left)) + motion.error : height / 2;
            aim = clampPaddle(aim);
        }
        const float maxSpeed[] = {210, 330, 490};
        const float maxAcceleration[] = {1000, 1500, 2200};
        // Critically damped spring: slow near the target, accelerate for distant balls.
        float distance = aim - position;
        float acceleration = 81 * distance - 18 * motion.velocity;
        motion.velocity += std::clamp(acceleration, -maxAcceleration[difficulty-1], maxAcceleration[difficulty-1]) * dt;
        motion.velocity = std::clamp(motion.velocity, -maxSpeed[difficulty-1], maxSpeed[difficulty-1]);
        if (std::abs(distance) < .1f && std::abs(motion.velocity) < 1.f) motion.velocity = 0;
        float next = position + motion.velocity * dt;
        position = clampPaddle(next);
        if (position != next) motion.velocity = 0;
    }
    // The caller advances at 240 Hz. Even at maximum speed the ball moves < 4 px.
    void step(float dt) {
        if (automatic) moveBot(player, playerThinkTime, playerBotTarget, leftMotion, true, dt);
        else player += (target - player) * (1 - std::exp(-28 * dt));
        moveBot(bot, thinkTime, botTarget, rightMotion, false, dt);
        if (serveTime > 0) { serveTime = std::max(0.f, serveTime - dt); return; }
        float oldX = x;
        x += vx * dt * 1.5; y += vy * dt * 1.5;
        if (y < radius) { y = 2 * radius - y; vy = std::abs(vy); }
        if (y > height - radius) { y = 2 * (height - radius) - y; vy = -std::abs(vy); }
        float leftFace = inset + paddleWidth + radius;
        float rightFace = width - inset - paddleWidth - radius;
        if (vx < 0 && oldX >= leftFace && x <= leftFace && std::abs(y-player) <= paddleHeight/2 + radius) {
            x = leftFace; bounce(player, 1);
        }
        if (vx > 0 && oldX <= rightFace && x >= rightFace && std::abs(y-bot) <= paddleHeight/2 + radius) {
            x = rightFace; bounce(bot, -1);
        }
        if (x <= radius) { ++rightScore; serve(-1); }
        else if (x >= width - radius) { ++leftScore; serve(1); }
    }
};
