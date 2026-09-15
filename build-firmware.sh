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
else
  wifi_ssid=''
  wifi_password=''
fi

build_dir="$(mktemp -d "${TMPDIR:-/tmp}/pico-marquee-build.XXXXXX")"
cleanup() {
  unset wifi_ssid wifi_password
  rm -rf -- "$build_dir"
}
trap cleanup EXIT

echo "Building $variant..."
WIFI_SSID="$wifi_ssid" WIFI_PASSWORD="$wifi_password" \
  cmake -S "$repo_dir" -B "$build_dir" -DPICO_BOARD="$variant"
cmake --build "$build_dir" -j"$(nproc)"

mkdir -p -- "$(dirname -- "$output")"
cp -- "$build_dir/pio_hub75.uf2" "$output"
echo "Wrote $output"
