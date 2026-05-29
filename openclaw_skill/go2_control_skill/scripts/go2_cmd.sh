#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SKILL_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
BIN="$SKILL_DIR/build/bin/go2_controller"
LOG_DIR="$SKILL_DIR/logs"
LOG_FILE="$LOG_DIR/go2_controller.log"
DDS_XML="$LOG_DIR/cyclonedds.xml"

resolve_interface() {
  if [[ -n "${GO2_INTERFACE:-}" ]]; then
    printf '%s\n' "$GO2_INTERFACE"
    return
  fi

  local default_iface
  default_iface=$(ip route show default 2>/dev/null | awk '/default/ { print $5; exit }')
  if [[ -n "$default_iface" ]]; then
    printf '%s\n' "$default_iface"
    return
  fi

  local candidate
  for candidate in wlp1s0 end0 end1; do
    if ip link show "$candidate" >/dev/null 2>&1; then
      printf '%s\n' "$candidate"
      return
    fi
  done

  printf 'end0\n'
}

resolve_interface_ipv4() {
  local iface=$1
  ip -4 -o addr show dev "$iface" | awk '{print $4}' | cut -d/ -f1 | head -n1
}

if [[ ! -x "$BIN" ]]; then
  "$SCRIPT_DIR/build_go2_bridge.sh"
fi

mkdir -p "$LOG_DIR"
INTERFACE=$(resolve_interface)
INTERFACE_IP=$(resolve_interface_ipv4 "$INTERFACE")
CONTROLLER_INTERFACE="$INTERFACE"

if [[ -n "$INTERFACE_IP" ]]; then
  cat > "$DDS_XML" <<XML
<CycloneDDS>
  <Domain Id="any">
    <General>
      <Interfaces>
        <NetworkInterface address="$INTERFACE_IP" />
      </Interfaces>
    </General>
  </Domain>
</CycloneDDS>
XML
  export CYCLONEDDS_URI="file://$DDS_XML"
  CONTROLLER_INTERFACE="auto"
fi

{
  echo "===== $(date -Iseconds) ====="
  echo "BIN=$BIN"
  echo "INTERFACE=$INTERFACE"
  echo "INTERFACE_IP=$INTERFACE_IP"
  echo "CONTROLLER_INTERFACE=$CONTROLLER_INTERFACE"
  echo "CYCLONEDDS_URI=${CYCLONEDDS_URI:-}"
  echo "ARGS=$*"
} >> "$LOG_FILE"

exec "$BIN" --interface "$CONTROLLER_INTERFACE" "$@" >> "$LOG_FILE" 2>&1
