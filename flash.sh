#!/usr/bin/env bash
cd "$(dirname "$0")"

finish() {
  status=$?
  if [ -t 0 ]; then
    echo
    read -r -p "Press Enter to close." _
  fi
  exit $status
}
trap finish EXIT
set -e

NODE="${1:-}"
PORT="${2:-}"
CRYSTAL="${CRYSTAL:-8mhz}"

case "$CRYSTAL" in
  8mhz)  ENV=megaatmega2560 ;;
  16mhz) ENV=megaatmega2560-16mhz ;;
  *) echo "CRYSTAL must be 8mhz or 16mhz."; exit 1 ;;
esac

list_ports() {
  echo "Serial ports found:"
  ls /dev/cu.usb* /dev/cu.wchusb* /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | sed 's/^/  /' || echo "  none"
  echo
}

bundled_avrdude() {
  local dir=""
  case "$(uname -s)-$(uname -m)" in
    Darwin-*)         dir=tools/avrdude/macos ;;
    Linux-x86_64)     dir=tools/avrdude/linux-x64 ;;
    Linux-aarch64)    dir=tools/avrdude/linux-arm64 ;;
  esac
  [ -n "$dir" ] && [ -x "$dir/bin/avrdude" ] || return 1
  if [ "$(uname -s)" = Darwin ]; then
    xattr -dr com.apple.quarantine tools/avrdude 2>/dev/null || true
    if [ "$(uname -m)" = arm64 ] && ! arch -x86_64 /usr/bin/true 2>/dev/null; then
      echo "This Mac needs Rosetta to run avrdude. Install it once with:"
      echo "  softwareupdate --install-rosetta --agree-to-license"
      exit 1
    fi
  fi
  AVRDUDE="$dir/bin/avrdude"
  CONF="$dir/etc/avrdude.conf"
}

echo "Mazduino CAN IO Extender - firmware flash"
echo

if [ -f platformio.ini ]; then
  PIO=$(command -v pio || true)
  [ -z "$PIO" ] && [ -x "$HOME/.platformio/penv/bin/pio" ] && PIO="$HOME/.platformio/penv/bin/pio"
  [ -z "$PIO" ] && { echo "PlatformIO not found: pip install platformio"; exit 1; }
  NODE="${NODE:-0}"
  case "$NODE" in 0|1|2|3) ;; *) echo "Node must be 0-3."; exit 1 ;; esac
  echo "Build $ENV, node $NODE${PORT:+, port $PORT}"
  read -r -p "Disconnect 12 V from the module; keep only USB connected. Press Enter..." _
  ARGS=(run -e "$ENV" -t upload)
  [ -n "$PORT" ] && ARGS+=(--upload-port "$PORT")
  PLATFORMIO_BUILD_FLAGS="-D NODE_ID=$NODE" "$PIO" "${ARGS[@]}"
  echo
  echo "Done. Unplug USB, then reconnect 12 V."
  exit 0
fi

AVRDUDE=""
CONF=""
bundled_avrdude || AVRDUDE=$(command -v avrdude || true)
[ -z "$AVRDUDE" ] && { echo "avrdude not found. Extract the whole zip again."; exit 1; }

list_ports
[ -z "$PORT" ] && read -r -p "Port (e.g. /dev/cu.usbmodem1101): " PORT
[ -z "$PORT" ] && { echo "No port given."; exit 1; }
[ -z "$NODE" ] && read -r -p "Node 0-3 [0]: " NODE
NODE="${NODE:-0}"
case "$NODE" in 0|1|2|3) ;; *) echo "Node must be 0-3."; exit 1 ;; esac

HEX="hex/can-io-extender-${CRYSTAL}-node${NODE}.hex"
[ -f "$HEX" ] || { echo "$HEX not found."; exit 1; }

echo
echo "File: $HEX"
echo "Port: $PORT"
read -r -p "Disconnect 12 V from the module; keep only USB connected. Press Enter..." _

ARGS=(-p atmega2560 -c wiring -P "$PORT" -b 115200 -D -U "flash:w:${HEX}:i")
[ -n "$CONF" ] && ARGS=(-C "$CONF" "${ARGS[@]}")
if ! "$AVRDUDE" "${ARGS[@]}"; then
  echo
  echo "No sync. Press RESET on the Mega just as avrdude starts, then try again."
  [ "$(uname -s)" = Linux ] && echo "On Linux, also check you are in the dialout group: sudo usermod -aG dialout \$USER"
  exit 1
fi

echo
echo "Done. Unplug USB, then reconnect 12 V."
