#!/usr/bin/env bash
# setup_pi5.sh
# One-shot setup for Raspberry Pi 5 — installs ROS2 Humble,
# colcon, microROS agent, and project dependencies.
#
# Tested on: Ubuntu 24.04 LTS (arm64) on Raspberry Pi 5
# Run as normal user (not root). Uses sudo internally.
#
# Usage:
#   chmod +x scripts/setup_pi5.sh
#   ./scripts/setup_pi5.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=============================================="
echo "  MicroROS SBR — Raspberry Pi 5 Setup"
echo "=============================================="
echo "Repo: $REPO_ROOT"
echo ""

# ── 1. System update ──────────────────────────────────────────────
echo "[1/8] Updating system packages..."
sudo apt-get update -qq
sudo apt-get upgrade -y -qq

# ── 2. ROS2 Humble ────────────────────────────────────────────────
echo "[2/8] Installing ROS2 Humble..."
if ! command -v ros2 &>/dev/null; then
    sudo apt-get install -y software-properties-common curl
    sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
        -o /usr/share/keyrings/ros-archive-keyring.gpg
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] \
http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" \
        | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null
    sudo apt-get update -qq
    sudo apt-get install -y ros-humble-desktop
    echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc
    echo "  ROS2 Humble installed."
else
    echo "  ROS2 Humble already installed — skipping."
fi

source /opt/ros/humble/setup.bash

# ── 3. ROS2 tools ─────────────────────────────────────────────────
echo "[3/8] Installing ROS2 build tools and dependencies..."
sudo apt-get install -y \
    python3-colcon-common-extensions \
    python3-rosdep \
    python3-vcstool \
    python3-pip \
    ros-humble-robot-localization \
    ros-humble-twist-mux \
    ros-humble-teleop-twist-keyboard \
    ros-humble-tf2-tools \
    ros-humble-rqt \
    ros-humble-rqt-common-plugins

# ── 4. rosdep ─────────────────────────────────────────────────────
echo "[4/8] Initialising rosdep..."
sudo rosdep init 2>/dev/null || true
rosdep update

# ── 5. microROS agent via Docker ──────────────────────────────────
echo "[5/8] Installing Docker for microROS agent..."
if ! command -v docker &>/dev/null; then
    curl -fsSL https://get.docker.com | sudo sh
    sudo usermod -aG docker "$USER"
    echo "  Docker installed. You may need to log out and back in."
else
    echo "  Docker already installed — skipping."
fi

# Pull microROS agent image
docker pull microros/micro-ros-agent:humble
echo "  microROS agent image pulled."

# ── 6. Build ROS2 workspace ───────────────────────────────────────
echo "[6/8] Building ros2_ws..."
cd "$REPO_ROOT/ros2_ws"
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release
echo "  ros2_ws built."

# ── 7. PlatformIO CLI (optional, for firmware builds on Pi) ───────
echo "[7/8] Installing PlatformIO CLI..."
pip install platformio --quiet
echo "  PlatformIO installed."

# ── 8. Shell environment ──────────────────────────────────────────
echo "[8/8] Configuring shell environment..."
ROS2_WS_SETUP="$REPO_ROOT/ros2_ws/install/setup.bash"
if ! grep -q "$ROS2_WS_SETUP" ~/.bashrc; then
    echo "source $ROS2_WS_SETUP" >> ~/.bashrc
    echo "export ROS_DOMAIN_ID=0" >> ~/.bashrc
fi

echo ""
echo "=============================================="
echo "  Setup complete!"
echo "=============================================="
echo ""
echo "Next steps:"
echo "  1. source ~/.bashrc   (or open a new terminal)"
echo "  2. Provision ESP32:   cd firmware/esp32 && python3 scripts/provision_nvs.py ..."
echo "  3. Flash STM32:       cd firmware/stm32 && pio run -e stm32f429 -t upload"
echo "  4. Start agent:       docker compose -f docker/docker-compose.yml up agent"
echo "  5. Launch robot:      ros2 launch sbr_bringup robot.launch.py"
echo "  6. Check topics:      python3 scripts/check_topics.py"
echo ""
