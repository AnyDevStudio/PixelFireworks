#ifndef PixelFireworks_Simulation_Fireworks_hpp
#define PixelFireworks_Simulation_Fireworks_hpp
#include "Simulation/Budget.hpp"
#include "Simulation/Random.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

// A free-running fireworks show. It advances in fixed steps fed by elapsed time, so the motion is the
// same at 60 Hz, 120 Hz or on a stalled frame, and never touches the heap after construction.
class Fireworks final {
public:
    static constexpr int StepsPerSecond = 120;
    static constexpr double StepSeconds = 1.0 / StepsPerSecond;
    // A longer gap than this (a hitch, a debugger) is dropped rather than replayed in one frame.
    static constexpr double MaxFrameSeconds = 0.1;

    Fireworks();
    void Update(double seconds);
    std::span<const PixelQuad> Quads() const { return std::span(quads).first(quadCount); }
    std::uint64_t Steps() const { return steps; }
    std::size_t ParticleCount() const { return particleCount; }
    std::size_t RocketCount() const { return rocketCount; }
    // Sparks refused because the particle pool was full; the show clips a burst rather than overrun.
    std::uint64_t DroppedParticles() const { return dropped; }

private:
    struct Particle {
        float X;
        float Y;
        float VelocityX;
        float VelocityY;
        float Age;
        float Lifetime;
        float Gravity;
        std::uint8_t Ramp;
        std::uint8_t StartSize;
    };

    struct Rocket {
        float X;
        float Y;
        float VelocityX;
        float VelocityY;
        std::uint8_t Ramp;
    };

    void step();
    void launch();
    void explode(const Rocket& rocket);
    bool spawn(const Particle& particle);
    void rebuildQuads();

    Random random;
    std::array<Particle, Budget::MaxParticles> particles{};
    std::array<Rocket, Budget::MaxRockets> rockets{};
    std::array<PixelQuad, Budget::MaxDynamicQuads> quads{};
    std::size_t particleCount = 0;
    std::size_t rocketCount = 0;
    std::size_t quadCount = 0;
    std::uint64_t steps = 0;
    std::uint64_t dropped = 0;
    double accumulator = 0.0;
    float launchCountdown = 0.0f;
};

#endif
