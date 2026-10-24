#pragma once
#include <algorithm>
#include <cmath>
#include <random>

struct Game {
    static constexpr float paddleWidth = 12, paddleHeight = 112, inset = 38, radius = 8;
    // Technical ceiling: 150,000 screen pixels/s including the simulation multiplier.
    // Swept collisions below remain accurate even across multiple hits in one tick.
    static constexpr float impossibleMaxSpeed = 100000;
    float width = 960, height = 600;
    float player = 300, bot = 300, target = 300;
    float x = 480, y = 300, vx = 350, vy = 120;
    float serveTime = .75f, thinkTime = 0, botTarget = 300;
    struct BotMotion { float velocity = 0, error = 0, shotOffset = 0; bool tracking = false; };
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
    void setDifficulty(int level) {
        int next = std::clamp(level, 1, 4);
        bool restart = next != difficulty && (next == 4 || difficulty == 4);
        difficulty = next;
        thinkTime = playerThinkTime = 0;
        leftMotion = {}; rightMotion = {};
        if (restart) reset();
    }
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
        float speed = std::min(difficulty == 4 ? impossibleMaxSpeed : 780.f, std::hypot(vx, vy) * 1.045f);
        vx = direction * speed * std::cos(offset * 1.05f);
        vy = speed * std::sin(offset * 1.05f);
    }
    void moveBot(float& position, float& timer, float& aim, BotMotion& motion, bool left, float dt) {
        bool approaching = (left ? vx < 0 : vx > 0) && serveTime <= 0;
        if (difficulty == 4) {
            timer = 0;
            motion.velocity = motion.error = 0;
            // Pick a deliberate contact offset once per incoming flight. Keep a
            // generous catching margin while varying both angle and direction.
            if (approaching && !motion.tracking) {
                motion.shotOffset = std::uniform_real_distribution<float>(.3f, .85f)(random) * paddleHeight / 2;
                if (std::uniform_int_distribution<int>(0, 1)(random)) motion.shotOffset = -motion.shotOffset;
            }
            motion.tracking = approaching;
            aim = height / 2;
            if (approaching) {
                float intercept = predictedY(left);
                aim = clampPaddle(intercept - motion.shotOffset);
                // Near a wall, choose the other direction if the paddle bounds
                // would turn the intended angled shot into a central hit.
                if (std::abs(intercept - aim) < .3f * paddleHeight / 2)
                    aim = clampPaddle(intercept + motion.shotOffset);
            }
            position = aim;
            return;
        }
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
    // The caller advances at 240 Hz; resolve impacts in chronological order so a
    // fast ball cannot tunnel through paddles or skip multiple wall reflections.
    void step(float dt) {
        if (automatic) moveBot(player, playerThinkTime, playerBotTarget, leftMotion, true, dt);
        else player += (target - player) * (1 - std::exp(-28 * dt));
        moveBot(bot, thinkTime, botTarget, rightMotion, false, dt);
        if (serveTime > 0) { serveTime = std::max(0.f, serveTime - dt); return; }
        const float leftFace = inset + paddleWidth + radius;
        const float rightFace = width - inset - paddleWidth - radius;
        float remaining = dt * 1.5f;
        while (remaining > 0) {
            // A tick can include several paddle hits. Perfect bots must also react
            // to each new incoming flight within that tick, on either side.
            if (difficulty == 4) {
                if (automatic) moveBot(player, playerThinkTime, playerBotTarget, leftMotion, true, 0);
                moveBot(bot, thinkTime, botTarget, rightMotion, false, 0);
            }
            float travel = remaining;
            enum Impact { none, wall, paddle, goal } impact = none;
            auto consider = [&](float time, Impact candidate) {
                if (time >= 0 && time <= travel) { travel = time; impact = candidate; }
            };
            if (vy < 0) consider((radius - y) / vy, wall);
            else if (vy > 0) consider((height - radius - y) / vy, wall);
            if (vx < 0) {
                consider((radius - x) / vx, goal);
                if (x >= leftFace) consider((leftFace - x) / vx, paddle);
            } else if (vx > 0) {
                consider((width - radius - x) / vx, goal);
                if (x <= rightFace) consider((rightFace - x) / vx, paddle);
            }
            x += vx * travel;
            y = std::clamp(y + vy * travel, radius, height - radius);
            remaining = std::max(0.f, remaining - travel);
            if (impact == wall) {
                vy = -vy;
            } else if (impact == paddle) {
                bool left = vx < 0;
                float center = left ? player : bot;
                if (std::abs(y - center) <= paddleHeight / 2 + radius) {
                    x = left ? leftFace : rightFace;
                    bounce(center, left ? 1 : -1);
                } else {
                    // Move just beyond the missed face so it is not detected again.
                    x = std::nextafter(left ? leftFace : rightFace, left ? 0.f : width);
                }
            } else if (impact == goal) {
                if (vx < 0) { ++rightScore; serve(-1); }
                else { ++leftScore; serve(1); }
                return;
            } else break;
        }
    }
};
