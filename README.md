# openclaw_go2

A reproducible experiment and code collection for **Radxa Airbox + Unitree Go2 + OpenClaw + Weixin**.

This repository is not intended to provide a complete ready-to-flash system image. Instead, it documents and preserves the core pieces needed to reproduce the setup:

- Core implementation for `OpenClaw -> ROS2 -> Go2`
- OpenClaw workspace skill
- Standalone `Go2 ROS2 bridge` workspace
- Local adaptation script for `unitree_ros2-master` on Airbox / ROS2 Jazzy / `end0`
- Local patch reference for Weixin `/go2 ...` commands

## 1. Implemented Capabilities

The following capabilities have been verified:

- Airbox can communicate with Go2 over Ethernet.
- `Unitree SDK2` can directly read Go2 status and send basic high-level actions.
- `unitree_ros2-master` on ROS2 Jazzy can expose Go2 status topics.
- The standalone ROS2 workspace `go2_bridge_ros2` can expose the `/go2/command` service.
- `go2_command_client stand-up / stop / status` can control Go2 through the ROS layer.
- The Weixin plugin can connect to OpenClaw.
- The Weixin side can bypass the LLM and call the ROS bridge through deterministic `/go2 ...` commands.

## 2. Directory Overview

### 2.1 `openclaw_skill/go2_control_skill`

OpenClaw workspace skill for mapping high-level commands to the Go2 ROS bridge.

Key files:

- `SKILL.md`
- `README.md`
- `scripts/go2_cmd.sh`
- `scripts/go2_controller.cpp`
- `scripts/build_go2_bridge.sh`

Note: this directory contains the OpenClaw-side skill definition and the early SDK2 direct bridge code. The recommended final control path has moved to the ROS2 bridge, but these files are kept for reference and comparison.

### 2.2 `go2_bridge_ros2`

A standalone ROS2 workspace dedicated to the Go2 command bridge.

It contains two ROS2 packages:

- `go2_bridge_msgs`
  - Provides `Go2Command.srv`
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

### 2.3 `unitree_ros2_overlay/setup_jazzy.sh`

This is a local adaptation script for the current Airbox environment. It is not the original upstream `unitree_ros2-master` script.

It does the following:

- Sources ROS2 Jazzy.
- Sources `unitree_ros2-master/cyclonedds_ws/install/setup.bash`.
- Forces `rmw_cyclonedds_cpp`.
- Binds DDS communication to `end0`.

### 2.4 `wechat_go2_patch`

Local command patch reference for `@tencent-weixin/openclaw-weixin`.

Purpose:

- Add a deterministic `/go2 ...` command entry to the Weixin plugin.
- Add deterministic natural-language mappings for Go2, such as "让 go2 站起来" and "让机器狗停止".
- Restrict the Weixin entry-layer trigger condition so that only `/` commands or messages explicitly mentioning `go2`, `unitree`, `宇树`, `机器狗`, or `机器犬` enter command recognition. This avoids affecting normal chat messages such as "你好".

Files:

- `slash-commands.js`: modified command handler reference implementation, including `/go2 ...` and Go2 natural-language action mapping.
- `process-message.js.patch`: Weixin entry-layer patch that limits command pre-checking to slash commands and explicit Go2 messages.
- `slash-commands.js.bak`: original backup version.

## 3. Reproduction Requirements

The following requirements should be met for reliable reproduction.

### 3.1 Hardware / System

- Radxa Airbox, `aarch64`
- Unitree Go2
- Airbox connected directly to Go2 over Ethernet, or both devices on the same reachable network segment
- This document assumes the Airbox `end0` network interface

### 3.2 Software Environment

- ROS2 Jazzy
- OpenClaw around version `2026.5.x`
- Installed `@tencent-weixin/openclaw-weixin`
- Existing and buildable `unitree_ros2-master` source tree

### 3.3 Network Assumptions

Common configuration used during testing:

- Go2: `192.168.123.161`
- Airbox `end0`: `192.168.123.222/24`

Check with:

```bash
ip -4 addr show dev end0
ping -c 4 192.168.123.161
```

## 4. Install Dependencies

