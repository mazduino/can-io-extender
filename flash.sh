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

port_label() {
  local vid pid name
  vid=$(echo "$1" | tr 'A-F' 'a-f'); pid=$(echo "$2" | tr 'A-F' 'a-f'); name="$3"
  case "$vid:$pid" in
    2341:0042|2341:0010|2341:0242|2a03:0042|2a03:0010) echo "Arduino Mega 2560"; return ;;
  esac
  case "$vid" in
    2341|2a03) echo "Arduino ${name}" ;;
    1a86) echo "CH340 USB serial (Mega clone)" ;;
    0403) echo "FTDI USB serial" ;;
    10c4) echo "CP210x USB serial" ;;
    *) echo "${name:-USB serial}" ;;
  esac
}

scan_ports() {
  PORTS=(); LABELS=()
  local path vid pid name
  if [ "$(uname -s)" = Darwin ]; then
    while IFS='|' read -r path vid pid name; do
      PORTS+=("$path"); LABELS+=("$(port_label "$vid" "$pid" "$name")")
    done < <(ioreg -p IOService -l -w0 | awk '
      /"USB Product Name" = /{split($0,a,"= \""); n=a[2]; sub(/"$/,"",n)}
      /"idVendor" = /{split($0,a,"= "); v=sprintf("%04x",a[2])}
      /"idProduct" = /{split($0,a,"= "); p=sprintf("%04x",a[2])}
      /"IOCalloutDevice" = /{split($0,a,"= \""); d=a[2]; sub(/"$/,"",d);
        if (d ~ /usb|wchusb|SLAB/) print d "|" v "|" p "|" n}')
  else
    for path in /dev/ttyACM* /dev/ttyUSB*; do
      [ -e "$path" ] || continue
      local dir; dir=$(readlink -f "/sys/class/tty/$(basename "$path")/device")
      vid=""; pid=""; name=""
      while [ -n "$dir" ] && [ "$dir" != / ]; do
        if [ -f "$dir/idVendor" ]; then
          vid=$(cat "$dir/idVendor"); pid=$(cat "$dir/idProduct"); name=$(cat "$dir/product" 2>/dev/null)
          break
        fi
        dir=$(dirname "$dir")
      done
      PORTS+=("$path"); LABELS+=("$(port_label "$vid" "$pid" "$name")")
    done
  fi
}

choose_port() {
  while true; do
    scan_ports
    if [ ${#PORTS[@]} -eq 0 ]; then
      read -r -p "No USB serial port found. Check the USB cable, then press Enter to scan again." _
      continue
    fi
    local i def=1
    for i in "${!LABELS[@]}"; do
      case "${LABELS[$i]}" in "Arduino Mega 2560"*|CH340*) def=$((i + 1)); break ;; esac
    done
    echo "Serial ports:"
    for i in "${!PORTS[@]}"; do
      printf "  %d) %-28s %s\n" $((i + 1)) "${PORTS[$i]}" "${LABELS[$i]}"
    done
    echo "  r) scan again"
    read -r -p "Choose port [$def]: " pick
    pick="${pick:-$def}"
    [ "$pick" = r ] && continue
    if [[ "$pick" =~ ^[0-9]+$ ]] && [ "$pick" -ge 1 ] && [ "$pick" -le ${#PORTS[@]} ]; then
      PORT="${PORTS[$((pick - 1))]}"
      return
    fi
    echo "Not in the list."
  done
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
    find tools/avrdude -exec xattr -d com.apple.quarantine {} + 2>/dev/null || true
    if xattr "$dir/bin/avrdude" 2>/dev/null | grep -q com.apple.quarantine; then
      echo "macOS still blocks avrdude. Run this once, then start again:"
      echo "  xattr -d com.apple.quarantine \"$PWD/$dir/bin/avrdude\""
      exit 1
    fi
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

[ -z "$PORT" ] && choose_port
[ -z "$NODE" ] && read -r -p "Node 0-3 [0]: " NODE
NODE="${NODE:-0}"
case "$NODE" in 0|1|2|3) ;; *) echo "Node must be 0-3."; exit 1 ;; esac

HEX="hex/can-io-extender-${CRYSTAL}-node${NODE}.hex"
[ -f "$HEX" ] || { echo "$HEX not found."; exit 1; }

echo
echo "File: $HEX"
echo "Port: $PORT"
read -r -p "Disconnect 12 V from the module; keep only USB connected. Press Enter..." _

run_avrdude() {
  local args=(-p atmega2560 -c "$1" -P "$PORT" -b 115200 -D -U "flash:w:${HEX}:i")
  [ -n "$CONF" ] && args=(-C "$CONF" "${args[@]}")
  "$AVRDUDE" "${args[@]}"
}

flashed=0
for attempt in 1 2 3; do
  [ $attempt -gt 1 ] && { echo; echo "No sync, retrying ($attempt/3)..."; sleep 1; }
  if run_avrdude wiring; then flashed=1; break; fi
done

if [ $flashed -eq 0 ]; then
  echo
  echo "Still no sync. Manual reset:"
  read -r -p "Press Enter, then press RESET on the Mega right away..." _
  run_avrdude stk500v2 && flashed=1
fi

if [ $flashed -eq 0 ]; then
  echo
  echo "Flash failed. Check the USB cable and that no other program (TunerStudio,"
  echo "Arduino IDE, a serial monitor) has the port open."
  [ "$(uname -s)" = Linux ] && echo "On Linux, also check you are in the dialout group: sudo usermod -aG dialout \$USER"
  exit 1
fi

echo
echo "Done. Unplug USB, then reconnect 12 V."
