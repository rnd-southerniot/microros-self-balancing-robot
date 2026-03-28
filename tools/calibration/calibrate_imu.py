#!/usr/bin/env python3
"""
calibrate_imu.py
Collect static IMU samples from ICM-20948 via ROS2 topic and compute
gyro bias offsets. Robot must be stationary and level during calibration.

Usage:
    # On Raspberry Pi 5 with ROS2 sourced:
    ros2 run sbr_bringup calibrate_imu.py

    # Or directly:
    python3 tools/calibration/calibrate_imu.py --samples 2000

Output:
    Prints calibration values to console.
    Saves to tools/calibration/imu_calib.yaml for reference.
    These values should be flashed into STM32 NVS or robot_params.h.
"""

import argparse
import math
import sys
import time
import yaml
from pathlib import Path

try:
    import rclpy
    from rclpy.node import Node
    from sensor_msgs.msg import Imu
    ROS2_AVAILABLE = True
except ImportError:
    ROS2_AVAILABLE = False
    print("[calibrate_imu] Warning: rclpy not available — using mock data")


class ImuCalibrator(Node if ROS2_AVAILABLE else object):

    def __init__(self, n_samples: int):
        if ROS2_AVAILABLE:
            super().__init__("imu_calibrator")

        self.n_samples   = n_samples
        self.samples     = []
        self.done        = False

        if ROS2_AVAILABLE:
            self.sub = self.create_subscription(
                Imu, "/imu/data", self._imu_callback, 100)
            self.get_logger().info(
                f"Collecting {n_samples} samples. Keep robot STILL and LEVEL.")
        else:
            print(f"[calibrate_imu] Mock mode — collecting {n_samples} samples")

    def _imu_callback(self, msg: "Imu"):
        if self.done:
            return
        self.samples.append({
            "gx": msg.angular_velocity.x,
            "gy": msg.angular_velocity.y,
            "gz": msg.angular_velocity.z,
            "ax": msg.linear_acceleration.x,
            "ay": msg.linear_acceleration.y,
            "az": msg.linear_acceleration.z,
        })
        count = len(self.samples)
        if count % 100 == 0:
            pct = int(count / self.n_samples * 100)
            print(f"  {pct}% ({count}/{self.n_samples} samples)")
        if count >= self.n_samples:
            self.done = True
            if ROS2_AVAILABLE:
                self.destroy_subscription(self.sub)

    def compute(self) -> dict:
        if not self.samples:
            raise RuntimeError("No samples collected")

        n = len(self.samples)
        bias = {
            "gyro_x": sum(s["gx"] for s in self.samples) / n,
            "gyro_y": sum(s["gy"] for s in self.samples) / n,
            "gyro_z": sum(s["gz"] for s in self.samples) / n,
            "accel_x": sum(s["ax"] for s in self.samples) / n,
            "accel_y": sum(s["ay"] for s in self.samples) / n,
            # accel_z should be ~9.81 m/s² at rest — offset from 9.81
            "accel_z": sum(s["az"] for s in self.samples) / n - 9.81,
        }

        # Compute noise standard deviation
        def std(key, bias_val):
            vals = [s[key] for s in self.samples]
            variance = sum((v - bias_val)**2 for v in vals) / n
            return math.sqrt(variance)

        noise = {
            "gyro_x_std": std("gx", bias["gyro_x"]),
            "gyro_y_std": std("gy", bias["gyro_y"]),
            "gyro_z_std": std("gz", bias["gyro_z"]),
        }

        return {"bias": bias, "noise": noise, "n_samples": n}


def main():
    parser = argparse.ArgumentParser(
        description="IMU calibration — collect gyro/accel bias at rest"
    )
    parser.add_argument("--samples", type=int, default=2000,
                        help="Number of samples (default: 2000 @ 500 Hz = 4 s)")
    parser.add_argument("--output", type=str,
                        default="tools/calibration/imu_calib.yaml",
                        help="Output YAML file path")
    args = parser.parse_args()

    print("=" * 55)
    print("IMU Calibration — ICM-20948")
    print("=" * 55)
    print("Place robot on flat surface. DO NOT MOVE during calibration.")
    print("Starting in 3 seconds...")
    time.sleep(3)

    if ROS2_AVAILABLE:
        rclpy.init()
        calib = ImuCalibrator(args.samples)

        print(f"\nCollecting {args.samples} samples...")
        while rclpy.ok() and not calib.done:
            rclpy.spin_once(calib, timeout_sec=0.01)

        result = calib.compute()
        rclpy.shutdown()
    else:
        print("ROS2 not available — cannot collect live samples.")
        print("Run this script on the Raspberry Pi 5 with ROS2 sourced.")
        sys.exit(1)

    # ── Print results ─────────────────────────────────────────────
    print("\n=== Calibration Results ===")
    print(f"Samples collected: {result['n_samples']}")
    print("\nGyro bias (deg/s) — add negation of these to driver:")
    for axis in ("x", "y", "z"):
        print(f"  gyro_{axis}: {result['bias'][f'gyro_{axis}']:+.4f} deg/s  "
              f"(std: {result['noise'][f'gyro_{axis}_std']:.4f})")
    print("\nAccel bias (m/s²):")
    for axis in ("x", "y", "z"):
        print(f"  accel_{axis}: {result['bias'][f'accel_{axis}']:+.4f} m/s²")

    # ── Save to YAML ──────────────────────────────────────────────
    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w") as f:
        yaml.dump(result, f, default_flow_style=False)
    print(f"\nSaved to: {out_path}")
    print("\nUpdate robot_params.h ICM20948_GYRO_BIAS_* and reflash STM32.")


if __name__ == "__main__":
    main()
