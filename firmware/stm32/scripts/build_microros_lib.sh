#!/usr/bin/env bash
# build_microros_lib.sh
# Build libmicroros.a static library for STM32F429 (Cortex-M4F).
# Requires Docker. Run on the VM (not the Mac).
#
# Usage:
#   cd firmware/stm32
#   bash scripts/build_microros_lib.sh
#
# Output:
#   lib/microros/libmicroros.a
#   lib/microros/include/  (all microROS headers)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$PROJECT_DIR/lib/microros"

echo "=============================================="
echo "  Building microROS static library"
echo "  Target: STM32F429 (Cortex-M4F, hard float)"
echo "  Output: $OUTPUT_DIR"
echo "=============================================="

# Create build workspace
BUILD_DIR=$(mktemp -d)
trap "rm -rf $BUILD_DIR" EXIT

mkdir -p "$BUILD_DIR/microros_static_library/library_generation"

# ── Toolchain file for Cortex-M4 with FPU ──
cat > "$BUILD_DIR/microros_static_library/library_generation/toolchain.cmake" << 'TOOLCHAIN'
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)

set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -ffunction-sections -fdata-sections -fno-exceptions -nostdlib")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -fno-rtti")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(__BIG_ENDIAN__ 0)
TOOLCHAIN

# ── colcon.meta: configure microROS packages ──
cat > "$BUILD_DIR/microros_static_library/library_generation/colcon.meta" << 'META'
{
    "names": {
        "tracetools": {
            "cmake-args": ["-DTRACETOOLS_DISABLED=ON"]
        },
        "rosidl_typesupport": {
            "cmake-args": ["-DROSIDL_TYPESUPPORT_SINGLE_TYPESUPPORT=ON"]
        },
        "rcl": {
            "cmake-args": ["-DRCL_COMMAND_LINE_ENABLED=OFF", "-DRCL_LOGGING_ENABLED=OFF"]
        },
        "rcutils": {
            "cmake-args": ["-DRCUTILS_NO_FILESYSTEM=ON", "-DRCUTILS_NO_THREAD_SUPPORT=ON", "-DRCUTILS_NO_64_ATOMIC=ON", "-DRCUTILS_AVOID_DYNAMIC_ALLOCATION=ON"]
        },
        "microxrcedds_client": {
            "cmake-args": ["-DUCLIENT_PROFILE_SERIAL=OFF", "-DUCLIENT_PROFILE_UDP=OFF", "-DUCLIENT_PROFILE_TCP=OFF", "-DUCLIENT_PROFILE_CUSTOM_TRANSPORT=ON", "-DUCLIENT_PROFILE_STREAM_FRAMING=ON"]
        },
        "rmw_microxrcedds": {
            "cmake-args": ["-DRMW_UXRCE_MAX_NODES=1", "-DRMW_UXRCE_MAX_PUBLISHERS=5", "-DRMW_UXRCE_MAX_SUBSCRIPTIONS=2", "-DRMW_UXRCE_MAX_SERVICES=1", "-DRMW_UXRCE_MAX_CLIENTS=0", "-DRMW_UXRCE_MAX_HISTORY=4", "-DRMW_UXRCE_TRANSPORT=custom"]
        }
    }
}
META

# ── Extra packages to include (message types) ──
cat > "$BUILD_DIR/microros_static_library/library_generation/extra_packages.repos" << 'REPOS'
repositories:
REPOS

echo "[1/3] Pulling Docker image..."
docker pull microros/micro_ros_static_library_builder:jazzy

echo "[2/3] Building static library (this takes ~10 minutes)..."
docker run --rm \
    -v "$BUILD_DIR":/project \
    --env MICROROS_LIBRARY_FOLDER=microros_static_library \
    microros/micro_ros_static_library_builder:jazzy

echo "[3/3] Copying output..."
mkdir -p "$OUTPUT_DIR"

LIB_FILE="$BUILD_DIR/microros_static_library/libmicroros.a"
INC_DIR="$BUILD_DIR/microros_static_library/include"
# Fallback paths
[ ! -f "$LIB_FILE" ] && LIB_FILE="$BUILD_DIR/libmicroros/libmicroros.a"
[ ! -d "$INC_DIR" ]   && INC_DIR="$BUILD_DIR/libmicroros/include"

if [ -f "$LIB_FILE" ]; then
    cp "$LIB_FILE" "$OUTPUT_DIR/"
    cp -r "$INC_DIR" "$OUTPUT_DIR/"
    echo ""
    echo "=============================================="
    echo "  SUCCESS!"
    echo "  Library: $(ls -lh "$OUTPUT_DIR/libmicroros.a" | awk '{print $5}')"
    echo "  Headers: $OUTPUT_DIR/include/"
    echo "=============================================="
else
    echo "ERROR: libmicroros.a not found in build output!"
    ls -la "$BUILD_DIR/libmicroros/" 2>/dev/null || echo "No libmicroros/ directory"
    exit 1
fi
