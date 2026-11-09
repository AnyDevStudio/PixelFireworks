#include "Simulation/NightSky.hpp"
#include "Simulation/Random.hpp"
#include <algorithm>

namespace {

using Budget::Columns;
using Budget::Rows;

constexpr std::uint64_t SkySeed = 0x46495245574f524bull;
constexpr int HorizonRow = 74;
constexpr int StarLowestRow = 58;
constexpr int MoonColumn = 128;
constexpr int MoonRow = 19;
constexpr int MoonRadius = 9;
constexpr int HaloRadius = MoonRadius + 5;

// Deep navy at the zenith down to dusk purple at the horizon.
constexpr std::array Gradient{
    Rgb(0x070A26), Rgb(0x0A0E30), Rgb(0x0F123B), Rgb(0x171648),
    Rgb(0x221B57), Rgb(0x322167), Rgb(0x472775), Rgb(0x602E82),
};

constexpr std::array<std::array<float, 4>, 4> Bayer{{
    {{0.5f / 16, 8.5f / 16, 2.5f / 16, 10.5f / 16}},
    {{12.5f / 16, 4.5f / 16, 14.5f / 16, 6.5f / 16}},
    {{3.5f / 16, 11.5f / 16, 1.5f / 16, 9.5f / 16}},
    {{15.5f / 16, 7.5f / 16, 13.5f / 16, 5.5f / 16}},
}};

constexpr float bayerAt(int column, int row) {
    return Bayer[static_cast<std::size_t>(row & 3)][static_cast<std::size_t>(column & 3)];
}

constexpr std::size_t cellIndex(int column, int row) {
    return static_cast<std::size_t>(row) * Columns + static_cast<std::size_t>(column);
}

// Tones from dimmest to peak; tint 0 is cool white, tint 1 is warm.
constexpr std::array<std::array<Color, 4>, 2> StarTones{{
    {Rgb(0x5E639C), Rgb(0x9A9FD4), Rgb(0xDFE2FF), Rgb(0xFFFFFF)},
    {Rgb(0x7A667C), Rgb(0xC2A68C), Rgb(0xF2DCB2), Rgb(0xFFF4D2)},
}};

constexpr std::array<std::uint8_t, 16> TwinkleCycle{1, 2, 3, 3, 2, 1, 1, 2, 2, 1, 0, 0, 1, 1, 2, 1};

struct Crater {
    int Column;
    int Row;
    int RadiusSquared;
};

constexpr std::array Craters{
    Crater{-3, -3, 4}, Crater{3, -1, 5}, Crater{-1, 4, 3}, Crater{4, 5, 2}, Crater{-5, 2, 1},
};

}

NightSky::NightSky() {
    paintGradient();
    paintMoon();
    placeStars();
    paintHills();
    paintCity();
    Update(0);
}

void NightSky::paintGradient() {
    const int lastBand = static_cast<int>(Gradient.size()) - 1;
    for (int row = 0; row < Rows; ++row) {
        const float height = std::min(static_cast<float>(row) / static_cast<float>(HorizonRow), 1.0f);
        // Squaring part of the range holds the navy for longer before the purple comes in.
        const float eased = height * (0.45f + 0.55f * height);
        const float scaled = eased * static_cast<float>(lastBand);
        const int base = static_cast<int>(scaled);
        const float fraction = scaled - static_cast<float>(base);

        for (int column = 0; column < Columns; ++column) {
            const int band = std::min(base + (fraction > bayerAt(column, row) ? 1 : 0), lastBand);
            cells[cellIndex(column, row)] = Gradient[static_cast<std::size_t>(band)];
        }
    }
}

void NightSky::paintMoon() {
    const Color face = Rgb(0xF4EBCF);
    const Color shade = Rgb(0xCFC3A3);
    const Color crater = Rgb(0xB7AB8E);
    const Color halo = Rgb(0x25226A);

    for (int row = MoonRow - HaloRadius; row <= MoonRow + HaloRadius; ++row) {
        for (int column = MoonColumn - HaloRadius; column <= MoonColumn + HaloRadius; ++column) {
            if (row < 0 || row >= Rows || column < 0 || column >= Columns) continue;
            const int dx = column - MoonColumn;
            const int dy = row - MoonRow;
            const int distance = dx * dx + dy * dy;
            auto& cell = cells[cellIndex(column, row)];

            if (distance > MoonRadius * MoonRadius + MoonRadius) {
                // The glow thins out with distance by dithering against the sky.
                const float reach = 1.0f - static_cast<float>(distance) / static_cast<float>(HaloRadius * HaloRadius);
                if (reach > 0.0f && reach * 1.4f > bayerAt(column, row)) cell = halo;
                continue;
            }

            // The shadow side is dithered so the disc reads as round without a third moon tone.
            const bool shaded = dx * 5 + dy * 3 < -28 && bayerAt(column, row) < 0.6f;
            cell = shaded ? shade : face;
            for (const auto& spot : Craters) {
                const int craterX = dx - spot.Column;
                const int craterY = dy - spot.Row;
                if (craterX * craterX + craterY * craterY <= spot.RadiusSquared) cell = crater;
            }
        }
    }
}

