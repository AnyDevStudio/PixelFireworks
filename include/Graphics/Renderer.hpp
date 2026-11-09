#ifndef PixelFireworks_Graphics_Renderer_hpp
#define PixelFireworks_Graphics_Renderer_hpp
#include "Graphics/Memory.hpp"
#include "Graphics/Display.hpp"
#include "Graphics/Pipeline.hpp"
#include "Simulation/Budget.hpp"
#include <array>
#include <memory>
#include <span>

class Renderer final {
public:
    Renderer(int width, int height, std::span<const Color, Budget::SkyCells> sky);
    ~Renderer() noexcept(false);
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    void Draw(Display& display, std::span<const StarPixel> stars, std::span<const PixelQuad> quads);

private:
    struct alignas(16) Frame {
        std::array<float, 4> rectangle;
        std::array<float, 4> color;
        std::array<float, 4> viewport;
        std::array<std::uint32_t, 4> glyph;
    };
    struct DrawSlot {
        std::uint32_t* Commands = nullptr;
        std::uint64_t* Fence = nullptr;
        Frame* Frames = nullptr;
        std::uint32_t* Descriptor = nullptr;
        ShaderRegister* Contexts = nullptr;
        std::size_t Bytes = 0;
        bool Pending = false;
    };
    std::array<DrawSlot, 3> slots{};
    std::size_t slotIndex = 0;
    bool shaderMemoryPending = true;
    static std::size_t requiredMemory();
    void selectSlot(std::size_t index);
    void waitSlot(DrawSlot& slot);
    Shader* createShader(std::span<const std::uint8_t> header, std::span<const std::uint8_t> code);
    Memory memory;
    std::unique_ptr<Pipeline> pipeline;
    Shader* vertex;
    Shader* fragment;
    std::uint32_t* commands;
    std::uint64_t* fence;
    Frame* frames;
    std::uint32_t* descriptor;
    ShaderRegister* contexts;
};
#endif
