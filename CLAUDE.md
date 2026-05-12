# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

Firmware and tooling for a 128×32 RGB LED matrix display (two 64×32 HUB75 panels in series) driven by a Raspberry Pi Pico. The Pico exposes a WebUSB interface so a browser can push frames or trigger on-board demos.

## Building the Firmware

Requires the Pico SDK and ARM toolchain. Standard CMake out-of-source build:

```bash
mkdir build && cd build
cmake ..
make
```

Flash by copying the resulting `.uf2` to the Pico in BOOTSEL mode, or send `B` over WebUSB/CDC serial to reboot into BOOTSEL.

Install the ARM toolchain (Arch/CachyOS):
```bash
sudo pacman -S cmake arm-none-eabi-gcc arm-none-eabi-newlib base-devel
```

## Web Apps

Two separate Vite/Svelte apps under `web/`:

| App | Path | Purpose |
|-----|------|---------|
| WebUSB controller | `web/webusb/` | Connect to device, send frames, trigger reboot |
| Demo preview | `web/demo/` | Preview animations in-browser without hardware |

Run either with:
```bash
cd web/webusb   # or web/demo
npm install
npm run dev     # dev server at localhost:5173
npm run build
npm run check   # svelte-check type checking (demo only)
npm run lint
```

## Architecture

### Firmware layers

- **`hub75.pio` / `hub75.c`** — Low-level HUB75 LED driver. The PIO program clocks out RGB888 pixel data; `core1_main()` runs on the second core continuously scanning rows from the display buffer, keeping the display refreshed without blocking the main loop.
- **`pixels.c` / `pixels.h`** — Double-buffered framebuffer (`WIDTH=128`, `HEIGHT=32`, RGB packed as 3 bytes per pixel). One extra hidden row (row 32) exists in the allocation and is used by the fire demo as a seed row. `flip_buffer(copy)` swaps render/display buffers; pass `copy=true` if the next frame builds on the previous one.
- **`demos.c` / `demos/snek.c`** — On-board demo dispatcher. `select_demo(n)` switches demos; `render_demo()` is called each frame from `render_task()`. Demo 0 = bouncing dot (default), 1 = fire, 2 = Snek game.
- **`webusb_main.c`** — Main loop on `core0`. Handles TinyUSB device tasks, CDC serial, WebUSB vendor class, LED blink, and 25 fps render tick (`FRAME_TIME = 40ms`).
- **`c64.h`** — C64 bitmap font data used for text rendering.
- **`usb_descriptors.c`** — TinyUSB descriptor definitions. Vendor ID is `0xcafe`.

### USB command protocol

Single-byte commands (or multi-byte for pixel push) over WebUSB or CDC serial:

| Byte | Command |
|------|---------|
| `B` (0x42) | Reboot to BOOTSEL |
| `F` (0x46) | Start fire demo |
| `S` | Start Snek game |
| `P` (0x50) | Push pixels: `[P, x, y, n, r,g,b × n]` — writes `n` pixels (max 16) starting at (x,y) |
| `w/a/s/d` | Snek direction |
| `p` | Pause Snek |
| `n` | New Snek game |

Packets are capped at 64 bytes (USB bulk packet limit), which is why pixel push sends 16 pixels at a time.

### Web / TypeScript

- `web/webusb/src/lib/hub75.ts` — `Pico75` class wrapping the WebUSB API: `connect()`, `disconnect()`, `reboot()`, `sendFrame()`.
- `web/demo/src/lib/pixels.ts` — Browser-side framebuffer matching the firmware layout.
- `web/demo/src/lib/demo.ts` — Browser demo logic (fire, scrollers, blocks).
- `web/demo/src/lib/c64text.ts` / `text.ts` — Font rendering in TypeScript mirroring `c64.h`.
