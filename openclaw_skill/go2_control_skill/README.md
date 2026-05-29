# go2-control-skill

This directory contains a workspace skill and a local command bridge so OpenClaw can control a Unitree Go2 on the Airbox.

The structure matches the same separation used in the arm documents:

- OpenClaw skill layer: decides which high-level action to run
- local command bridge: `go2_bridge_ros2` ROS2 bridge client
- Unitree SDK2 backend: `scripts/go2_controller.cpp`

## Files

- `SKILL.md`
  The OpenClaw skill instructions and command-selection rules.
- `go2_bridge_ros2` ROS2 bridge client
  Wrapper that picks a network interface, builds the bridge if needed, and runs the controller.
- `go2_bridge_ros2`
  Standalone ROS2 workspace that exposes /go2/command and calls Unitree ROS2 high-level sport control.
- `scripts/go2_controller.cpp`
  High-level Go2 controller built on `SportClient` and `RobotStateClient`.
- `scripts/CMakeLists.txt`
  Minimal local CMake project for the controller binary.

## Environment

- Unitree SDK2 root: `/home/radxa/unitree_sdk2/unitree_sdk2-main`
- Workspace skill root: `/home/radxa/.openclaw/workspace/skills/go2_control_skill`
- Default Go2 interface resolution:
  1. `GO2_INTERFACE` env var
  2. default route interface
  3. `wlp1s0`
  4. `end0`
  5. `end1`

## Build

```bash
~/.openclaw/workspace/skills/go2_control_skill/scripts/build_go2_bridge.sh
```

## Common commands

```bash
source /home/radxa/unitree_ros2-master/unitree_ros2-master/setup_jazzy.sh
source /home/radxa/go2_bridge_ros2/install/setup.bash
ros2 run go2_bridge_nodes go2_command_client status
ros2 run go2_bridge_nodes go2_command_client stand-up
ros2 run go2_bridge_nodes go2_command_client sit
ros2 run go2_bridge_nodes go2_command_client recover-stand
ros2 run go2_bridge_nodes go2_command_client move --vx 0.2 --vy 0.0 --vyaw 0.0 --duration 1.5
ros2 run go2_bridge_nodes go2_command_client stop
```

To force a specific interface:

```bash
GO2_INTERFACE=wlp1s0 ~/.openclaw/workspace/skills/go2_control_skill/scripts/go2_cmd.sh status
```

## Failure handling

If `status` reports `success: false`:

- confirm Go2 is powered on
- confirm the Airbox and Go2 are on the same network
- set the correct interface with `GO2_INTERFACE=...`
- retry `status` before sending motion commands

This skill now uses a dedicated standalone ROS2 workspace: /home/radxa/go2_bridge_ros2. The bridge talks to unitree_ros2-master over ROS2 and forwards high-level sport commands to Go2.
