#!/usr/bin/env python3
from __future__ import annotations

import json
import time
from typing import Dict, Tuple

import rclpy
from rclpy.node import Node

from go2_bridge_msgs.srv import Go2Command
from unitree_api.msg import Request
from unitree_go.msg import SportModeState

ROBOT_SPORT_API_ID_DAMP = 1001
ROBOT_SPORT_API_ID_BALANCESTAND = 1002
ROBOT_SPORT_API_ID_STOPMOVE = 1003
ROBOT_SPORT_API_ID_STANDUP = 1004
ROBOT_SPORT_API_ID_STANDDOWN = 1005
ROBOT_SPORT_API_ID_RECOVERYSTAND = 1006
ROBOT_SPORT_API_ID_MOVE = 1008
ROBOT_SPORT_API_ID_SIT = 1009
ROBOT_SPORT_API_ID_RISESIT = 1010


class Go2BridgeNode(Node):
    def __init__(self) -> None:
        super().__init__('go2_bridge_node')

        self.declare_parameter('status_topic', '/sportmodestate')
        self.declare_parameter('sport_request_topic', '/api/sport/request')
        self.declare_parameter('status_timeout_s', 2.0)
        self.declare_parameter('command_settle_s', 0.2)

        self._status_topic = str(self.get_parameter('status_topic').value)
        self._sport_request_topic = str(self.get_parameter('sport_request_topic').value)
        self._status_timeout_s = float(self.get_parameter('status_timeout_s').value)
        self._command_settle_s = float(self.get_parameter('command_settle_s').value)

        self._sport_pub = self.create_publisher(Request, self._sport_request_topic, 10)
        self._sport_state_sub = self.create_subscription(
            SportModeState,
            self._status_topic,
            self._on_state,
            10,
        )
        self._latest_state: SportModeState | None = None
        self._latest_state_time = 0.0

        self._service = self.create_service(Go2Command, '/go2/command', self._handle_command)
        self.get_logger().info(
            f'Standalone Go2 ROS bridge ready: service=/go2/command request_topic={self._sport_request_topic} status_topic={self._status_topic}'
        )

    def _on_state(self, msg: SportModeState) -> None:
        self._latest_state = msg
        self._latest_state_time = time.monotonic()

    def _now_id(self) -> int:
        return time.monotonic_ns()

    def _publish_simple(self, api_id: int) -> None:
        req = Request()
        req.header.identity.api_id = api_id
        req.header.identity.id = self._now_id()
        req.parameter = ''
        req.binary = []
        self._sport_pub.publish(req)

    def _publish_move(self, vx: float, vy: float, vyaw: float) -> None:
        req = Request()
        req.header.identity.api_id = ROBOT_SPORT_API_ID_MOVE
        req.header.identity.id = self._now_id()
        req.parameter = json.dumps({'x': vx, 'y': vy, 'z': vyaw})
        req.binary = []
        self._sport_pub.publish(req)

    def _wait_for_state(self) -> Tuple[bool, SportModeState | None]:
        deadline = time.monotonic() + self._status_timeout_s
        while time.monotonic() < deadline:
            if self._latest_state is not None:
                age = time.monotonic() - self._latest_state_time
                if age <= self._status_timeout_s:
                    return True, self._latest_state
            rclpy.spin_once(self, timeout_sec=0.05)
        return False, None

    def _handle_status(self, response: Go2Command.Response) -> Go2Command.Response:
        self.get_logger().info('Handling status request')
        ok, state = self._wait_for_state()
        response.success = ok
        response.exit_code = 0 if ok else 1
        response.message = 'ok' if ok else f'no state on {self._status_topic} within {self._status_timeout_s:.1f}s'
        if not ok or state is None:
            response.stdout = ''
            response.stderr = ''
            return response

        payload = {
            'mode': int(state.mode),
            'gait_type': int(state.gait_type),
            'progress': float(state.progress),
            'body_height': float(state.body_height),
            'position': [float(v) for v in state.position],
            'velocity': [float(v) for v in state.velocity],
            'yaw_speed': float(state.yaw_speed),
            'rpy': [float(v) for v in state.imu_state.rpy],
            'error_code': int(state.error_code),
        }
        response.stdout = json.dumps(payload, ensure_ascii=False)
        response.stderr = ''
        return response

    def _handle_motion(self, request: Go2Command.Request, response: Go2Command.Response) -> Go2Command.Response:
        command = request.command.strip()
        mapping: Dict[str, int] = {
            'balance-stand': ROBOT_SPORT_API_ID_BALANCESTAND,
            'stand-down': ROBOT_SPORT_API_ID_STANDDOWN,
            'stand-up': ROBOT_SPORT_API_ID_STANDUP,
            'damp': ROBOT_SPORT_API_ID_DAMP,
            'recover-stand': ROBOT_SPORT_API_ID_RECOVERYSTAND,
            'sit': ROBOT_SPORT_API_ID_SIT,
            'rise-sit': ROBOT_SPORT_API_ID_RISESIT,
            'stop': ROBOT_SPORT_API_ID_STOPMOVE,
        }

        if command == 'move':
            self.get_logger().info(
                f'Publishing move vx={request.vx:.3f} vy={request.vy:.3f} vyaw={request.vyaw:.3f} duration={request.duration:.3f}'
            )
            self._publish_move(request.vx, request.vy, request.vyaw)
            if request.duration > 0.0:
                time.sleep(float(request.duration))
                self._publish_simple(ROBOT_SPORT_API_ID_STOPMOVE)
        elif command in mapping:
            self.get_logger().info(f'Publishing sport command: {command}')
            self._publish_simple(mapping[command])
            time.sleep(self._command_settle_s)
        else:
            raise ValueError(f'unsupported command: {command}')

        response.success = True
        response.exit_code = 0
        response.message = 'ok'
        response.stdout = ''
        response.stderr = ''
        return response

    def _handle_command(self, request: Go2Command.Request, response: Go2Command.Response) -> Go2Command.Response:
        try:
            if request.command.strip() == 'status':
                return self._handle_status(response)
            return self._handle_motion(request, response)
        except ValueError as exc:
            response.success = False
            response.exit_code = 2
            response.message = str(exc)
            response.stdout = ''
            response.stderr = ''
            return response
        except Exception as exc:  # pragma: no cover
            response.success = False
            response.exit_code = 1
            response.message = str(exc)
            response.stdout = ''
            response.stderr = ''
            return response


def main() -> None:
    rclpy.init()
    node = Go2BridgeNode()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
