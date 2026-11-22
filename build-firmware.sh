#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
output="${1:-$repo_dir/pio_hub75.uf2}"

echo "Select the Pico variant:"
select variant in pico pico_w pico2 pico2_w; do
  if [[ -n "${variant:-}" ]]; then
    break
  fi
  echo "Please choose 1, 2, 3, or 4."
done

wifi=0
case "$variant" in
  pico_w|pico2_w)
    wifi=1
    ;;
esac

if (( wifi )); then
  read -r -p "WiFi SSID: " wifi_ssid
  read -r -s -p "WiFi password: " wifi_password
  printf '\n'
  read -r -p "MQTT broker for Home Assistant (blank to disable): " mqtt_host
  if [[ -n "$mqtt_host" ]]; then
    read -r -p "MQTT port [1883]: " mqtt_port
    read -r -p "MQTT user: " mqtt_user
    read -r -s -p "MQTT password: " mqtt_password
    printf '\n'
  fi
fi
wifi_ssid="${wifi_ssid:-}"
wifi_password="${wifi_password:-}"
mqtt_host="${mqtt_host:-}"
mqtt_port="${mqtt_port:-1883}"
mqtt_user="${mqtt_user:-}"
mqtt_password="${mqtt_password:-}"

build_dir="$(mktemp -d "${TMPDIR:-/tmp}/pico-marquee-build.XXXXXX")"
cleanup() {
  unset wifi_ssid wifi_password mqtt_user mqtt_password
  rm -rf -- "$build_dir"
}
trap cleanup EXIT

echo "Building $variant..."
WIFI_SSID="$wifi_ssid" WIFI_PASSWORD="$wifi_password" \
  MQTT_HOST="$mqtt_host" MQTT_PORT="$mqtt_port" \
  MQTT_USER="$mqtt_user" MQTT_PASSWORD="$mqtt_password" \
  cmake -S "$repo_dir" -B "$build_dir" -DPICO_BOARD="$variant"
cmake --build "$build_dir" -j"$(nproc)"

mkdir -p -- "$(dirname -- "$output")"
cp -- "$build_dir/pio_hub75.uf2" "$output"
echo "Wrote $output"
