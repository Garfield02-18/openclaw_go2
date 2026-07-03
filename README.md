# openclaw_go2

A reproducible experiment and code collection for **Qualcomm-based Radxa Airbox + Unitree Go2 + OpenClaw + Weixin**.

Before starting, please make sure you have read the relevant documentation for the Qualcomm-based Radxa Airbox Q900: https://docs.radxa.com/fogwise/airbox-q900/system-use/uart-login

This repository is centered on one specific deployment target: **running a local robot control stack on a Qualcomm-based Radxa Airbox and exposing that control path through OpenClaw and Weixin.**

It is not intended to provide a complete ready-to-flash system image. Instead, it preserves and documents the core pieces required to reproduce the setup:

- Core implementation for `OpenClaw -> ROS2 -> Go2`
- OpenClaw workspace skill
- Standalone `Go2 ROS2 bridge` workspace
- Local adaptation script for `unitree_ros2-master` on the Qualcomm-based Radxa Airbox / ROS2 Jazzy / `end0`
- Local patch reference for Weixin `/go2 ...` commands
- Deployment notes specific to the **Qualcomm-based Radxa Airbox**

## 1. Why the Qualcomm-based Radxa Airbox

This project is built specifically around the **Qualcomm-based Radxa Airbox**, rather than a generic PC or a cloud host.

In this project, the Qualcomm-based Radxa Airbox is valuable because it can play several roles at once:

- a local Linux host for ROS2 and OpenClaw
- an edge controller deployed close to the robot
- an optional local LLM runtime host
- an Ethernet access point for direct communication with Unitree Go2
- an always-on gateway for Weixin and command integration

As a result, the Qualcomm-based Radxa Airbox can serve as a compact edge-computing node that keeps the command pipeline local:

```text
Weixin / OpenClaw
-> Qualcomm-based Radxa Airbox
-> ROS2 bridge
-> Unitree Go2
```

That is the main reason this repository exists.

## 2. Project Goal for the Qualcomm-based Radxa Airbox

The goal of this repository is not simply “control Go2.” More specifically, it is to:

- make Go2 controllable from a **Qualcomm-based Radxa Airbox**
- preserve the glue code actually needed on the Qualcomm-based Radxa Airbox
- document Qualcomm-based Radxa Airbox-specific environment setup and debugging issues
- provide a directory structure that can be uploaded, shared, and reproduced later

For that reason, this README intentionally keeps details that might be omitted in a generic robotics repository, such as:

- the Qualcomm-based Radxa Airbox network interface naming (`end0`)
- local ROS2 Jazzy adaptation
- OpenClaw local gateway usage on the Qualcomm-based Radxa Airbox
- optional QAIRT / Genie local model paths on the Qualcomm-based Radxa Airbox
- plugin patching and shell wrapper behavior on the Qualcomm-based Radxa Airbox

## 3. Verified Capabilities

The following capabilities have been verified on the Qualcomm-based Radxa Airbox:

- The Qualcomm-based Radxa Airbox can communicate with Go2 over Ethernet.
- `Unitree SDK2` can directly read Go2 state and send basic high-level actions.
- `unitree_ros2-master` on ROS2 Jazzy can expose Go2 state topics.
- The standalone ROS2 workspace `go2_bridge_ros2` can provide the `/go2/command` service.
- `go2_command_client stand-up / stop / status` can control Go2 through the ROS layer.
- The Weixin plugin can connect to OpenClaw.
- The Weixin side can bypass the LLM and call the ROS bridge through deterministic `/go2 ...` commands.

## 4. Qualcomm-based Radxa Airbox Environment Notes

### 4.1 Device Role

In this project, the Qualcomm-based Radxa Airbox acts as the:

- ROS2 host
- OpenClaw gateway host
- local execution host for bridge scripts
- network-side entry point connected to Go2
- optional Genie / QAIRT local model experiment host

### 4.2 Default Architecture Assumption

This repository assumes a directory layout on the Qualcomm-based Radxa Airbox roughly like this:

```text
Qualcomm-based Radxa Airbox
├── OpenClaw runtime
├── OpenClaw Weixin plugin
├── unitree_ros2-master
├── go2_bridge_ros2
├── optional ai-engine-direct-helper / Genie runtime
└── exported openclaw_go2 reproducible directory
```

### 4.3 Why This README Emphasizes the Qualcomm-based Radxa Airbox

During debugging, many failures were not generic Go2 problems. They were **Qualcomm-based Radxa Airbox deployment problems**, for example:

