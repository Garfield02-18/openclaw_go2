#!/usr/bin/env python3
from __future__ import annotations

import argparse
import sys

import rclpy
from rclpy.node import Node

from go2_bridge_msgs.srv import Go2Command


class Go2CommandClient(Node):
    def __init__(self) -> None:
        super().__init__('go2_command_client')
        self._client = self.create_client(Go2Command, '/go2/command')

    def send(self, args: argparse.Namespace) -> int:
        if not self._client.wait_for_service(timeout_sec=5.0):
            self.get_logger().error('service /go2/command not available')
            return 1

        req = Go2Command.Request()
        req.command = args.command
        req.vx = args.vx
        req.vy = args.vy
        req.vyaw = args.vyaw
        req.duration = args.duration
        req.with_services = args.with_services

        future = self._client.call_async(req)
        rclpy.spin_until_future_complete(self, future)
        if future.result() is None:
            self.get_logger().error('service call failed')
            return 1

        resp = future.result()
        print(f'success: {str(resp.success).lower()}')
        print(f'exit_code: {resp.exit_code}')
        print(f'message: {resp.message}')
        if resp.stdout:
            print('stdout:')
            print(resp.stdout, end='' if resp.stdout.endswith('\n') else '\n')
        if resp.stderr:
            print('stderr:')
            print(resp.stderr, end='' if resp.stderr.endswith('\n') else '\n')
        return 0 if resp.success else 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description='Call the standalone ROS2 Go2 command bridge')
    parser.add_argument('command')
    parser.add_argument('--vx', type=float, default=0.0)
    parser.add_argument('--vy', type=float, default=0.0)
    parser.add_argument('--vyaw', type=float, default=0.0)
    parser.add_argument('--duration', type=float, default=1.0)
    parser.add_argument('--with-services', action='store_true')
    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    rclpy.init()
    node = Go2CommandClient()
    try:
        code = node.send(args)
    finally:
        node.destroy_node()
        rclpy.shutdown()
    sys.exit(code)


if __name__ == '__main__':
    main()
