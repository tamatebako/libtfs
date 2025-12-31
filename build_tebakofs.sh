#!/bin/bash
# Build script for tebakofs CLI tool
# Usage: ./build_tebakofs.sh [clean]

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== tebakofs Build Script ===${NC}"

# Check if clean build requested
if [ "$1" = "clean" ]; then
    echo -e "${YELLOW}Cleaning previous build...${NC}"
    rm -rf build
    echo -e "${GREEN}✓ Clean complete${NC}"
fi

# Check vcpkg environment
if [ -z "$VCPKG_ROOT" ]; then
    echo -e "${RED}ERROR: VCPKG_ROOT not set${NC}"
    echo "Please set VCPKG_ROOT to your vcpkg installation directory"
    echo "Example: export VCPKG_ROOT=/path/to/vcpkg"
    exit 1
fi

if [ ! -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]; then
    echo -e "${RED}ERROR: vcpkg toolchain file not found${NC}"
    echo "Please ensure VCPKG_ROOT points to a valid vcpkg installation"
    exit 1
fi

echo -e "${GREEN}✓ vcpkg found at: $VCPKG_ROOT${NC}"

# Create build directory
mkdir -p build
cd build

# Configure
echo -e "${YELLOW}Configuring with CMake...${NC}"
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DWITH_TESTS=ON

echo -e "${GREEN}✓ Configuration complete${NC}"

# Build tebakofs
echo -e "${YELLOW}Building tebakofs...${NC}"
cmake --build . --target tebakofs -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo -e "${GREEN}✓ Build complete${NC}"

# Check if binary exists
if [ -f "tebakofs" ]; then
    echo -e "${GREEN}=== Build Successful ===${NC}"
    echo -e "Binary location: ${GREEN}$(pwd)/tebakofs${NC}"
    echo ""
    echo -e "${YELLOW}To run tebakofs:${NC}"
    echo "  cd build"
    echo "  ./tebakofs help"
    echo ""
    echo -e "${YELLOW}Or install system-wide:${NC}"
    echo "  sudo cmake --install . --component tebakofs"
else
    echo -e "${RED}ERROR: tebakofs binary not found${NC}"
    exit 1
fi