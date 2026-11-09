#include "Application.hpp"
#include "Graphics/Display.hpp"
#include "Graphics/Renderer.hpp"
#include "Simulation/Budget.hpp"
#include "Simulation/Fireworks.hpp"
#include "Simulation/NightSky.hpp"
#include "Error.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceAgc/libSceAgc.hpp"
#include <algorithm>
#include <memory>

namespace {

constexpr std::uint64_t ReportMicroseconds = 5000000;

// Hand-rolled so the frame loop stays free of libc formatting and any allocation it might do.
char* appendText(char* cursor, const char* text) {
    while (*text != '\0') *cursor++ = *text++;
    return cursor;
}

char* appendNumber(char* cursor, std::uint64_t value) {
    char digits[20];
    int count = 0;
    do {
        digits[count++] = static_cast<char>('0' + value % 10);
        value /= 10;
    } while (value != 0);
    while (count > 0) *cursor++ = digits[--count];
    return cursor;
}

// The renderer test result: how many quads this window peaked at, and the average frame time it held.
void reportThroughput(std::uint64_t frames, std::uint64_t elapsed, std::size_t peakQuads) {
    char line[128];
    char* cursor = appendText(line, "PixelFireworks: peak quads ");
    cursor = appendNumber(cursor, peakQuads);
    cursor = appendText(cursor, "/");
    cursor = appendNumber(cursor, Budget::MaxQuads);
    cursor = appendText(cursor, ", avg frame ");
    cursor = appendNumber(cursor, frames == 0 ? 0 : elapsed / frames);
    cursor = appendText(cursor, " us, frames ");
    cursor = appendNumber(cursor, frames);
    cursor = appendText(cursor, "\n");
    *cursor = '\0';
    // Diagnostics only: a failed debug print must not stop the demo.
    static_cast<void>(sceKernelDebugOutText(0, line));
}

}

void Application::Run() {
    constexpr int Width = Budget::ScreenWidth;
    constexpr int Height = Budget::ScreenHeight;

    Display display(Width, Height);
    // Both live on the heap for good: they are far larger than a stack frame should be.
    const auto sky = std::make_unique<NightSky>();
    const auto fireworks = std::make_unique<Fireworks>();
    Renderer renderer(Width, Height, sky->Cells());

    auto previous = sceKernelGetProcessTime();
    auto reportStart = previous;
    std::uint64_t reportFrames = 0;
    std::size_t reportPeak = 0;

    for (;;) {
        const auto now = sceKernelGetProcessTime();
        fireworks->Update(static_cast<double>(now - previous) / 1000000.0);
        previous = now;
        sky->Update(fireworks->Steps());

        renderer.Draw(display, sky->Stars(), fireworks->Quads());

        ++reportFrames;
        reportPeak = std::max(reportPeak, Budget::SkyCells + fireworks->Quads().size());
        if (now - reportStart >= ReportMicroseconds) {
            reportThroughput(reportFrames, now - reportStart, reportPeak);
            reportStart = now;
            reportFrames = 0;
            reportPeak = 0;
        }

        const auto beforeSuspend = sceKernelGetProcessTime();
        Check(sceAgcSuspendPoint(), "sceAgcSuspendPoint");
        const auto suspended = sceKernelGetProcessTime() - beforeSuspend;
        previous += suspended;
        reportStart += suspended;
    }
}
