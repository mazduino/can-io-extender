#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

NODE=${1:-0}
PORT=${2:-}
CRYSTAL=${CRYSTAL:-8mhz}

if [[ ! "$NODE" =~ ^[0-3]$ ]]; then
  echo "Node harus 0-3"
  exit 1
fi

case "$CRYSTAL" in
  8mhz)  ENV=megaatmega2560 ;;
  16mhz) ENV=megaatmega2560-16mhz ;;
  *) echo "CRYSTAL harus 8mhz atau 16mhz"; exit 1 ;;
esac

PIO=$(command -v pio || true)
[[ -z "$PIO" && -x "$HOME/.platformio/penv/bin/pio" ]] && PIO="$HOME/.platformio/penv/bin/pio"
if [[ -z "$PIO" ]]; then
  echo "PlatformIO tidak ditemukan. Pasang: pip install platformio"
  exit 1
fi

echo "Build $ENV node $NODE${PORT:+, port $PORT}"
read -rp "Lepas 12 V dari modul, lalu tekan Enter untuk mulai..."

ARGS=(run -e "$ENV" -t upload)
[[ -n "$PORT" ]] && ARGS+=(--upload-port "$PORT")
PLATFORMIO_BUILD_FLAGS="-D NODE_ID=$NODE" "$PIO" "${ARGS[@]}"

echo "Selesai. Cabut USB, lalu sambungkan lagi 12 V."
