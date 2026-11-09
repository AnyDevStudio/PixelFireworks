#ifndef PixelFireworks_Simulation_Random_hpp
#define PixelFireworks_Simulation_Random_hpp
#include <array>
#include <cstddef>
#include <cstdint>

// PCG32. The sequence depends only on the seed, so every run of the demo is identical.
class Random final {
public:
    explicit Random(std::uint64_t seed) : state(seed + Increment) { Next(); }

    std::uint32_t Next() {
        const auto previous = state;
        state = previous * 6364136223846793005ull + Increment;
        const auto shifted = static_cast<std::uint32_t>(((previous >> 18) ^ previous) >> 27);
        const auto rotation = static_cast<std::uint32_t>(previous >> 59);
        return (shifted >> rotation) | (shifted << ((0u - rotation) & 31u));
    }

    // Inclusive on both ends.
    int Range(int minimum, int maximum) {
        const auto span = static_cast<std::uint32_t>(maximum - minimum) + 1u;
        return minimum + static_cast<int>(Next() % span);
    }

    float Range(float minimum, float maximum) {
        const auto unit = static_cast<float>(Next() >> 8) / 16777216.0f;
        return minimum + (maximum - minimum) * unit;
    }

private:
    static constexpr std::uint64_t Increment = 0x14057b7ef767814full;
    std::uint64_t state;
};

// Sine over a 256-step circle. A table keeps the demo off libm and gives the same answer on every compiler.
namespace Trig {

consteval std::array<float, 256> buildSine() {
    constexpr double Pi = 3.14159265358979323846;
    std::array<float, 256> table{};
    for (std::size_t index = 0; index < table.size(); ++index) {
        double angle = 2.0 * Pi * static_cast<double>(index) / 256.0;
        if (angle > Pi) angle -= 2.0 * Pi;
        double term = angle;
        double sum = 0.0;
        for (int power = 1; power < 22; power += 2) {
            sum += term;
            term *= -angle * angle / (static_cast<double>(power + 1) * static_cast<double>(power + 2));
        }
        table[index] = static_cast<float>(sum);
    }
    return table;
}

inline constexpr std::array<float, 256> SineTable = buildSine();

constexpr float Sin(int step) { return SineTable[static_cast<std::size_t>(step & 255)]; }
constexpr float Cos(int step) { return SineTable[static_cast<std::size_t>((step + 64) & 255)]; }

}

#endif
