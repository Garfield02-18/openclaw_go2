#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  cat <<'TXT'
用法:
/go2 status
/go2 stand-up
/go2 stop
/go2 sit
/go2 rise-sit
/go2 recover-stand
/go2 move --vx 0.2 --vy 0 --vyaw 0 --duration 1.0
TXT
  exit 2
fi

export GO2_INTERFACE="${GO2_INTERFACE:-end0}"
export AMENT_TRACE_SETUP_FILES="${AMENT_TRACE_SETUP_FILES:-}"
export AMENT_PYTHON_EXECUTABLE="${AMENT_PYTHON_EXECUTABLE:-}"
set +u
source /home/radxa/unitree_ros2-master/unitree_ros2-master/setup_jazzy.sh
source /home/radxa/go2_bridge_ros2/install/setup.bash
set -u

set +e
RAW_OUTPUT=$(timeout 20s /home/radxa/go2_bridge_ros2/install/go2_bridge_nodes/lib/go2_bridge_nodes/go2_command_client "$@" 2>&1)
RC=$?
set -e

FILTERED=$(printf '%s\n' "$RAW_OUTPUT" | grep -v 'Failed to parse type hash' | grep -v '^Setup unitree ros2 jazzy environment$' | sed '/^$/d' || true)

if [[ -z "$FILTERED" ]]; then
  FILTERED="$RAW_OUTPUT"
fi

printf '%s\n' "$FILTERED"
exit $RC
