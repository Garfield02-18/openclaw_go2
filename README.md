# openclaw_go2

一个面向 **Radxa Airbox + Unitree Go2 + OpenClaw + 微信** 的可复现实验文档与代码集合。

本仓库的目标不是提供一套“下载即用”的完整系统镜像，而是提供：

- `OpenClaw -> ROS2 -> Go2` 的核心实现代码
- `OpenClaw` 工作区 skill
- 独立的 `Go2 ROS2 bridge` 工作区
- `unitree_ros2-master` 在 Airbox / ROS2 Jazzy / `end0` 网口下的本地适配脚本
- 微信 `/go2 ...` 命令的本地补丁参考

## 1. 当前已经实现的能力

当前代码已经验证通过的能力：

- Airbox 通过网线与 Go2 连通
- 通过 `Unitree SDK2` 直连读取 Go2 状态、发送基本高层动作
- `unitree_ros2-master` 在 ROS2 Jazzy 下可以看到 Go2 的状态话题
- 独立 ROS2 工作区 `go2_bridge_ros2` 可以暴露 `/go2/command` service
- `go2_command_client stand-up / stop / status` 能通过 ROS 层调用 Go2
- 微信插件已经可接入 OpenClaw
- 微信侧可通过确定性命令 `/go2 ...` 绕过 LLM，直接调 ROS bridge

## 2. 目录说明

### 2.1 `openclaw_skill/go2_control_skill`

OpenClaw 工作区 skill，用于把高层指令映射到 Go2 ROS bridge。

关键文件：

- `SKILL.md`
- `README.md`
- `scripts/go2_cmd.sh`
- `scripts/go2_controller.cpp`
- `scripts/build_go2_bridge.sh`

说明：这部分是 OpenClaw 侧“技能定义”和早期 SDK2 直连桥的代码。当前推荐的最终控制路径已经切到 ROS2 bridge，但这些文件仍保留以便参考和对照。

### 2.2 `go2_bridge_ros2`

独立的 ROS2 工作区，只用于 Go2 的 ROS command bridge。

包含两个 ROS2 package：

- `go2_bridge_msgs`
  - 提供 `Go2Command.srv`
- `go2_bridge_nodes`
  - `go2_bridge_node.py`
  - `go2_command_client.py`

当前推荐控制路径：

```text
OpenClaw
-> go2_command_client
-> /go2/command
-> go2_bridge_node
-> Unitree ROS2 /api/sport/request
-> Go2
```

### 2.3 `unitree_ros2_overlay/setup_jazzy.sh`

这是针对当前 Airbox 本机环境写的适配脚本，不是上游 `unitree_ros2-master` 官方原始脚本。

作用：

- source ROS2 Jazzy
- source `unitree_ros2-master/cyclonedds_ws/install/setup.bash`
- 强制使用 `rmw_cyclonedds_cpp`
- 将 DDS 通信绑定到 `end0`

### 2.4 `wechat_go2_patch`

这是对 `@tencent-weixin/openclaw-weixin` 的本地命令补丁参考。

作用：

- 给微信插件增加 `/go2 ...` 确定性命令入口。
- 增加面向 Go2 的确定性自然语言映射，例如“让 go2 站起来”“让机器狗停止”。
- 在微信入口层限制触发条件，只有 `/` 命令或明确提到 `go2`、`unitree`、`宇树`、`机器狗`、`机器犬` 的消息才进入命令识别，避免影响“你好”等普通聊天。

当前目录包含：

- `slash-commands.js`：修改后的命令处理参考实现，包含 `/go2 ...` 和 Go2 自然语言动作映射。
- `process-message.js.patch`：微信入口层补丁，用于把命令预检查限制到斜杠命令和明确 Go2 消息。
- `slash-commands.js.bak`：原始备份版本。

## 3. 复现前提

以下前提必须满足，才能高概率复现：

### 3.1 硬件 / 系统

- Radxa Airbox（aarch64）
- Unitree Go2
- Airbox 与 Go2 通过网线直连，或保证在同一可通信网段
- 当前文档默认使用 Airbox 的 `end0` 网口

### 3.2 软件环境

- ROS2 Jazzy
- OpenClaw 2026.5.x 左右版本
- 已安装 `@tencent-weixin/openclaw-weixin`
- 已存在 `unitree_ros2-master` 代码树并可编译

