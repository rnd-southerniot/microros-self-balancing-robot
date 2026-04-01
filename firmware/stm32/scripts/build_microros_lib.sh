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
#   lib/microros/include/

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$PROJECT_DIR/lib/microros"

echo "=============================================="
echo "  Building microROS static library"
echo "  Target: STM32F429 (Cortex-M4F, hard float)"
echo "  Output: $OUTPUT_DIR"
echo "=============================================="

# Create build workspace with expected directory structure
BUILD_DIR=$(mktemp -d)
trap "rm -rf $BUILD_DIR" EXIT

LIBGEN="$BUILD_DIR/microros_static_library/library_generation"
mkdir -p "$LIBGEN"

# ── Toolchain file for Cortex-M4 with FPU ──
cat > "$LIBGEN/toolchain.cmake" << 'TOOLCHAIN'
SET(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_CROSSCOMPILING 1)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER $ENV{TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER $ENV{TOOLCHAIN_PREFIX}g++)

SET(CMAKE_C_COMPILER_WORKS 1 CACHE INTERNAL "")
SET(CMAKE_CXX_COMPILER_WORKS 1 CACHE INTERNAL "")

set(FLAGS $ENV{RET_CFLAGS} CACHE STRING "" FORCE)
set(MICROROSFLAGS "-DCLOCK_MONOTONIC=0 -D'__attribute__(x)='" CACHE STRING "" FORCE)

set(CMAKE_C_FLAGS_INIT "-std=c11 ${FLAGS} ${MICROROSFLAGS} " CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_INIT "-std=c++14 ${FLAGS} -fno-rtti ${MICROROSFLAGS} " CACHE STRING "" FORCE)

set(__BIG_ENDIAN__ 0)
TOOLCHAIN

# ── colcon.meta: configure microROS packages ──
cat > "$LIBGEN/colcon.meta" << 'META'
{
    "names": {
        "tracetools": {
            "cmake-args": ["-DTRACETOOLS_DISABLED=ON", "-DTRACETOOLS_STATUS_CHECKING_TOOL=OFF"]
        },
        "rosidl_typesupport": {
            "cmake-args": ["-DROSIDL_TYPESUPPORT_SINGLE_TYPESUPPORT=ON"]
        },
        "rcl": {
            "cmake-args": ["-DBUILD_TESTING=OFF", "-DRCL_COMMAND_LINE_ENABLED=OFF", "-DRCL_LOGGING_ENABLED=OFF"]
        },
        "rcutils": {
            "cmake-args": ["-DENABLE_TESTING=OFF", "-DRCUTILS_NO_FILESYSTEM=ON", "-DRCUTILS_NO_THREAD_SUPPORT=ON", "-DRCUTILS_NO_64_ATOMIC=ON", "-DRCUTILS_AVOID_DYNAMIC_ALLOCATION=ON"]
        },
        "microxrcedds_client": {
            "cmake-args": ["-DUCLIENT_PIC=OFF", "-DUCLIENT_PROFILE_UDP=OFF", "-DUCLIENT_PROFILE_TCP=OFF", "-DUCLIENT_PROFILE_DISCOVERY=OFF", "-DUCLIENT_PROFILE_SERIAL=OFF", "-UCLIENT_PROFILE_STREAM_FRAMING=ON", "-DUCLIENT_PROFILE_CUSTOM_TRANSPORT=ON"]
        },
        "rmw_microxrcedds": {
            "cmake-args": ["-DRMW_UXRCE_MAX_NODES=1", "-DRMW_UXRCE_MAX_PUBLISHERS=10", "-DRMW_UXRCE_MAX_SUBSCRIPTIONS=5", "-DRMW_UXRCE_MAX_SERVICES=1", "-DRMW_UXRCE_MAX_CLIENTS=1", "-DRMW_UXRCE_MAX_HISTORY=4", "-DRMW_UXRCE_TRANSPORT=custom"]
        }
    }
}
META

# ── Extra packages (empty — using standard message types) ──
mkdir -p "$LIBGEN/extra_packages"
cat > "$LIBGEN/extra_packages/extra_packages.repos" << 'REPOS'
repositories:
REPOS