void NightSky::placeStars() {
    Random random(SkySeed);
    int attempts = 0;
    while (starCount < stars.size() && attempts++ < 20000) {
        const int column = random.Range(1, Columns - 2);
        // Favoring the smaller of two rolls thins the stars out toward the horizon.
        const int row = std::min(random.Range(0, StarLowestRow), random.Range(0, StarLowestRow));
        const int dx = column - MoonColumn;
        const int dy = row - MoonRow;
        if (dx * dx + dy * dy <= HaloRadius * HaloRadius) continue;

        const auto cell = static_cast<std::uint16_t>(cellIndex(column, row));
        const bool taken = std::any_of(stars.begin(), stars.begin() + static_cast<std::ptrdiff_t>(starCount), [cell](const Star& star) { return star.Cell == cell; });
        if (taken) continue;

        stars[starCount] = {
            cell,
            static_cast<std::uint16_t>(random.Range(10, 34)),
            static_cast<std::uint8_t>(random.Range(0, 15)),
            static_cast<std::uint8_t>(random.Range(0, 3) == 0 ? 1 : 0),
        };
        pixels[starCount].Cell = cell;

        // Every twelfth star gets a dim plus shape so the field has a few focal points.
        if (starCount % 12 == 0) {
            const Color arm = StarTones[stars[starCount].Tint][0];
            cells[cellIndex(column - 1, row)] = arm;
            cells[cellIndex(column + 1, row)] = arm;
            if (row > 0) cells[cellIndex(column, row - 1)] = arm;
            cells[cellIndex(column, row + 1)] = arm;
        }

        ++starCount;
    }
}

void NightSky::paintHills() {
    const Color far = Rgb(0x2B1A56);
    const Color near = Rgb(0x1A1040);

    for (int column = 0; column < Columns; ++column) {
        const float farHeight = 15.0f + 4.0f * Trig::Sin(column * 3 + 20) + 2.0f * Trig::Sin(column * 7 + 90);
        const float nearHeight = 10.0f + 3.0f * Trig::Sin(column * 5 + 170) + 1.5f * Trig::Sin(column * 11);
        const int farTop = Rows - static_cast<int>(farHeight);
        const int nearTop = Rows - static_cast<int>(nearHeight);

        for (int row = farTop; row < Rows; ++row) cells[cellIndex(column, row)] = row >= nearTop ? near : far;
    }
}

void NightSky::paintCity() {
    const Color wall = Rgb(0x0A0A20);
    const Color ground = Rgb(0x07071A);
    const Color windowOff = Rgb(0x15142F);
    const Color windowWarm = Rgb(0xE8B05A);
    const Color windowCool = Rgb(0x67E3C8);
    Random random(SkySeed ^ 0x9e3779b97f4a7c15ull);
    constexpr int GroundRows = 2;
    constexpr int CityFloor = Rows - GroundRows;

    for (int row = CityFloor; row < Rows; ++row) {
        for (int column = 0; column < Columns; ++column) cells[cellIndex(column, row)] = ground;
    }

    int column = 0;
    while (column < Columns) {
        const int width = random.Range(5, 12);
        const bool tower = random.Range(0, 6) == 0;
        const int height = tower ? random.Range(18, 24) : random.Range(5, 15);
        const int top = CityFloor - height;
        const int right = std::min(column + width, Columns);

        for (int x = column; x < right; ++x) {
            for (int y = top; y < CityFloor; ++y) {
                const bool windowSlot = (x - column) % 2 == 1 && (x + 1) < right && (y - top) % 2 == 1 && y + 1 < CityFloor;
                Color tint = wall;
                if (windowSlot) {
                    const int roll = random.Range(0, 99);
                    tint = roll < 18 ? windowWarm : roll < 21 ? windowCool : windowOff;
                }
                cells[cellIndex(x, y)] = tint;
            }
        }

        if (random.Range(0, 3) == 0) {
            const int mast = column + width / 2;
            if (mast < Columns) {
                const int mastTop = top - random.Range(2, 4);
                for (int y = mastTop; y < top; ++y) cells[cellIndex(mast, y)] = wall;
            }
        }

        // Most blocks sit shoulder to shoulder; a rare gap lets the hills show through.
        column = right + (random.Range(0, 4) == 0 ? random.Range(1, 3) : 0);
    }
}

void NightSky::Update(std::uint64_t steps) {
    for (std::size_t index = 0; index < starCount; ++index) {
        const auto& star = stars[index];
        const auto step = (steps / star.StepsPerTone + star.Phase) % TwinkleCycle.size();
        pixels[index].Tint = StarTones[star.Tint][TwinkleCycle[step]];
    }
}