### 3.3 网络假设

当前实践里常见配置：

- Go2: `192.168.123.161`
- Airbox `end0`: `192.168.123.222/24`

可用以下命令确认：

```bash
ip -4 addr show dev end0
ping -c 4 192.168.123.161
```

## 4. 依赖安装

### 4.1 ROS2 依赖

```bash
sudo apt update
sudo apt install -y \
  ros-jazzy-rmw-cyclonedds-cpp \
  ros-jazzy-rosidl-generator-dds-idl \
  libyaml-cpp-dev
```

### 4.2 OpenClaw 本体

需要确保本机已安装 OpenClaw，并且命令可用：

```bash
export PATH=/home/radxa/.npm-global/bin:$PATH
openclaw --version
```

### 4.3 微信插件

安装：

```bash
openclaw plugins install "@tencent-weixin/openclaw-weixin@2.4.4"
openclaw config set plugins.entries.openclaw-weixin.enabled true
openclaw channels login --channel openclaw-weixin
```

如果扫码成功但微信侧仍显示异常，请先确认 `openclaw gateway` 是否正常：

```bash
openclaw gateway restart
curl -s http://127.0.0.1:18789/health
```

## 5. 编译 `unitree_ros2-master`

### 5.1 编译 `cyclonedds_ws`

```bash
cd ~/unitree_ros2-master/unitree_ros2-master/cyclonedds_ws
source /opt/ros/jazzy/setup.bash
colcon build
```

### 5.2 测试 Go2 话题是否可见

```bash
source ~/unitree_ros2-master/unitree_ros2-master/setup_jazzy.sh
ros2 topic list | grep -E 'sportmodestate|lowstate|wireless'
```

预期至少看到：

- `/sportmodestate`
- `/lf/sportmodestate`
- `/lowstate`
- `/wirelesscontroller`

### 5.3 编译 example 工作区

```bash
cd ~/unitree_ros2-master/unitree_ros2-master/example
source ~/unitree_ros2-master/unitree_ros2-master/setup_jazzy.sh
colcon build
```

### 5.4 验证官方高层控制是否能工作

例如：

```bash
./install/unitree_ros2_example/bin/go2_sport_client 4
```

其中 `4` 对应 `STAND_UP`。

## 6. 编译独立 `go2_bridge_ros2`

```bash
cd ~/go2_bridge_ros2
source /opt/ros/jazzy/setup.bash
source ~/unitree_ros2-master/unitree_ros2-master/cyclonedds_ws/install/setup.bash
colcon build
```

## 7. 启动独立 ROS bridge

新开终端：

```bash
export GO2_INTERFACE=end0
source ~/unitree_ros2-master/unitree_ros2-master/setup_jazzy.sh
source ~/go2_bridge_ros2/install/setup.bash
ros2 run go2_bridge_nodes go2_bridge_node
```

此终端应保持运行。

## 8. 本地验证 ROS bridge

另开一个终端：

```bash
source ~/unitree_ros2-master/unitree_ros2-master/setup_jazzy.sh
source ~/go2_bridge_ros2/install/setup.bash
ros2 run go2_bridge_nodes go2_command_client status
ros2 run go2_bridge_nodes go2_command_client stand-up
ros2 run go2_bridge_nodes go2_command_client stop
```

如果这些命令返回：

```text
success: true
exit_code: 0
message: ok
```

则说明：

```text
ROS2 bridge -> Go2
```

已经打通。

## 9. 本地模型（可选）

### 9.1 说明

当前项目曾尝试使用 Airbox 本地 `GenieAPIService` 作为 OpenClaw 的 LLM 后端。

这条路径在部分环境中能启动，但稳定性依赖本机 `QAIRT/QNN` 运行时环境，不能保证开箱即用。对复现者而言，**不是必须项**。

### 9.2 如果要用本地模型

```bash
source /home/radxa/miniconda3/etc/profile.d/conda.sh
conda activate llm
export LD_LIBRARY_PATH=/home/radxa/qairt/2.42.0.251225/lib/aarch64-oe-linux-gcc11.2:/home/radxa/ai-engine-direct-helper/script/qai_appbuilder/libs:$LD_LIBRARY_PATH
export ADSP_LIBRARY_PATH=/home/radxa/qairt/2.42.0.251225/lib/hexagon-v73/unsigned
cd ~/ai-engine-direct-helper/samples
python genie/python/GenieAPIService.py --modelname "Phi-3.5-mini" --loadmodel --profile
```