# ── library_generation.sh — the script Docker's entrypoint calls ──
cat > "$LIBGEN/library_generation.sh" << 'GENSCRIPT'
#!/bin/bash
set -e

export BASE_PATH=/project/$MICROROS_LIBRARY_FOLDER

######## Init ########
apt-get update -qq
apt-get install -y -qq gcc-arm-none-eabi > /dev/null

cd /uros_ws
source /opt/ros/$ROS_DISTRO/setup.bash
source install/local_setup.bash

ros2 run micro_ros_setup create_firmware_ws.sh generate_lib

######## Adding extra packages ########
pushd firmware/mcu_ws > /dev/null

    # tf2_msgs
    git clone -b jazzy https://github.com/ros2/geometry2 2>/dev/null || true
    if [ -d geometry2/tf2_msgs ]; then
        cp -R geometry2/tf2_msgs ros2/tf2_msgs
        rm -rf geometry2
    fi

    # User extra packages
    mkdir -p extra_packages
    pushd extra_packages > /dev/null
        cp -R $BASE_PATH/library_generation/extra_packages/* . 2>/dev/null || true
        if [ -f extra_packages.repos ]; then
            vcs import --input extra_packages.repos 2>/dev/null || true
        fi
    popd > /dev/null

popd > /dev/null

######## Build with hardcoded CFLAGS for Cortex-M4F ########
export RET_CFLAGS="-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -ffunction-sections -fdata-sections -fno-exceptions -nostdlib -DCLOCK_MONOTONIC=0"
export TOOLCHAIN_PREFIX=/usr/bin/arm-none-eabi-

echo "Building with CFLAGS: $RET_CFLAGS"
ros2 run micro_ros_setup build_firmware.sh \
    $BASE_PATH/library_generation/toolchain.cmake \
    $BASE_PATH/library_generation/colcon.meta

######## Collect output ########
find firmware/build/include/ -name "*.c" -delete
rm -rf $BASE_PATH/libmicroros
mkdir -p $BASE_PATH/libmicroros/include
cp -R firmware/build/include/* $BASE_PATH/libmicroros/include/
cp firmware/build/libmicroros.a $BASE_PATH/libmicroros/libmicroros.a

######## Fix nested include paths ########
pushd firmware/mcu_ws > /dev/null
    INCLUDE_ROS2_PACKAGES=$(colcon list | awk '{print $1}' | awk -v d=" " '{s=(NR==1?s:s d)$0}END{print s}')
popd > /dev/null

for var in ${INCLUDE_ROS2_PACKAGES}; do
    if [ -d "$BASE_PATH/libmicroros/include/${var}/${var}" ]; then
        rsync -r $BASE_PATH/libmicroros/include/${var}/${var}/* $BASE_PATH/libmicroros/include/${var}
        rm -rf $BASE_PATH/libmicroros/include/${var}/${var}
    fi
done

######## Fix permissions ########
chmod -R 777 $BASE_PATH/libmicroros/

echo "===== microROS static library build complete ====="
ls -lh $BASE_PATH/libmicroros/libmicroros.a
GENSCRIPT
chmod +x "$LIBGEN/library_generation.sh"

# ── Dummy Makefile so Docker doesn't complain ──
cat > "$BUILD_DIR/Makefile" << 'MAKEFILE'
print_cflags:
	@echo "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -ffunction-sections -fdata-sections -fno-exceptions -nostdlib"
MAKEFILE

echo "[1/3] Docker image already pulled"
echo "[2/3] Building static library (this takes ~10 minutes)..."

docker run --rm \
    -v "$BUILD_DIR":/project \
    --env MICROROS_LIBRARY_FOLDER=microros_static_library \
    microros/micro_ros_static_library_builder:jazzy

echo "[3/3] Copying output..."
mkdir -p "$OUTPUT_DIR"

LIB_FILE="$BUILD_DIR/microros_static_library/libmicroros/libmicroros.a"
INC_DIR="$BUILD_DIR/microros_static_library/libmicroros/include"

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
    echo "ERROR: libmicroros.a not found!"
    find "$BUILD_DIR" -name "libmicroros.a" 2>/dev/null
    exit 1
fi
