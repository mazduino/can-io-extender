#!/usr/bin/env bash
set -euo pipefail

REPO=mazduino/can-io-extender
PORT=${1:-}
TARGET=${2:-0}
CRYSTAL=${CRYSTAL:-8mhz}

if ! command -v avrdude >/dev/null; then
  echo "avrdude tidak ditemukan."
  echo "  macOS : brew install avrdude"
  echo "  Linux : sudo apt install avrdude"
  exit 1
fi

if [[ "$TARGET" == *.hex ]]; then
  HEX=$TARGET
elif [[ "$TARGET" =~ ^[0-3]$ ]]; then
  HEX="${TMPDIR:-/tmp}/can-io-extender-$CRYSTAL-node$TARGET.hex"
  echo "Mengunduh firmware node $TARGET..."
  curl -fsSL -o "$HEX" "https://github.com/$REPO/releases/latest/download/can-io-extender-$CRYSTAL-node$TARGET.hex"
else
  echo "Node harus 0-3, atau path ke file .hex"
  exit 1
fi

if [[ -z "$PORT" ]]; then
  shopt -s nullglob
  PORTS=(/dev/cu.usbserial* /dev/cu.usbmodem* /dev/cu.wchusbserial* /dev/ttyUSB* /dev/ttyACM*)
  if (( ${#PORTS[@]} == 1 )); then
    PORT=${PORTS[0]}
  else
    echo "Port tidak bisa dipilih otomatis. Port yang ada:"
    printf '  %s\n' "${PORTS[@]:-(tidak ada)}"
    echo "Jalankan: $0 <port> [node 0-3 | file.hex]"
    exit 1
  fi
fi

echo "Port: $PORT"
echo "File: $HEX"
read -rp "Lepas 12 V dari modul, lalu tekan Enter untuk mulai flash..."

avrdude -p m2560 -c wiring -P "$PORT" -b 115200 -D -U "flash:w:$HEX:i"
echo "Selesai. Cabut USB, lalu sambungkan lagi 12 V."
