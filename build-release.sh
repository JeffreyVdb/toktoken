#!/bin/bash
# =============================================================================
# TokToken Release Build Script
# =============================================================================
#
# Builds release binaries for Linux and Windows targets.
# macOS builds are CI-only (see build-release-macos.sh).
#
# Usage:
#   ./build-release.sh              # Build all targets
#   ./build-release.sh --test       # Run tests first
#   ./build-release.sh --clean      # Clean rebuild
#   ./build-release.sh --target X   # Build only one target
#   -j N                            # Parallel jobs
#
# Targets: linux-x64, linux-arm64, win-x64
#
# Output:
#   dist/toktoken-linux-x86_64
#   dist/toktoken-linux-aarch64
#   dist/toktoken-win-x86_64.exe
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
SINGLE_TARGET=""
JOBS=$(nproc 2>/dev/null || echo 4)

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
        --target)
            if [[ -n "${2:-}" ]]; then
                SINGLE_TARGET="$2"
                shift 2
            else
                echo -e "${RED}Error: --target requires a value${NC}"
                exit 1
            fi
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
            echo "  --test, -t       Run tests before building"
            echo "  --clean, -c      Clean rebuild from scratch"
            echo "  --target TARGET  Build only one target"
            echo "  -j N             Parallel jobs (default: $(nproc 2>/dev/null || echo auto))"
            echo "  --help, -h       Show this help"
            echo ""
            echo "Targets: linux-x64, linux-arm64, win-x64"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

should_build() {
    [[ -z "$SINGLE_TARGET" || "$SINGLE_TARGET" == "$1" ]]
}

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  TokToken Release Build${NC}"
echo -e "${BLUE}================================================${NC}"
echo -e "  Jobs: ${JOBS} | Clean: ${CLEAN_BUILD} | Tests: ${RUN_TESTS}"
if [[ -n "$SINGLE_TARGET" ]]; then
    echo -e "  Target: ${SINGLE_TARGET}"
fi
echo ""

# Clean
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${YELLOW}Cleaning build directories...${NC}"
    rm -rf build/release-* build/cross-*
    echo -e "${GREEN}Clean complete${NC}"
    echo ""
fi

# Tests (native Debug build)
if [ "$RUN_TESTS" = true ]; then
    echo -e "${YELLOW}[test] Building and running tests...${NC}"
    cmake -B build/debug -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug > /dev/null 2>&1
    cmake --build build/debug -j"${JOBS}" > /dev/null 2>&1
    cd build/debug && ctest --output-on-failure && cd ../..
    echo -e "${GREEN}All tests passed!${NC}"
    echo ""
fi

STEP=0
TOTAL=0
should_build linux-x64   && ((TOTAL++)) || true
should_build linux-arm64 && ((TOTAL++)) || true
should_build win-x64     && ((TOTAL++)) || true

build_target() {
    local name="$1"
    local build_dir="$2"
    local cmake_flags="$3"
    local binary_src="$4"
    local binary_dst="$5"

    ((STEP++)) || true
    echo -e "${YELLOW}[${STEP}/${TOTAL}] Building ${name} (-j${JOBS})...${NC}"
    cmake -B "$build_dir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTS=OFF \
        $cmake_flags > /dev/null 2>&1
    cmake --build "$build_dir" -j"${JOBS}" > /dev/null 2>&1

    mkdir -p dist
    cp "$build_dir/$binary_src" "dist/$binary_dst"
    echo -e "${GREEN}${name} complete${NC}"
}

# Linux x86_64 (native, static)
if should_build linux-x64; then
    build_target "Linux x86_64" "build/release-linux-x64" "-DTT_STATIC=ON" "toktoken" "toktoken-linux-x86_64"
fi

# Linux aarch64 (cross)
if should_build linux-arm64; then
    if command -v aarch64-linux-gnu-gcc > /dev/null 2>&1; then
        build_target "Linux aarch64" "build/cross-linux-arm64" \
            "-DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm64.cmake -DTT_STATIC=ON" \
            "toktoken" "toktoken-linux-aarch64"
    else
        echo -e "${YELLOW}[skip] Linux aarch64: aarch64-linux-gnu-gcc not found${NC}"
    fi
fi

# Windows x86_64 (MinGW cross)
if should_build win-x64; then
    if command -v x86_64-w64-mingw32-gcc > /dev/null 2>&1; then
        build_target "Windows x86_64" "build/cross-win-x64" \
            "-DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake" \
            "toktoken.exe" "toktoken-win-x86_64.exe"
    else
        echo -e "${YELLOW}[skip] Windows x86_64: mingw-w64 not installed${NC}"
    fi
fi

# SHA256SUMS
if ls dist/toktoken-* > /dev/null 2>&1; then
    echo ""
    echo -e "${YELLOW}Generating SHA256SUMS...${NC}"
    cd dist && sha256sum toktoken-* > SHA256SUMS && cd ..
    echo -e "${GREEN}SHA256SUMS generated${NC}"
fi

# Cleanup intermediate build directories
echo ""
echo -e "${YELLOW}Cleaning up build directories...${NC}"
rm -rf build/release-* build/cross-*
echo -e "${GREEN}Cleanup complete${NC}"

# Results
echo ""
echo -e "${BLUE}================================================${NC}"
echo -e "${GREEN}  Build Complete!${NC}"
echo -e "${BLUE}================================================${NC}"
echo ""
echo "Output files:"
for f in dist/toktoken-*; do
    if [[ -f "$f" && "$f" != *SHA256SUMS* ]]; then
        size=$(ls -lh "$f" | awk '{print $5}')
        format=$(file -b "$f" | head -c 60)
        echo "  $(basename "$f") ($size) - $format"
    fi
done
echo ""

# Verify binary formats
echo "Verification:"
FAIL=0
verify() {
    local file="$1"
    local expect="$2"
    local label="$3"
    if [[ ! -f "$file" ]]; then return; fi
    echo -n "  ${label}: "
    if file "$file" | grep -q "$expect"; then
        echo -e "${GREEN}OK${NC}"
    else
        echo -e "${RED}FAIL (expected: $expect)${NC}"
        FAIL=1
    fi
}
verify "dist/toktoken-linux-x86_64" "ELF 64-bit" "linux-x86_64"
verify "dist/toktoken-linux-aarch64" "ELF 64-bit" "linux-aarch64"
verify "dist/toktoken-win-x86_64.exe" "PE32+" "win-x86_64"
echo ""

if [[ "$FAIL" -ne 0 ]]; then
    echo -e "${RED}Some verifications failed!${NC}"
    exit 1
fi
