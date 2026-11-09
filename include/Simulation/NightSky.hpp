#ifndef PixelFireworks_Simulation_NightSky_hpp
#define PixelFireworks_Simulation_NightSky_hpp
#include "Simulation/Budget.hpp"
#include <array>
#include <cstdint>
#include <span>

class NightSky final {
public:
    NightSky();
    std::span<const Color, Budget::SkyCells> Cells() const { return cells; }
    std::span<const StarPixel> Stars() const { return std::span(pixels).first(starCount); }
    // Recolors every star for the given number of elapsed simulation steps.
    void Update(std::uint64_t steps);

private:
    struct Star {
        std::uint16_t Cell;
        std::uint16_t StepsPerTone;
        std::uint8_t Phase;
        std::uint8_t Tint;
    };

    void paintGradient();
    void paintMoon();
    void placeStars();
    void paintHills();
    void paintCity();

    std::array<Color, Budget::SkyCells> cells{};
    std::array<Star, Budget::MaxStars> stars{};
    std::array<StarPixel, Budget::MaxStars> pixels{};
    std::size_t starCount = 0;
};

#endif