- the network interface name used by CycloneDDS did not match the actual Qualcomm-based Radxa Airbox interface
- ROS2 setup scripts conflicted with shell strict mode inside Qualcomm-based Radxa Airbox wrappers
- the OpenClaw plugin had to invoke scripts from the Qualcomm-based Radxa Airbox local filesystem
- local model stability depended on the exact QAIRT library layout on the Qualcomm-based Radxa Airbox

Because of that, this README treats the Qualcomm-based Radxa Airbox as part of the system rather than just “the machine where the code happened to run.”

## 5. Directory Overview

### 5.1 `openclaw_skill/go2_control_skill`

OpenClaw workspace skill directory used to map high-level commands to the Go2 ROS bridge.

Key files:

- `SKILL.md`
- `README.md`
- `scripts/go2_cmd.sh`
- `scripts/go2_controller.cpp`
- `scripts/build_go2_bridge.sh`

Note: this directory preserves both the OpenClaw-side skill definition and the earlier SDK2 direct bridge code. Although the recommended final path has moved to the ROS2 bridge, these files are still useful for reference.

### 5.2 `go2_bridge_ros2`

This is a standalone ROS2 workspace created specifically for Go2 command bridging.

It contains two ROS2 packages:

- `go2_bridge_msgs`
  - provides `Go2Command.srv`
- `go2_bridge_nodes`
  - `go2_bridge_node.py`
  - `go2_command_client.py`

Recommended control path:

```text
OpenClaw
-> go2_command_client
-> /go2/command
-> go2_bridge_node
-> Unitree ROS2 /api/sport/request
-> Go2
```

### 5.3 `unitree_ros2_overlay/setup_jazzy.sh`

This is a local adaptation script for the current Qualcomm-based Radxa Airbox environment. It is not the original upstream `unitree_ros2-master` script.

It is responsible for:

- sourcing ROS2 Jazzy
- sourcing `unitree_ros2-master/cyclonedds_ws/install/setup.bash`
- forcing `rmw_cyclonedds_cpp`
- binding DDS communication to the Qualcomm-based Radxa Airbox side network path used to connect to Go2

### 5.4 `wechat_go2_patch`

This is a local command patch reference for `@tencent-weixin/openclaw-weixin`.

Its purpose is to:

- add a deterministic `/go2 ...` command entry to the Weixin plugin
- add natural-language action mappings for Go2, such as “make go2 stand up” or “stop the robot dog”
- restrict command recognition so that only slash commands or messages explicitly mentioning `go2`, `unitree`, `宇树`, `机器狗`, or `机器犬` enter command parsing
- avoid interfering with normal chat messages such as “hello”

Files included:

- `slash-commands.js`
- `process-message.js.patch`
- `slash-commands.js.bak`

## 6. Reproduction Requirements

### 6.1 Hardware / System

- Qualcomm-based Radxa Airbox, `aarch64`
- Unitree Go2
- Qualcomm-based Radxa Airbox directly connected to Go2 over Ethernet, or both devices placed on the same reachable subnet
- this document assumes the Qualcomm-based Radxa Airbox Ethernet interface is `end0`

### 6.2 Software Environment

- Ubuntu / Linux environment on the Qualcomm-based Radxa Airbox
- ROS2 Jazzy
- OpenClaw around version `2026.5.x`
- installed `@tencent-weixin/openclaw-weixin`
- an existing and buildable `unitree_ros2-master` source tree

### 6.3 Network Assumptions

Common configuration used during testing:

- Go2: `192.168.123.161`
- Qualcomm-based Radxa Airbox `end0`: `192.168.123.222/24`

Check with:

```bash
ip -4 addr show dev end0
ping -c 4 192.168.123.161
```

## 7. Qualcomm-based Radxa Airbox Deployment Checklist

Before debugging the bridge itself, first verify the Qualcomm-based Radxa Airbox base environment:

- `uname -m` returns `aarch64`
- `ip -brief link` shows `end0`
- `source /opt/ros/jazzy/setup.bash` works correctly
- the OpenClaw gateway can start locally on the Qualcomm-based Radxa Airbox
- the Qualcomm-based Radxa Airbox can reach Go2 over Ethernet
- `unitree_ros2-master` example binaries can run on the Qualcomm-based Radxa Airbox

If any of these fail, fix that first. Many later “bridge issues” are actually caused by an incomplete Qualcomm-based Radxa Airbox base environment.

## 8. Install Dependencies

### 8.1 ROS2 Dependencies

```bash
sudo apt update
sudo apt install -y \
  ros-jazzy-rmw-cyclonedds-cpp \
  ros-jazzy-rosidl-generator-dds-idl \
  libyaml-cpp-dev
```

