// Host-side checks for the simulation. Nothing here touches the SDK or a GPU.
//
//   clang++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
//       -Wall -Wextra -Wpedantic -Werror -Iinclude tests/SimulationTest.cpp \
//       src/Simulation/NightSky.cpp src/Simulation/Fireworks.cpp -o build/SimulationTest
#include "Simulation/Fireworks.hpp"
#include "Simulation/NightSky.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <vector>

namespace {

std::atomic<std::size_t> allocations{0};

int failures = 0;
#define EXPECT(condition) do { if (!(condition)) { ++failures; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); } } while (false)

std::uint64_t hashBytes(const void* data, std::size_t bytes, std::uint64_t seed = 1469598103934665603ull) {
    const auto* cursor = static_cast<const unsigned char*>(data);
    for (std::size_t index = 0; index < bytes; ++index) seed = (seed ^ cursor[index]) * 1099511628211ull;
    return seed;
}

std::uint64_t hashSky(const NightSky& sky) {
    auto hash = hashBytes(sky.Cells().data(), sky.Cells().size_bytes());
    return hashBytes(sky.Stars().data(), sky.Stars().size_bytes(), hash);
}

void checkSky() {
    const auto first = std::make_unique<NightSky>();
    const auto second = std::make_unique<NightSky>();
    EXPECT(first->Cells().size() == 14400);
    EXPECT(hashSky(*first) == hashSky(*second));
    EXPECT(first->Stars().size() == Budget::MaxStars);

    std::vector<bool> seen(Budget::SkyCells, false);
    for (const auto& star : first->Stars()) {
        EXPECT(star.Cell < Budget::SkyCells);
        EXPECT(!seen[star.Cell]);
        seen[star.Cell] = true;
    }

    const auto cell = [&](int column, int row) { return first->Cells()[static_cast<std::size_t>(row) * Budget::Columns + static_cast<std::size_t>(column)]; };
    // Most of the top row is the deep navy; stars may replace a few cells.
    int navyCells = 0;
    int duskCells = 0;
    for (int column = 0; column < Budget::Columns; ++column) {
        if (cell(column, 0) == Rgb(0x070A26)) ++navyCells;
        const auto low = cell(column, 60);
        if (low.Red >= 0x30 && low.Blue >= 0x60) ++duskCells;
    }
    EXPECT(navyCells > 100);
    EXPECT(duskCells > 100);
    EXPECT(cell(128, 19) == Rgb(0xF4EBCF) || cell(128, 19) == Rgb(0xCFC3A3) || cell(128, 19) == Rgb(0xB7AB8E));
    EXPECT(cell(0, Budget::Rows - 1) == Rgb(0x07071A));
    EXPECT(cell(159, Budget::Rows - 1) == Rgb(0x07071A));

    // Twinkle must change something over time, and only colors, never the grid.
    const auto before = hashSky(*first);
    first->Update(5000);
    EXPECT(hashSky(*first) != before);
    EXPECT(hashBytes(first->Cells().data(), first->Cells().size_bytes()) == hashBytes(second->Cells().data(), second->Cells().size_bytes()));
}

struct Totals {
    std::size_t peakParticles = 0;
    std::size_t peakRockets = 0;
    std::size_t peakQuads = 0;
    std::size_t saturatedFrames = 0;
    std::size_t frames = 0;
    bool sawGold = false;
    bool sawMint = false;
    bool sawRocket = false;
};

// Runs the show for a span of simulated time at the given frame pattern and checks every frame.
template <typename Pattern>
Totals runShow(Fireworks& fireworks, double seconds, Pattern nextFrame) {
    Totals totals;
    const auto before = allocations.load();
    double elapsed = 0.0;
    while (elapsed < seconds) {
        const double frame = nextFrame();
        fireworks.Update(frame);
        elapsed += frame;
        ++totals.frames;

        totals.peakParticles = std::max(totals.peakParticles, fireworks.ParticleCount());
        totals.peakRockets = std::max(totals.peakRockets, fireworks.RocketCount());
        totals.peakQuads = std::max(totals.peakQuads, fireworks.Quads().size());
        if (fireworks.ParticleCount() == Budget::MaxParticles) ++totals.saturatedFrames;

        EXPECT(fireworks.ParticleCount() <= Budget::MaxParticles);
        EXPECT(fireworks.RocketCount() <= Budget::MaxRockets);
        EXPECT(fireworks.Quads().size() <= Budget::MaxDynamicQuads);

        for (const auto& quad : fireworks.Quads()) {
            EXPECT(quad.X >= 0 && quad.Y >= 0);
            EXPECT(quad.Width > 0 && quad.Height > 0);
            EXPECT(quad.X + quad.Width <= Budget::ScreenWidth);
            EXPECT(quad.Y + quad.Height <= Budget::ScreenHeight);
            EXPECT(quad.X % Budget::CellSize == 0 && quad.Y % Budget::CellSize == 0);
            EXPECT(quad.Width % Budget::CellSize == 0 && quad.Height % Budget::CellSize == 0);
            totals.sawGold = totals.sawGold || quad.Tint == Rgb(0xFFD23F);
            totals.sawMint = totals.sawMint || quad.Tint == Rgb(0x67E3C8);
            totals.sawRocket = totals.sawRocket || quad.Height == Budget::CellSize * 2;
        }
    }
    EXPECT(allocations.load() == before);
    return totals;
}

void report(const char* name, const Totals& totals) {
    std::printf("%-14s frames=%zu peakParticles=%zu/%zu peakRockets=%zu/%zu peakDynamicQuads=%zu/%zu peakTotalQuads=%zu/%zu saturatedFrames=%zu\n",
        name, totals.frames, totals.peakParticles, Budget::MaxParticles, totals.peakRockets, Budget::MaxRockets,
        totals.peakQuads, Budget::MaxDynamicQuads, Budget::SkyCells + totals.peakQuads, Budget::MaxQuads, totals.saturatedFrames);
}

void checkShow() {
    constexpr double Minutes = 60.0 * 30.0;

    auto show = std::make_unique<Fireworks>();
    const auto at120 = runShow(*show, Minutes, [] { return 1.0 / 120.0; });
    report("120Hz", at120);
    // The finale must actually reach the budget and be clipped by it, or the budget proves nothing.
    EXPECT(at120.peakParticles >= Budget::MaxParticles * 9 / 10);
    EXPECT(show->DroppedParticles() > 0);
    std::printf("dropped sparks over 30 simulated minutes: %llu\n", static_cast<unsigned long long>(show->DroppedParticles()));
    EXPECT(at120.sawGold);
    EXPECT(at120.sawMint);
    EXPECT(at120.sawRocket);

    auto sixty = std::make_unique<Fireworks>();
    report("60Hz", runShow(*sixty, Minutes, [] { return 1.0 / 60.0; }));

    // Frame times that wander and occasionally stall hard, including zero and negative deltas.
    auto jitter = std::make_unique<Fireworks>();
    std::uint32_t state = 12345;
    report("jitter", runShow(*jitter, Minutes, [&state] {
        state = state * 1664525u + 1013904223u;
        const auto roll = (state >> 16) % 1000u;
        if (roll == 0) return 2.5;
        if (roll == 1) return 0.0;
        if (roll == 2) return -1.0;
        return 0.004 + static_cast<double>(roll) / 1000.0 * 0.03;
    }));

    // Same inputs, same show.
    auto first = std::make_unique<Fireworks>();
    auto second = std::make_unique<Fireworks>();
    for (int index = 0; index < 20000; ++index) {
        first->Update(1.0 / 120.0);
        second->Update(1.0 / 120.0);
        EXPECT(first->Quads().size() == second->Quads().size());
        EXPECT(first->Quads().size_bytes() == second->Quads().size_bytes());
        if (hashBytes(first->Quads().data(), first->Quads().size_bytes()) != hashBytes(second->Quads().data(), second->Quads().size_bytes())) {
            ++failures;
            std::printf("FAIL determinism at frame %d\n", index);
            break;
        }
    }
}

}

void* operator new(std::size_t bytes) {
    ++allocations;
    if (void* pointer = std::malloc(bytes)) return pointer;
    throw std::bad_alloc{};
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }

int main() {
    checkSky();
    checkShow();
    std::printf("%s (%d failures)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
