#include "Simulation/Fireworks.hpp"
#include <algorithm>

namespace {

using Budget::CellSize;
using Budget::Columns;
using Budget::Rows;

constexpr std::uint64_t ShowSeed = 0x50495845'4c424f4dull;
constexpr float Step = static_cast<float>(Fireworks::StepSeconds);
constexpr float Width = static_cast<float>(Budget::ScreenWidth);
constexpr float Height = static_cast<float>(Budget::ScreenHeight);

constexpr float RocketGravity = 480.0f;
constexpr float BurstSpeed = -90.0f;
constexpr int EmberEverySteps = 2;
constexpr float ParticleDrag = 0.990f;
constexpr float CullMargin = 96.0f;

// The show loops: mostly solo shells, then a dense finale that pushes the particle budget.
constexpr std::uint64_t CycleSteps = 20 * Fireworks::StepsPerSecond;
constexpr std::uint64_t FinaleSteps = 4 * Fireworks::StepsPerSecond;

constexpr std::size_t RampSteps = 6;
constexpr std::size_t TrailRamp = 8;
constexpr std::array<std::array<Color, RampSteps>, 9> Ramps{{
    {Rgb(0xFFF5CF), Rgb(0xFFD23F), Rgb(0xF2A32E), Rgb(0xC96C28), Rgb(0x7A3A2A), Rgb(0x3A2038)},
    {Rgb(0xE8FFF6), Rgb(0x67E3C8), Rgb(0x3CB9A8), Rgb(0x2A8390), Rgb(0x215468), Rgb(0x1B2F4E)},
    {Rgb(0xFFE6EF), Rgb(0xFF6B9A), Rgb(0xE03F78), Rgb(0xA32A63), Rgb(0x5F2152), Rgb(0x2E1A44)},
    {Rgb(0xE8F2FF), Rgb(0x6FB6FF), Rgb(0x3E7FE0), Rgb(0x2D54A8), Rgb(0x233475), Rgb(0x181F4C)},
    {Rgb(0xFFEAD0), Rgb(0xFF9A3C), Rgb(0xE26A2A), Rgb(0xA8452A), Rgb(0x6A2B2C), Rgb(0x33203E)},
    {Rgb(0xF6E8FF), Rgb(0xC28BFF), Rgb(0x9259E8), Rgb(0x6840B8), Rgb(0x432D82), Rgb(0x251F52)},
    {Rgb(0xF6FFD6), Rgb(0xC4F250), Rgb(0x8FCB3A), Rgb(0x5B9636), Rgb(0x33603A), Rgb(0x1C3640)},
    {Rgb(0xFFE3DA), Rgb(0xFF6F59), Rgb(0xD6453F), Rgb(0x99303E), Rgb(0x5C2340), Rgb(0x2C1B40)},
    {Rgb(0xFFF0B0), Rgb(0xFFB347), Rgb(0xD87A2E), Rgb(0x8C4A30), Rgb(0x46283A), Rgb(0x261C38)},
}};

constexpr int floorToCell(float value) {
    const auto scaled = value / static_cast<float>(CellSize);
    const auto truncated = static_cast<int>(scaled);
    return static_cast<float>(truncated) > scaled ? truncated - 1 : truncated;
}

}

Fireworks::Fireworks() : random(ShowSeed) {}

void Fireworks::Update(double seconds) {
    if (!(seconds > 0.0)) return;
    accumulator += std::min(seconds, MaxFrameSeconds);
    while (accumulator >= StepSeconds) {
        accumulator -= StepSeconds;
        step();
    }
    rebuildQuads();
}

void Fireworks::step() {
    ++steps;
    launchCountdown -= Step;
    if (launchCountdown <= 0.0f) {
        launch();
        const bool finale = steps % CycleSteps >= CycleSteps - FinaleSteps;
        launchCountdown = finale ? random.Range(0.10f, 0.20f) : random.Range(0.45f, 1.05f);
    }

    for (std::size_t index = 0; index < rocketCount;) {
        auto& rocket = rockets[index];
        rocket.VelocityY += RocketGravity * Step;
        rocket.X += rocket.VelocityX * Step;
        rocket.Y += rocket.VelocityY * Step;

        if (rocket.VelocityY >= BurstSpeed) {
            explode(rocket);
            rockets[index] = rockets[--rocketCount];
            continue;
        }

        if (steps % EmberEverySteps == 0) {
            spawn({rocket.X, rocket.Y, random.Range(-10.0f, 10.0f), random.Range(20.0f, 60.0f), 0.0f, random.Range(0.28f, 0.42f), 120.0f, static_cast<std::uint8_t>(TrailRamp), 1});
        }

        ++index;
    }

    for (std::size_t index = 0; index < particleCount;) {
        auto& particle = particles[index];
        particle.Age += Step;
        particle.VelocityX *= ParticleDrag;
        particle.VelocityY = particle.VelocityY * ParticleDrag + particle.Gravity * Step;
        particle.X += particle.VelocityX * Step;
        particle.Y += particle.VelocityY * Step;

        const bool gone = particle.X < -CullMargin || particle.X > Width + CullMargin || particle.Y > Height + CullMargin;
        if (particle.Age >= particle.Lifetime || gone) {
            particles[index] = particles[--particleCount];
            continue;
        }

        ++index;
    }
}