### 8.2 OpenClaw

Make sure OpenClaw is installed on the Qualcomm-based Radxa Airbox:

```bash
export PATH=/home/radxa/.npm-global/bin:$PATH
openclaw --version
```

### 8.3 Weixin Plugin

```bash
openclaw plugins install "@tencent-weixin/openclaw-weixin@2.4.4"
openclaw config set plugins.entries.openclaw-weixin.enabled true
openclaw channels login --channel openclaw-weixin
```

If QR login succeeds but Weixin-side behavior is still abnormal, first check whether the local OpenClaw gateway on the Qualcomm-based Radxa Airbox is healthy:

```bash
openclaw gateway restart
curl -s http://127.0.0.1:18789/health
```

## 9. Build `unitree_ros2-master`

### 9.1 Build `cyclonedds_ws`

```bash
cd ~/unitree_ros2-master/unitree_ros2-master/cyclonedds_ws
source /opt/ros/jazzy/setup.bash
colcon build
```

### 9.2 Verify Go2 Topics

```bash
source ~/unitree_ros2-master/unitree_ros2-master/setup_jazzy.sh
ros2 topic list | grep -E 'sportmodestate|lowstate|wireless'
```

Expected topics should include at least:

- `/sportmodestate`
- `/lf/sportmodestate`
- `/lowstate`
- `/wirelesscontroller`

### 9.3 Build the Example Workspace

```bash
cd ~/unitree_ros2-master/unitree_ros2-master/example
source ~/unitree_ros2-master/unitree_ros2-master/setup_jazzy.sh
colcon build
```

### 9.4 Verify Official High-Level Control

For example:

```bash
./install/unitree_ros2_example/bin/go2_sport_client 4
```

Here `4` corresponds to `STAND_UP`.

## 10. Build the Standalone `go2_bridge_ros2`

```bash
cd ~/go2_bridge_ros2
source /opt/ros/jazzy/setup.bash
source ~/unitree_ros2-master/unitree_ros2-master/cyclonedds_ws/install/setup.bash
colcon build
```

## 11. Start the Standalone ROS Bridge

Open a new terminal on the Qualcomm-based Radxa Airbox:

```bash
export GO2_INTERFACE=end0
source ~/unitree_ros2-master/unitree_ros2-master/setup_jazzy.sh
source ~/go2_bridge_ros2/install/setup.bash
ros2 run go2_bridge_nodes go2_bridge_node
```

Keep this terminal running.

## 12. Verify the ROS Bridge Locally

Open another terminal:

```bash
source ~/unitree_ros2-master/unitree_ros2-master/setup_jazzy.sh
source ~/go2_bridge_ros2/install/setup.bash
ros2 run go2_bridge_nodes go2_command_client status
ros2 run go2_bridge_nodes go2_command_client stand-up
ros2 run go2_bridge_nodes go2_command_client stop
```

If the commands return:

```text
success: true
exit_code: 0
message: ok
```

then the following path is working:

```text
Qualcomm-based Radxa Airbox ROS2 bridge -> Go2
```

## 13. Local Model (Optional)

### 13.1 Notes

This project previously also tested using `GenieAPIService` on the Qualcomm-based Radxa Airbox as the local LLM backend for OpenClaw.

This path can start successfully in some environments, but its stability depends on the local `QAIRT/QNN` runtime environment on the Qualcomm-based Radxa Airbox. It is **not guaranteed to work out of the box**, and it is **not required** to reproduce the core project.

### 13.2 Why It Is Optional

For robot control, a deterministic `/go2 ...` command path is more important than a free-form local chat model.

So as long as the Qualcomm-based Radxa Airbox can already run:

- the OpenClaw gateway
- the Weixin plugin
- the ROS2 bridge
- the Go2 control binaries

then the project already has its core value, even without enabling the local model.

## 14. Recommended Startup Order

If you want the most stable reproduction on the Qualcomm-based Radxa Airbox, bring the system up in the following order:

1. Qualcomm-based Radxa Airbox to Go2 network
2. ROS2 Jazzy environment
3. `unitree_ros2-master` example control
4. standalone `go2_bridge_ros2`
5. OpenClaw gateway
6. Weixin `/go2 ...` command patch
7. optional local LLM backend

This order reflects the actual debugging and integration process of the project.

## 15. Demo videos
[OpenClaw Go2 Demo](assets/demo1.mp4)

[OpenClaw Go2 Demo](assets/demo2.mp4)

## Official Documentation
-（https://radxa.com/products/fogwise/airbox-q900/）

-（https://docs.radxa.com/fogwise/airbox-q900）