### 4.1 ROS2 Dependencies

```bash
sudo apt update
sudo apt install -y \
  ros-jazzy-rmw-cyclonedds-cpp \
  ros-jazzy-rosidl-generator-dds-idl \
  libyaml-cpp-dev
```

### 4.2 OpenClaw

Make sure OpenClaw is installed and available:

```bash
export PATH=/home/radxa/.npm-global/bin:$PATH
openclaw --version
```

### 4.3 Weixin Plugin

Install:

```bash
openclaw plugins install "@tencent-weixin/openclaw-weixin@2.4.4"
openclaw config set plugins.entries.openclaw-weixin.enabled true
openclaw channels login --channel openclaw-weixin
```

If QR login succeeds but the Weixin side still behaves incorrectly, first check whether `openclaw gateway` is healthy:

```bash
openclaw gateway restart
curl -s http://127.0.0.1:18789/health
```

## 5. Build `unitree_ros2-master`

### 5.1 Build `cyclonedds_ws`

```bash
cd ~/unitree_ros2-master/unitree_ros2-master/cyclonedds_ws
source /opt/ros/jazzy/setup.bash
colcon build
```

### 5.2 Verify Go2 Topics

```bash
source ~/unitree_ros2-master/unitree_ros2-master/setup_jazzy.sh
ros2 topic list | grep -E 'sportmodestate|lowstate|wireless'
```

Expected topics include at least:

- `/sportmodestate`
- `/lf/sportmodestate`
- `/lowstate`
- `/wirelesscontroller`

### 5.3 Build the Example Workspace

```bash
cd ~/unitree_ros2-master/unitree_ros2-master/example
source ~/unitree_ros2-master/unitree_ros2-master/setup_jazzy.sh
colcon build
```

### 5.4 Verify Official High-Level Control

Example:

```bash
./install/unitree_ros2_example/bin/go2_sport_client 4
```

Here `4` corresponds to `STAND_UP`.

## 6. Build the Standalone `go2_bridge_ros2`

```bash
cd ~/go2_bridge_ros2
source /opt/ros/jazzy/setup.bash
source ~/unitree_ros2-master/unitree_ros2-master/cyclonedds_ws/install/setup.bash
colcon build
```

## 7. Start the Standalone ROS Bridge

Open a new terminal:

```bash
export GO2_INTERFACE=end0
source ~/unitree_ros2-master/unitree_ros2-master/setup_jazzy.sh
source ~/go2_bridge_ros2/install/setup.bash
ros2 run go2_bridge_nodes go2_bridge_node
```

Keep this terminal running.

## 8. Verify the ROS Bridge Locally

Open another terminal:

```bash
source ~/unitree_ros2-master/unitree_ros2-master/setup_jazzy.sh
source ~/go2_bridge_ros2/install/setup.bash
ros2 run go2_bridge_nodes go2_command_client status
ros2 run go2_bridge_nodes go2_command_client stand-up
ros2 run go2_bridge_nodes go2_command_client stop
```

If these commands return:

```text
success: true
exit_code: 0
message: ok
```

then the following path is working:

```text
ROS2 bridge -> Go2
```

## 9. Local Model, Optional

### 9.1 Notes

This project previously tested the Airbox local `GenieAPIService` as the OpenClaw LLM backend.

This path can start in some environments, but its stability depends on the local `QAIRT/QNN` runtime environment. It is not guaranteed to work out of the box. For reproduction, it is **not required**.

### 9.2 Run a Local Model

```bash
source /home/radxa/miniconda3/etc/profile.d/conda.sh
conda activate llm
export LD_LIBRARY_PATH=/home/radxa/qairt/2.42.0.251225/lib/aarch64-oe-linux-gcc11.2:/home/radxa/ai-engine-direct-helper/script/qai_appbuilder/libs:$LD_LIBRARY_PATH
export ADSP_LIBRARY_PATH=/home/radxa/qairt/2.42.0.251225/lib/hexagon-v73/unsigned
cd ~/ai-engine-direct-helper/samples
python genie/python/GenieAPIService.py --modelname "Phi-3.5-mini" --loadmodel --profile
```

Notes:

