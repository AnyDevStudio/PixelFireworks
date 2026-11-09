#ifndef PixelFireworks_Simulation_Budget_hpp
#define PixelFireworks_Simulation_Budget_hpp
#include <cstddef>
#include <cstdint>

// Every buffer in the demo, CPU or GPU, is sized from these numbers before the first frame.
namespace Budget {

inline constexpr int ScreenWidth = 1920;
inline constexpr int ScreenHeight = 1080;
inline constexpr int CellSize = 12;
inline constexpr int Columns = ScreenWidth / CellSize;
inline constexpr int Rows = ScreenHeight / CellSize;

inline constexpr std::size_t SkyCells = static_cast<std::size_t>(Columns) * Rows;
inline constexpr std::size_t MaxStars = 170;
inline constexpr std::size_t MaxParticles = 2000;
inline constexpr std::size_t MaxRockets = 16;
inline constexpr std::size_t MaxDynamicQuads = MaxParticles + MaxRockets;
inline constexpr std::size_t MaxQuads = SkyCells + MaxDynamicQuads;

}

struct Color {
    std::uint8_t Red;
    std::uint8_t Green;
    std::uint8_t Blue;
};

constexpr bool operator==(Color left, Color right) {
    return left.Red == right.Red && left.Green == right.Green && left.Blue == right.Blue;
}

constexpr Color Rgb(std::uint32_t hex) {
    return {static_cast<std::uint8_t>(hex >> 16), static_cast<std::uint8_t>(hex >> 8), static_cast<std::uint8_t>(hex)};
}

// One solid rectangle in screen pixels. Coordinates and sizes are always multiples of Budget::CellSize.
struct PixelQuad {
    std::int16_t X;
    std::int16_t Y;
    std::int16_t Width;
    std::int16_t Height;
    Color Tint;
};

// A sky cell whose color changes this frame, addressed by its index in the sky grid.
struct StarPixel {
    std::uint16_t Cell;
    Color Tint;
};

#endif
