# Base-X Game Engine

A VR-first game engine with Dynamic Radix Optimization.

## Features

- Vulkan rendering
- Bullet3 physics
- OpenXR VR support
- ONNX Runtime integration (optional)
- Modular architecture

## Building

1. Install dependencies via vcpkg:
   ```
   cd vcpkg
   vcpkg install
   ```

2. Configure with CMake:
   ```
   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
   ```

3. Build:
   ```
   cmake --build build
   ```

## Options

- `BUILD_ENGINE=ON/OFF`: Build core engine libraries
- `BUILD_DEMOS=ON/OFF`: Build demo applications
- `ENABLE_VR=ON/OFF`: Enable OpenXR VR support

## Usage

Include `BaseX/Engine.h` and use the `basex::Engine` class.