- The local LLM path is experimental.
- It is not required for the ROS bridge.
- For stable Go2 control from Weixin, prefer deterministic `/go2 ...` commands.
- If natural language is used, only support deterministic rules that explicitly mention Go2. Do not let normal chat messages enter robot command recognition.

## 10. Control Go2 from Weixin

### 10.1 Core Principle

Weixin-side Go2 control should be split into three paths:

- Normal chat, such as `你好`, goes directly to the OpenClaw AI conversation flow.
- Slash commands, such as `/go2 stand-up`, directly call the Go2 bridge.
- Natural-language messages that explicitly mention Go2, such as `让go2站起来`, are mapped by deterministic rules to Go2 bridge commands.

Do not send every Weixin text message through robot command recognition. This degrades normal chat quality. During debugging, normal conversation such as `你好` recovered after the entry-layer trigger was narrowed to `/` commands or messages explicitly containing `go2`, `unitree`, `宇树`, `机器狗`, or `机器犬`.

### 10.2 Recommended Command Protocol

Use fixed commands in Weixin:

```text
/go2 status
/go2 stand-up
/go2 stop
/go2 sit
/go2 rise-sit
/go2 recover-stand
/go2 move --vx 0.2 --vy 0 --vyaw 0 --duration 1.0
```

These commands should be recognized directly by the Weixin plugin and executed through:

```text
wechat_go2.sh
-> go2_command_client
-> /go2/command
-> Go2
```

### 10.3 Supported Natural-Language Examples

The following examples are recognized by deterministic rules and do not rely on free-form LLM interpretation:

```text
让go2站起来
让go2起立
让机器狗停止
检查go2是否在线
让go2前进
让go2后退
让go2左转
让go2右转
```

Movement commands use a short duration and automatic stop by default. For example, forward movement maps to:

```text
/go2 move --vx 0.2 --vy 0.0 --vyaw 0.0 --duration 1.0
```

### 10.4 Apply the Weixin Plugin Patch

Example:

```bash
PLUGIN=/home/radxa/.openclaw/npm/node_modules/@tencent-weixin/openclaw-weixin
cp wechat_go2_patch/slash-commands.js "$PLUGIN/dist/src/messaging/slash-commands.js"
cd "$PLUGIN"
patch -p0 < /home/radxa/openclaw_go2/wechat_go2_patch/process-message.js.patch
systemctl --user restart openclaw-gateway.service
```

If `process-message.js` was already modified manually, verify that the entry condition is:

```js
if (shouldCheckDirectCommand(textBody)) {
```

### 10.5 Weixin Command Response

- `/go2 status` should return the current Go2 status JSON.
- `/go2 stand-up` should return `success: true`.
- The `go2_bridge_node` terminal should show the corresponding log output.

## 11. Recommended Runtime Setup

If the goal is stable Go2 control, keep the following two terminals running.

### Terminal A: Go2 Bridge

```bash
export GO2_INTERFACE=end0
source ~/unitree_ros2-master/unitree_ros2-master/setup_jazzy.sh
source ~/go2_bridge_ros2/install/setup.bash
ros2 run go2_bridge_nodes go2_bridge_node
```

### Terminal B: OpenClaw Gateway

```bash
export PATH=/home/radxa/.npm-global/bin:$PATH
openclaw gateway restart
```

The local model terminal can be managed separately if used.

## 12. Known Limitations

- Upstream `unitree_ros2-master` mainly targets `foxy/humble`. This repository provides a local `jazzy` adaptation, so not all upstream examples are guaranteed to behave identically.
- `go2_stand_example` is a low-level joint trajectory example and is not recommended as the final bridge backend.
- The official Weixin plugin does not recognize `/go2` by default. A local command protocol patch or a custom plugin is required.
- Local `Genie` model stability can vary significantly across Airbox environments.

## 13. Most Reproducible Core Path

The recommended minimal closed loop is:

```text
Go2 direct network
-> unitree_ros2-master topics visible
-> go2_bridge_ros2 can call /go2/command
-> Weixin /go2 status returns a response
```

This path is more stable than "natural language + local LLM automatic routing" and is the best first reproduction target.
