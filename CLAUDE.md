# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

JSBSim is an open-source, cross-platform Flight Dynamics Model (FDM) written in C++. It simulates aircraft physics under various forces and moments. The project includes bindings for Python, MATLAB, Julia, and Unreal Engine.

## Build Commands

```bash
# Standard build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Build with Python module
cmake -DBUILD_PYTHON_MODULE=ON ..

# Release build with optimizations
cmake -DCMAKE_CXX_FLAGS_RELEASE="-O3 -march=native -mtune=native" -DCMAKE_BUILD_TYPE=Release ..

# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

**Key CMake Options:**
- `SYSTEM_EXPAT`: Use system libExpat instead of bundled (default: OFF)
- `BUILD_SHARED_LIBS`: Build shared libraries (default: ON)
- `BUILD_PYTHON_MODULE`: Build Python module (default: ON)
- `BUILD_DOCS`: Build Doxygen documentation (default: ON)
- `ENABLE_COVERAGE`: Enable code coverage

## Testing

```bash
# Run all tests from build directory
ctest --parallel $(nproc) --output-on-failure

# Run specific test
ctest -R TestName

# If tests fail with library not found error
LD_LIBRARY_PATH=$PWD/src ctest
```

Tests are located in `tests/` (Python tests) and `tests/unit_tests/` (CxxTest C++ tests).

## Running the Simulator

```bash
./build/src/JSBSim scripts/c1721.xml
```

## Architecture

**Core Engine:** `src/FGFDMExec.cpp` - Main simulation executive that runs the flight dynamics model

**Key Directories:**
- `src/models/` - Physics models (atmosphere, propulsion, flight control, aerodynamics, landing gear)
- `src/math/` - Vector/matrix operations, quaternions, coordinate transformations
- `src/input_output/` - XML parsing (Expat), data output (screen, file, socket)
- `src/initialization/` - Initial conditions, trim calculation, script processing

**Data Files (XML):**
- `aircraft/` - Aircraft model definitions
- `engine/` - Engine definitions
- `systems/` - Control system definitions
- `scripts/` - Simulation scripts

**Language Bindings:**
- `python/` - Cython-based Python bindings (`jsbsim.pyx.in`)
- `matlab/` - MATLAB S-Function
- `julia/` - Julia bindings
- `UnrealEngine/` - Unreal Engine plugin

## Code Standards

- C++17 standard
- Platform-independent (Linux, macOS, Windows)
- XML-based configuration files for aircraft, engines, and simulations
- Property system for runtime parameter access
