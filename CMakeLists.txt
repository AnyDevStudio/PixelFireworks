cmake_minimum_required(VERSION 3.22.1)
project(PixelFireworks LANGUAGES C CXX)
find_package(AnyDevKit CONFIG REQUIRED)
adk_add_executable(PixelFireworks
    src/Main.cpp
    src/Application.cpp
    src/Error.cpp
    src/Simulation/Fireworks.cpp
    src/Simulation/NightSky.cpp
    src/Graphics/Display.cpp
    src/Graphics/Memory.cpp
    src/Graphics/Pipeline.cpp
    src/Graphics/Renderer.cpp
)
target_include_directories(PixelFireworks PRIVATE include)
adk_link_libraries(PixelFireworks libc libkernel libSceVideoOut libSceSystemService libSceAgc libSceAgcDriver)
adk_add_shaders(PixelFireworks shaders/Screen.vert HEADER ScreenVertex OPTIONS --descriptor "0,0,storage-buffer,1,0,16")
adk_add_shaders(PixelFireworks shaders/Screen.frag HEADER ScreenFragment COLOR_FORMAT 4)
