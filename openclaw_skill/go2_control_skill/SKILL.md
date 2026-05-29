---
name: go2-control
description: Use when the user wants OpenClaw to control or check a Unitree Go2 from this Airbox through the standalone ROS2 bridge workspace at /home/radxa/go2_bridge_ros2. Trigger on English or Chinese requests such as 检查go2是否在线, go2在线吗, go2在吗, 让go2起立, 让go2坐下, 停止go2, 让go2前进, 让go2左转, recover stand, stand up, sit down, stop. Always call the ROS2 Go2 command client instead of answering from general knowledge.
---

# Go2 Control

Use this skill whenever the user wants to operate the Unitree Go2 connected to this Airbox.

Control path:

- OpenClaw decides the task-level action.
- This skill calls the standalone ROS2 client in `/home/radxa/go2_bridge_ros2`.
- The ROS2 bridge sends high-level sport requests to `unitree_ros2-master`.
- `unitree_ros2-master` forwards them to Go2.

Do not answer Go2 control or status questions from general knowledge. Always call the bridge first.

## Environment

- Skill root: `/home/radxa/.openclaw/workspace/skills/go2_control_skill`
- ROS2 client: `/home/radxa/go2_bridge_ros2/install/go2_bridge_nodes/lib/go2_bridge_nodes/go2_command_client`
- ROS2 bridge workspace: `/home/radxa/go2_bridge_ros2`
- Unitree ROS2 setup script: `/home/radxa/unitree_ros2-master/unitree_ros2-master/setup_jazzy.sh`

## Bridge startup

Before any Go2 command can work, the standalone ROS2 bridge must already be running on the Airbox:

```bash
export GO2_INTERFACE=end0
source /home/radxa/unitree_ros2-master/unitree_ros2-master/setup_jazzy.sh
source /home/radxa/go2_bridge_ros2/install/setup.bash
ros2 run go2_bridge_nodes go2_bridge_node
```

If `/go2/command` is unavailable, tell the user the ROS2 bridge is not running.

## Command client

Use these exact commands:

```bash
source /home/radxa/unitree_ros2-master/unitree_ros2-master/setup_jazzy.sh
source /home/radxa/go2_bridge_ros2/install/setup.bash
ros2 run go2_bridge_nodes go2_command_client status
ros2 run go2_bridge_nodes go2_command_client stand-up
ros2 run go2_bridge_nodes go2_command_client stand-down
ros2 run go2_bridge_nodes go2_command_client balance-stand
ros2 run go2_bridge_nodes go2_command_client recover-stand
ros2 run go2_bridge_nodes go2_command_client sit
ros2 run go2_bridge_nodes go2_command_client rise-sit
ros2 run go2_bridge_nodes go2_command_client stop
ros2 run go2_bridge_nodes go2_command_client move --vx 0.2 --vy 0.0 --vyaw 0.0 --duration 1.0
ros2 run go2_bridge_nodes go2_command_client move --vx 0.0 --vy 0.0 --vyaw 0.3 --duration 1.0
```

## Mandatory rule for status questions

When the user asks any of the following, you MUST call `status` first and only then answer:

- `检查go2是否在线`
- `go2在线吗`
- `go2在吗`
- `检查机器狗状态`
- `检查机器狗是否在线`
- `check if go2 is online`
- `is go2 online`
- `check go2 status`

Do not explain the JSON payload as if it were unrelated chat metadata.

Interpret `status` like this:

- If `success: true`, answer briefly that Go2 is online.
- If `success: false`, answer briefly that Go2 is not reachable and mention the ROS2 bridge or network may be down.
- If `stdout` contains JSON-like status fields such as `mode`, `position`, `rpy`, use them only as robot status, never as conversation metadata.

Recommended answer style for a successful check:

- `Go2在线。`
- `Go2在线，状态已收到。`

Recommended answer style for a failed check:

- `Go2当前不在线，未收到状态。请检查ROS2 bridge和网线连接。`

## Natural-language mapping

Always map these requests to commands:

- `让go2起立` -> `stand-up`
- `让go2站起来` -> `stand-up`
- `让go2坐下` -> `sit`
- `让go2恢复站立` -> `recover-stand`
- `停止go2` -> `stop`
- `让go2前进一步` -> `move --vx 0.2 --vy 0.0 --vyaw 0.0 --duration 1.0`
- `让go2后退` -> `move --vx -0.2 --vy 0.0 --vyaw 0.0 --duration 1.0`
- `让go2左转` -> `move --vx 0.0 --vy 0.0 --vyaw 0.3 --duration 1.0`
- `让go2右转` -> `move --vx 0.0 --vy 0.0 --vyaw -0.3 --duration 1.0`
- `stand up` -> `stand-up`
- `sit down` -> `sit`
- `recover and stand` -> `recover-stand`
- `stop go2` -> `stop`

## Response rules

- For status checks, keep the final answer short.
- For motion commands, tell the user whether the command was sent successfully.
- If the command client reports failure, summarize the failure instead of improvising.
- If the user asks for risky motion near people or obstacles, ask for confirmation first.

## Safety

- This skill controls real hardware. Prefer high-level sport commands only.
- Do not expose low-level DDS or motor commands unless the user explicitly asks for low-level control.
- Keep move durations short by default.
