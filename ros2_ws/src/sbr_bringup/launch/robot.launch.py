"""
robot.launch.py
Full bringup for MicroROS self-balancing robot on Raspberry Pi 5.

Starts:
  1. microROS agent  (UDP port 8888 — bridges ESP32 ↔ ROS2)
  2. robot_state_publisher  (URDF → TF tree)
  3. robot_localization EKF  (fuses /imu/data + /odom → /odometry/filtered)
  4. sbr_controller/safety_node  (e-stop watchdog)
  5. sbr_controller/telemetry_node  (health aggregation)
  6. twist_mux  (arbitrates cmd_vel priority)

Usage:
  ros2 launch sbr_bringup robot.launch.py
  ros2 launch sbr_bringup robot.launch.py use_sim_time:=false
"""

import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    TimerAction,
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():

    # ── Directories ────────────────────────────────────────────
    bringup_dir  = get_package_share_directory("sbr_bringup")
    desc_dir     = get_package_share_directory("sbr_description")

    # ── Launch arguments ───────────────────────────────────────
    use_sim_time_arg = DeclareLaunchArgument(
        "use_sim_time",
        default_value="false",
        description="Use simulation (Gazebo) clock",
    )
    use_sim_time = LaunchConfiguration("use_sim_time")

    # ── URDF / robot_state_publisher ───────────────────────────
    urdf_file = os.path.join(desc_dir, "urdf", "balancer.urdf.xacro")

    robot_state_publisher = Node(
        package="robot_state_publisher",
        executable="robot_state_publisher",
        name="robot_state_publisher",
        output="screen",
        parameters=[
            {"use_sim_time": use_sim_time},
            {"robot_description": open(urdf_file).read()
             if os.path.exists(urdf_file) else ""},
        ],
    )

    # ── microROS agent (UDP) ───────────────────────────────────
    # Bridges ESP32 WiFi UDP traffic to the ROS2 graph on Pi 5.
    # Delay 2 s to allow network interfaces to stabilise after boot.
    microros_agent = TimerAction(
        period=2.0,
        actions=[
            Node(
                package="micro_ros_agent",
                executable="micro_ros_agent",
                name="micro_ros_agent",
                arguments=["udp4", "--port", "8888"],
                output="screen",
            )
        ],
    )

    # ── robot_localization EKF ─────────────────────────────────
    ekf_config = os.path.join(bringup_dir, "config", "ekf_params.yaml")

    ekf_node = Node(
        package="robot_localization",
        executable="ekf_node",
        name="ekf_filter_node",
        output="screen",
        parameters=[ekf_config, {"use_sim_time": use_sim_time}],
        remappings=[
            ("odometry/filtered", "/odometry/filtered"),
        ],
    )

    # ── Safety node ────────────────────────────────────────────
    safety_node = Node(
        package="sbr_controller",
        executable="safety_node",
        name="safety_node",
        output="screen",
        parameters=[{"use_sim_time": use_sim_time}],
    )

    # ── Telemetry node ─────────────────────────────────────────
    telemetry_node = Node(
        package="sbr_controller",
        executable="telemetry_node",
        name="telemetry_node",
        output="screen",
        parameters=[{"use_sim_time": use_sim_time}],
    )

    # ── twist_mux ──────────────────────────────────────────────
    twist_mux_config = os.path.join(bringup_dir, "config", "twist_mux.yaml")

    twist_mux = Node(
        package="twist_mux",
        executable="twist_mux",
        name="twist_mux",
        output="screen",
        parameters=[twist_mux_config],
        remappings=[("cmd_vel_out", "/cmd_vel")],
    )

    return LaunchDescription([
        use_sim_time_arg,
        robot_state_publisher,
        microros_agent,
        ekf_node,
        safety_node,
        telemetry_node,
        twist_mux,
    ])
