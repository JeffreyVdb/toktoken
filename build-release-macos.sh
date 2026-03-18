#!/bin/bash
# =============================================================================
# TokToken macOS Release Build Script
# =============================================================================
#
# Build TokToken on macOS (nativo). Eseguire su macchina macOS o CI runner.
# macOS non supporta -static; il binario dipende solo da libSystem.B.dylib.
#
# Usage:
#   ./build-release-macos.sh            # Build per l'architettura corrente
#   ./build-release-macos.sh --test     # Run tests first
#   ./build-release-macos.sh --clean    # Clean rebuild
#   -j N                                # Parallel jobs
#
# Output:
#   dist/toktoken-macos-<arch>         # x86_64 o aarch64
#   dist/SHA256SUMS
#
# All intermediate build artifacts go under build/ (gitignored).
# Final release binaries go to dist/ (gitignored).
#
# =============================================================================

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

RUN_TESTS=false
CLEAN_BUILD=false
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

while [[ $# -gt 0 ]]; do
    case $1 in
        --test|-t)
            RUN_TESTS=true
            shift
            ;;
        --clean|-c)
            CLEAN_BUILD=true
            shift
            ;;
        -j)
            if [[ -n "${2:-}" && "$2" =~ ^[0-9]+$ ]]; then
                JOBS="$2"
                shift 2
            else
                echo -e "${RED}Error: -j requires a number${NC}"
                exit 1
            fi
            ;;
        -j*)
            JOBS="${1#-j}"
            if [[ ! "$JOBS" =~ ^[0-9]+$ ]]; then
                echo -e "${RED}Error: -j requires a valid number${NC}"
                exit 1
            fi
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --test, -t     Run tests before building"
            echo "  --clean, -c    Clean rebuild"
            echo "  -j N           Parallel jobs (default: auto)"
            echo "  --help, -h     Show this help"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Detect architecture: arm64 -> aarch64 for naming consistency
ARCH=$(uname -m)
if [ "$ARCH" = "arm64" ]; then
    ARCH="aarch64"
fi
BINARY_NAME="toktoken-macos-${ARCH}"

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  TokToken macOS Release Build${NC}"
echo -e "${BLUE}================================================${NC}"
echo -e "  Arch: ${ARCH} | Jobs: ${JOBS} | Clean: ${CLEAN_BUILD} | Tests: ${RUN_TESTS}"
echo ""

if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${YELLOW}Cleaning build directories...${NC}"
    rm -rf build/debug build/release
    echo -e "${GREEN}Clean complete${NC}"
    echo ""
fi

# Tests (Debug build)
if [ "$RUN_TESTS" = true ]; then
    echo -e "${YELLOW}[1/3] Building and running tests...${NC}"
    cmake -B build/debug -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug > /dev/null 2>&1
    cmake --build build/debug -j"${JOBS}" > /dev/null 2>&1
    cd build/debug && ctest --output-on-failure && cd ../..
    echo -e "${GREEN}All tests passed!${NC}"
    echo ""
else
    echo -e "${YELLOW}[1/3] Skipping tests (use --test to run them)${NC}"
fi

# Release build (no -static on macOS)
echo -e "${YELLOW}[2/3] Building release binary (-j${JOBS})...${NC}"
cmake -B build/release \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF > /dev/null 2>&1
cmake --build build/release -j"${JOBS}" > /dev/null 2>&1
echo -e "${GREEN}Build complete${NC}"

# Copy to dist
echo -e "${YELLOW}[3/3] Preparing dist/...${NC}"
mkdir -p dist
cp build/release/toktoken "dist/${BINARY_NAME}"
chmod +x "dist/${BINARY_NAME}"

# SHA256SUMS (macOS uses shasum, not sha256sum)
cd dist && shasum -a 256 toktoken-* > SHA256SUMS && cd ..
echo -e "${GREEN}Binary copied to dist/${BINARY_NAME}${NC}"

# Cleanup
rm -rf build/release

# Verify
echo ""
echo -e "${BLUE}================================================${NC}"
echo -e "${GREEN}  Build Complete!${NC}"
echo -e "${BLUE}================================================${NC}"
echo ""

size=$(ls -lh "dist/${BINARY_NAME}" | awk '{print $5}')
echo "  ${BINARY_NAME} (${size})"
echo ""

# Check dependencies with otool
echo "Dependencies (otool -L):"
if command -v otool > /dev/null 2>&1; then
    otool -L "dist/${BINARY_NAME}" | tail -n +2
    echo ""
    # Verify only libSystem.B.dylib
    deps=$(otool -L "dist/${BINARY_NAME}" | tail -n +2 | grep -cv "libSystem.B.dylib" || true)
    if [ "$deps" -eq 0 ]; then
        echo -e "${GREEN}OK: only libSystem.B.dylib dependency${NC}"
    else
        echo -e "${RED}WARNING: unexpected dependencies found${NC}"
        exit 1
    fi
else
    echo "  (otool not available on this platform)"
fi
echo ""
