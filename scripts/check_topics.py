#!/usr/bin/env python3
"""
check_topics.sh — validate all expected topics are publishing at correct rates.
Run on Raspberry Pi 5 after launching robot.launch.py.

Usage:
    python3 scripts/check_topics.py

Exit code: 0 if all checks pass, 1 if any fail.
"""

import subprocess
import sys
import time

# (topic, min_hz, max_hz)
EXPECTED_TOPICS = [
    ("/imu/data",             80,   600),
    ("/odom",                 40,   60),
    ("/odometry/filtered",    40,   60),
    ("/sonar/range",           5,   60),
    ("/balance_state",        80,   120),
    ("/robot_status",          0.5,  2),
    ("/safety/estopped",      10,   250),
]

GREEN  = "\033[92m"
RED    = "\033[91m"
YELLOW = "\033[93m"
RESET  = "\033[0m"

def get_topic_hz(topic: str, duration: float = 2.0) -> float | None:
    """Sample topic rate for `duration` seconds. Returns Hz or None."""
    try:
        result = subprocess.run(
            ["ros2", "topic", "hz", topic, "--window", "50"],
            capture_output=True, text=True, timeout=duration + 1.0
        )
        for line in result.stdout.splitlines():
            if "average rate:" in line.lower():
                hz = float(line.split(":")[1].strip().split()[0])
                return hz
    except (subprocess.TimeoutExpired, ValueError, IndexError):
        pass
    return None


def main():
    print("=" * 60)
    print("SBR Topic Health Check")
    print("=" * 60)
    print(f"Checking {len(EXPECTED_TOPICS)} topics...\n")

    results = []
    all_pass = True

    for topic, min_hz, max_hz in EXPECTED_TOPICS:
        print(f"  Checking {topic:<35}", end="", flush=True)
        hz = get_topic_hz(topic, duration=2.5)

        if hz is None:
            status = f"{RED}FAIL  — not publishing{RESET}"
            results.append((topic, False, "not publishing"))
            all_pass = False
        elif hz < min_hz:
            status = f"{RED}FAIL  — {hz:.1f} Hz (min {min_hz} Hz){RESET}"
            results.append((topic, False, f"{hz:.1f} Hz < {min_hz} Hz"))
            all_pass = False
        elif hz > max_hz:
            status = f"{YELLOW}WARN  — {hz:.1f} Hz (max {max_hz} Hz){RESET}"
            results.append((topic, False, f"{hz:.1f} Hz > {max_hz} Hz"))
            all_pass = False
        else:
            status = f"{GREEN}PASS  — {hz:.1f} Hz{RESET}"
            results.append((topic, True, f"{hz:.1f} Hz"))

        print(status)

    print("\n" + "=" * 60)
    passed = sum(1 for _, ok, _ in results if ok)
    print(f"Result: {passed}/{len(EXPECTED_TOPICS)} checks passed")

    if all_pass:
        print(f"{GREEN}All topics healthy.{RESET}")
        sys.exit(0)
    else:
        print(f"{RED}Some checks failed — see above.{RESET}")
        sys.exit(1)


if __name__ == "__main__":
    main()
