#!/usr/bin/env python3
"""
provision_nvs.py
Write WiFi credentials and agent IP to ESP32 NVS flash.
No credentials are stored in source code.

Usage:
    python3 scripts/provision_nvs.py \
        --port /dev/ttyUSB0 \
        --ssid "MyNetwork" \
        --password "MyPassword" \
        --agent-ip "192.168.1.100"

Requirements:
    pip install esptool nvs-partition-gen

The script:
  1. Generates a binary NVS partition from the provided values
  2. Flashes it to the ESP32 NVS partition address (0x9000)
"""

import argparse
import os
import sys
import tempfile
import subprocess
import csv

NVS_PARTITION_ADDRESS = "0x9000"
NVS_PARTITION_SIZE    = "0x6000"  # 24 KB — matches default partition table
NVS_NAMESPACE         = "sbr_cfg"

def generate_nvs_csv(ssid: str, password: str, agent_ip: str,
                     csv_path: str) -> None:
    """Write an NVS CSV file understood by nvs_partition_gen."""
    rows = [
        ["key",        "type",   "encoding", "value"],
        ["sbr_cfg",    "namespace", "",      ""],
        ["ssid",       "data",   "string",   ssid],
        ["password",   "data",   "string",   password],
        ["agent_ip",   "data",   "string",   agent_ip],
    ]
    with open(csv_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerows(rows)
    print(f"[provision] NVS CSV written: {csv_path}")


def generate_nvs_binary(csv_path: str, bin_path: str) -> bool:
    """Run nvs_partition_gen to produce a flashable .bin."""
    try:
        result = subprocess.run(
            [
                "python3", "-m", "nvs_partition_gen", "generate",
                csv_path, bin_path, NVS_PARTITION_SIZE,
            ],
            capture_output=True, text=True, check=True
        )
        print(f"[provision] NVS binary generated: {bin_path}")
        return True
    except subprocess.CalledProcessError as e:
        print(f"[provision] ERROR generating NVS binary:\n{e.stderr}")
        return False
    except FileNotFoundError:
        print("[provision] ERROR: nvs_partition_gen not found.")
        print("           Install: pip install nvs-partition-gen")
        return False


def flash_nvs(port: str, bin_path: str) -> bool:
    """Flash the NVS binary to the ESP32 at the NVS partition address."""
    try:
        result = subprocess.run(
            [
                "python3", "-m", "esptool",
                "--chip", "esp32",
                "--port", port,
                "--baud", "921600",
                "write_flash",
                NVS_PARTITION_ADDRESS, bin_path,
            ],
            capture_output=True, text=True, check=True
        )
        print(f"[provision] NVS flashed to {port} @ {NVS_PARTITION_ADDRESS}")
        print(result.stdout)
        return True
    except subprocess.CalledProcessError as e:
        print(f"[provision] ERROR flashing:\n{e.stderr}")
        return False
    except FileNotFoundError:
        print("[provision] ERROR: esptool not found.")
        print("           Install: pip install esptool")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Provision WiFi credentials to ESP32 NVS flash"
    )
    parser.add_argument("--port",     required=True,
                        help="Serial port, e.g. /dev/ttyUSB0 or COM3")
    parser.add_argument("--ssid",     required=True,
                        help="WiFi SSID")
    parser.add_argument("--password", required=True,
                        help="WiFi password")
    parser.add_argument("--agent-ip", required=True,
                        help="Raspberry Pi 5 IP address for microROS agent")
    args = parser.parse_args()

    print(f"[provision] Target port  : {args.port}")
    print(f"[provision] WiFi SSID    : {args.ssid}")
    print(f"[provision] Agent IP     : {args.agent_ip}")
    print(f"[provision] Password     : {'*' * len(args.password)}")
    print()

    with tempfile.TemporaryDirectory() as tmpdir:
        csv_path = os.path.join(tmpdir, "nvs_data.csv")
        bin_path = os.path.join(tmpdir, "nvs_data.bin")

        generate_nvs_csv(args.ssid, args.password, args.agent_ip, csv_path)

        if not generate_nvs_binary(csv_path, bin_path):
            sys.exit(1)

        if not flash_nvs(args.port, bin_path):
            sys.exit(1)

    print("\n[provision] Done. ESP32 NVS credentials written successfully.")
    print("[provision] Reboot the ESP32 to apply changes.")


if __name__ == "__main__":
    main()
