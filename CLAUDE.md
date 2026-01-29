# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

JSBSim is an open-source, cross-platform Flight Dynamics Model (FDM) written in C++. It simulates aircraft physics under forces and moments using a 6-DOF nonlinear model. The project includes bindings for Python, MATLAB, Julia, and Unreal Engine.

## Build Commands

```bash
# Standard build
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)   # macOS
make -j$(nproc)               # Linux

# Build with Python module (requires Cython)
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

## Testing

```bash
# Run all tests from build directory
ctest -j$(nproc) --output-on-failure

# Run specific test by name pattern
ctest -R TestDensityAltitude

# Run tests matching pattern
ctest -R Altitude

# If tests fail with "cannot open shared object file" error (Linux)
LD_LIBRARY_PATH=$PWD/src ctest

# View test failure details
cat Testing/Temporary/LastTestsFailed.log
```

Tests require Python with `numpy`, `pandas`, and `scipy`. Python tests are in `tests/`, C++ unit tests (CxxTest) are in `tests/unit_tests/`.

## Running the Simulator

```bash
./build/src/JSBSim --script=scripts/c1721.xml
```

## Architecture

### Execution Model

**FGFDMExec** (`src/FGFDMExec.cpp`) is the simulation executive. Models execute in a strict order to satisfy data dependencies:

1. **Propagate** - State integration (position, velocity, attitude via quaternions)
2. **Input** - Control inputs
3. **Inertial** - Earth/planet reference frame
4. **Atmosphere**, **Winds** - Environmental conditions
5. **Systems (FGFCS)** - Flight control system
6. **MassBalance** - CG and inertia (must run before forces)
7. **Auxiliary** - Derived aerodynamic quantities
8. **Propulsion**, **Aerodynamics**, **GroundReactions**, **ExternalReactions**, **BuoyantForces** - Force/moment producers
9. **Aircraft** - Force/moment aggregation
10. **Accelerations** - Newton's 2nd law (F=ma)
11. **Output** - Data logging

Each model's `Run()` is preceded by `LoadInputs()` which transfers outputs from previous models.

### Model Hierarchy

All physics models inherit from **FGModel** (`src/models/FGModel.h`):
- `InitModel()` - initialization
- `Run(bool Holding)` - execute physics
- `Load(Element* el)` - parse XML configuration

### Property System

**FGPropertyManager** (`src/input_output/FGPropertyManager.h`) provides a hierarchical namespace for runtime parameter access (e.g., `position/h-sl-ft`, `velocities/vc-kts`). Models bind their variables to properties, enabling script-driven control and data output.

### XML Processing

**FGXMLElement** (`src/input_output/FGXMLElement.h`) wraps Expat for XML parsing with built-in unit conversion. Aircraft, engine, and system definitions are all XML-based.

### Key Source Directories

- `src/models/` - Physics models (atmosphere, propulsion, flight control, aerodynamics, ground reactions)
- `src/models/propulsion/` - Engine types (piston, turbine, rocket, electric), tanks, thrusters
- `src/models/flight_control/` - FCS components (filters, gains, switches, PID)
- `src/math/` - Vector/matrix math, quaternions, integrators
- `src/input_output/` - XML parsing, property system, output (file, socket, screen)
- `src/initialization/` - Initial conditions, trim, linearization

### Data Files

- `aircraft/` - Aircraft model XML definitions
- `engine/` - Engine XML definitions
- `systems/` - Flight control system XML definitions
- `scripts/` - Simulation script XML files

## Code Standards

- C++17 standard
- Platform-independent (Linux, macOS, Windows)
- No external dependencies (Expat bundled, GeographicLib bundled)