void Fireworks::launch() {
    if (rocketCount == rockets.size()) return;
    const float flightSeconds = random.Range(1.55f, 1.95f);
    rockets[rocketCount++] = {
        random.Range(Width * 0.08f, Width * 0.92f),
        Height - 8.0f,
        random.Range(-30.0f, 30.0f),
        -RocketGravity * flightSeconds,
        static_cast<std::uint8_t>(random.Range(0, static_cast<int>(TrailRamp) - 1)),
    };
}

bool Fireworks::spawn(const Particle& particle) {
    if (particleCount == particles.size()) {
        ++dropped;
        return false;
    }
    particles[particleCount++] = particle;
    return true;
}

void Fireworks::explode(const Rocket& rocket) {
    const int kind = random.Range(0, 4);
    if (kind <= 1) {
        // Peony: a filled ball.
        for (int index = 0; index < 150; ++index) {
            const int angle = random.Range(0, 255);
            const float speed = random.Range(110.0f, 360.0f);
            // Mostly small sparks; a few big blocks keep the ball chunky without filling it in.
            const auto startSize = static_cast<std::uint8_t>(random.Range(0, 4) == 0 ? 3 : random.Range(1, 2));
            const bool spawned = spawn({rocket.X, rocket.Y, Trig::Cos(angle) * speed + rocket.VelocityX, Trig::Sin(angle) * speed, 0.0f, random.Range(1.1f, 1.9f), 190.0f, rocket.Ramp, startSize});
            if (!spawned) return;
        }
    } else if (kind <= 3) {
        // Ring: an evenly spaced shell with a second, smaller ring inside it.
        const auto innerRamp = static_cast<std::uint8_t>(random.Range(0, static_cast<int>(TrailRamp) - 1));
        for (int index = 0; index < 96; ++index) {
            const int angle = index * 256 / 96;
            const float speed = random.Range(300.0f, 320.0f);
            if (!spawn({rocket.X, rocket.Y, Trig::Cos(angle) * speed, Trig::Sin(angle) * speed, 0.0f, random.Range(1.2f, 1.6f), 170.0f, rocket.Ramp, 2})) return;
            if (index % 2 == 0) {
                const float innerSpeed = speed * 0.5f;
                if (!spawn({rocket.X, rocket.Y, Trig::Cos(angle) * innerSpeed, Trig::Sin(angle) * innerSpeed, 0.0f, random.Range(1.0f, 1.4f), 170.0f, innerRamp, 1})) return;
            }
        }
    } else {
        // Willow: slower sparks that hang and fall for a long time.
        for (int index = 0; index < 130; ++index) {
            const int angle = random.Range(0, 255);
            const float speed = random.Range(50.0f, 240.0f);
            if (!spawn({rocket.X, rocket.Y, Trig::Cos(angle) * speed, Trig::Sin(angle) * speed - 40.0f, 0.0f, random.Range(2.0f, 3.2f), 110.0f, rocket.Ramp, 2})) return;
        }
    }
}

void Fireworks::rebuildQuads() {
    quadCount = 0;

    for (std::size_t index = 0; index < particleCount; ++index) {
        const auto& particle = particles[index];
        const float life = std::clamp(particle.Age / particle.Lifetime, 0.0f, 1.0f);
        const auto colorStep = std::min(static_cast<std::size_t>(life * RampSteps), RampSteps - 1);

        // Shrinks in whole-cell steps so the block never leaves the 12-pixel grid.
        int size = particle.StartSize;
        if (life >= 0.75f) size = 1;
        else if (life >= 0.4f) size = std::max(size - 1, 1);

        const int column = floorToCell(particle.X) - (size - 1) / 2;
        const int row = floorToCell(particle.Y) - (size - 1) / 2;
        // A spark above the sky stays alive and falls back in; it just draws nothing while it is out.
        if (column < 0 || row < 0 || column + size > Columns || row + size > Rows) continue;

        const auto pixels = static_cast<std::int16_t>(size * CellSize);
        quads[quadCount++] = {static_cast<std::int16_t>(column * CellSize), static_cast<std::int16_t>(row * CellSize), pixels, pixels, Ramps[particle.Ramp][colorStep]};
    }

    for (std::size_t index = 0; index < rocketCount; ++index) {
        const auto& rocket = rockets[index];
        const int column = floorToCell(rocket.X);
        const int row = floorToCell(rocket.Y) - 1;
        if (column < 0 || row < 0 || column + 1 > Columns || row + 2 > Rows) continue;

        quads[quadCount++] = {static_cast<std::int16_t>(column * CellSize), static_cast<std::int16_t>(row * CellSize), CellSize, CellSize * 2, Ramps[TrailRamp][0]};
    }
}