但请注意：

- 本地 LLM 路径是实验性的。
- 与 ROS bridge 打通无强依赖。
- 稳定控狗优先使用确定性微信命令 `/go2 ...`。
- 如果要使用自然语言，建议只支持明确提到 Go2 的确定性规则，不要让普通聊天进入机器人命令识别。

## 10. 微信侧控制 Go2

### 10.1 关键原则

微信侧控制 Go2 应分成三条路径：

- 普通聊天，例如 `你好`，直接进入 OpenClaw AI 对话流程。
- 斜杠命令，例如 `/go2 stand-up`，直接调用 Go2 bridge。
- 明确提到 Go2 的自然语言，例如 `让go2站起来`，通过确定性规则映射为 Go2 bridge 命令。

不要让所有微信文本都先进入机器人命令识别，否则会影响普通聊天质量。实际调试中，入口层收窄到只处理 `/` 命令或明确包含 `go2`、`unitree`、`宇树`、`机器狗`、`机器犬` 后，`你好` 这类普通对话可以恢复正常。

### 10.2 推荐命令协议

微信里使用固定命令：

```text
/go2 status
/go2 stand-up
/go2 stop
/go2 sit
/go2 rise-sit
/go2 recover-stand
/go2 move --vx 0.2 --vy 0 --vyaw 0 --duration 1.0
```

这类命令应由微信插件直接识别并调用：

```text
wechat_go2.sh
-> go2_command_client
-> /go2/command
-> Go2
```

### 10.3 支持的自然语言示例

以下文本会被确定性规则识别，不依赖 LLM 自由理解：

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

默认移动类命令会带短时长和自动 stop，例如前进会映射为：

```text
/go2 move --vx 0.2 --vy 0.0 --vyaw 0.0 --duration 1.0
```

### 10.4 应用微信插件补丁

示例操作：

```bash
PLUGIN=/home/radxa/.openclaw/npm/node_modules/@tencent-weixin/openclaw-weixin
cp wechat_go2_patch/slash-commands.js "$PLUGIN/dist/src/messaging/slash-commands.js"
cd "$PLUGIN"
patch -p0 < /home/radxa/openclaw_go2/wechat_go2_patch/process-message.js.patch
systemctl --user restart openclaw-gateway.service
```

如果已经手工改过 `process-message.js`，需要确认入口判断是：

```js
if (shouldCheckDirectCommand(textBody)) {
```

### 10.5 微信命令返回特点

- `/go2 status` 应返回 Go2 当前状态 JSON
- `/go2 stand-up` 应返回 `success: true`
- 同时 `go2_bridge_node` 终端会出现对应的日志输出

## 11. 建议的运行方式

如果目标是稳定控制 Go2，推荐同时保持以下 2 个终端长期运行：

### 终端 A：Go2 bridge

```bash
export GO2_INTERFACE=end0
source ~/unitree_ros2-master/unitree_ros2-master/setup_jazzy.sh
source ~/go2_bridge_ros2/install/setup.bash
ros2 run go2_bridge_nodes go2_bridge_node
```

### 终端 B：OpenClaw gateway

```bash
export PATH=/home/radxa/.npm-global/bin:$PATH
openclaw gateway restart
```

本地模型终端（如果使用）可单独维护。

## 12. 已知限制

- `unitree_ros2-master` 官方主要面向 `foxy/humble`，这里做的是 `jazzy` 本地适配，不保证所有上游示例都行为一致
- `go2_stand_example` 属于低层关节轨迹示例，不建议作为最终 bridge 后端
- 微信插件官方版默认并不认识 `/go2`，需要本地命令协议补丁或自定义插件支持
- 本地 `Genie` 模型链在不同 Airbox 环境下稳定性差异较大

## 13. 当前最可复现的核心路径

最推荐复现的最小闭环是：

```text
Go2 直连网络
-> unitree_ros2-master 话题可见
-> go2_bridge_ros2 可调用 /go2/command
-> 微信 /go2 status 能得到返回
```

这条路径比“自然语言 + 本地 LLM 自动路由”更稳定，更适合作为第一版复现目标。
