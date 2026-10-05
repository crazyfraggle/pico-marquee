# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

Firmware and tooling for a 128×32 RGB LED matrix display (two 64×32 HUB75 panels in series) driven by a Raspberry Pi Pico. The Pico exposes a WebUSB interface so a browser can push frames or trigger on-board demos.

## Building the Firmware

Requires the Pico SDK and ARM toolchain. Standard CMake out-of-source build:

```bash
git submodule update --init --recursive
./build-firmware.sh
```

Flash by copying the resulting `.uf2` to the Pico in BOOTSEL mode, or send `B` over WebUSB/CDC serial to reboot into BOOTSEL.

`build-firmware.sh` asks for the Pico variant and, for Pico W variants, the
WiFi credentials. It passes credentials through the environment and removes
the temporary CMake build directory when it exits. The credentials are still
necessarily embedded in the resulting firmware.

The repository pins the Pico SDK as the `pico-sdk` submodule. Install the ARM
toolchain (Arch/CachyOS):
```bash
sudo pacman -S --needed base-devel cmake git python \
  arm-none-eabi-binutils arm-none-eabi-gcc arm-none-eabi-newlib
```

### Checking a build non-interactively

`build-firmware.sh` prompts, so it is awkward for automated checks. To verify
that both variants still compile, drive CMake directly — always check `pico_w`
*and* `pico`, since a lot of code is conditional on WiFi support:

```bash
WIFI_SSID=x WIFI_PASSWORD=y cmake -S . -B /tmp/bw -DPICO_BOARD=pico_w -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/bw -j8
```

The build pulls picotool from source into each new build directory, so a fresh
directory takes a few minutes. The installed CMake does not accept
`--build --quiet`; redirect to a log and grep for `warning:|error:` instead.
The tree is expected to build with zero warnings.

### `PICO_CYW43_SUPPORTED` is not a compiler define

The SDK board headers declare it with `pico_board_cmake_set(...)`, which makes
it a **CMake variable only**. Any `#ifdef PICO_CYW43_SUPPORTED` in project
source silently takes the false branch unless the define is added explicitly,
which `CMakeLists.txt` now does inside the `if (PICO_CYW43_SUPPORTED)` block.
This previously compiled all WiFi support out of the Pico W build while the
CYW43 libraries were still being linked, so everything looked fine. To confirm
a change really landed, check the ELF rather than trusting the build to fail:

```bash
arm-none-eabi-nm /tmp/bw/pio_hub75.elf | grep connect_wifi
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
- **`demos.c` / `demos/snek.c`** — On-board demo dispatcher. `select_demo(n)` switches demos; `render_demo()` is called each frame from `render_task()`. Demo 0 = bouncing dot (default), 1 = fire, 2 = Snek game, 3 = bright white backlight, 4 = text mode, 5 = Matrix rain clock (`demos/matrix.c`). The `DEMO_*` enum in `demos.h` names the indices. Returning `true` means the demo painted the whole frame and `render()` skips the default renderer. Because `render_task()` calls `flip_buffer(true)` first, the buffer still holds the **previous** frame, so a demo that does not overwrite every pixel must clear it (as `render_textmode()` does) or it will accumulate.
- **`text.c` / `text.h`** — 8×8 font rendering and text mode. Owns the `c64.h` include, maps ASCII onto the C64 charmap, keeps up to `TEXT_MAX_LINES` strings for demo 4, and parses the ASCII text command form.
- **`clock.c` / `clock.h`** — Wall clock kept as an offset from `time_us_64()`. Set by lwIP SNTP on a Pico W (via `SNTP_SET_SYSTEM_TIME_US` in `lwipopts.h`, started from `wifi.c` once connected) or by the `k` command. Converts UTC to local time with `CLOCK_UTC_OFFSET_MIN` and the EU summer time rule (`CLOCK_EU_DST`), both CMake variables.
- **`webusb_main.c`** — Main loop on `core0`. Handles TinyUSB device tasks, CDC serial, WebUSB vendor class, LED blink, and 25 fps render tick (`FRAME_TIME = 40ms`).
- **`c64.h`** — C64 bitmap font data used for text rendering. Both arrays are `static` in the header, so **every** translation unit that includes it gets its own copy in RAM. Only `text.c` includes it; go through `text_draw_glyph()` instead of including it again.
- **`usb_descriptors.c`** — TinyUSB descriptor definitions. Vendor ID is `0xcafe`.

### Command protocol

`handle_input_buffer()` in `webusb_main.c` is the single dispatcher for **all
four transports**: WebUSB, USB CDC serial, the hardware UART, and the TCP
listener on port 4242. A command added there is immediately available
everywhere, and anything assuming a particular transport (packet sizes,
reply paths, how much arrives per call) has to hold for all of them. Note
that the TCP listener is unauthenticated: anything on the LAN can drive the
display.

Single-byte commands (or multi-byte for pixel push) over WebUSB or CDC serial:

| Byte | Command |
|------|---------|
| `B` (0x42) | Reboot to BOOTSEL |
| `F` (0x46) | Start fire demo |
| `S` | Start Snek game |
| `L` | Bright white backlight at full brightness |
| `L, brightness` | Bright white backlight with brightness `0–255` |
| `I` | Report WiFi status and current IP address |
| `W` | Retry the WiFi connection |
| `T` / `O` | Text, binary form: `[T, x, y, fr,fg,fb, br,bg,bb, opts, n, chars × n]` — `opts` bit 0 paints the background, `n` ≤ 32 |
| `t` / `o` | Text, ASCII form: `t<x>,<y>,<rrggbb>[,<rrggbb>]:<text>` terminated by newline |
| `C` | Clear all stored text lines |
| `M` | Matrix rain clock |
| `k` | Set clock, ASCII line: `k<unix seconds>[.fraction]` terminated by newline (UTC) |
| `P` (0x50) | Push pixels: `[P, x, y, n, r,g,b × n]` — writes `n` pixels (max 16) starting at (x,y) |
| `w/a/s/d` | Snek direction |
| `p` | Pause Snek |
| `n` | New Snek game |

Packets are capped at 64 bytes (USB bulk packet limit), which is why pixel push sends 16 pixels at a time.

Any multi-byte command **must validate `count` before reading past `buf[0]`**.
`uart_task()` calls the dispatcher with a single byte, so a partially received
command is the normal case rather than an error, and the handler is reached
with far less data than the format implies. This was a real out-of-bounds read
in the `P` command. If a command needs several bytes over the UART, give it an
ASCII line form instead.

Uppercase `T`/`t` store the line and switch to text mode (demo 4), which keeps up to 4 lines and redraws them every frame. Lines are keyed by `y`, so resending at the same `y` replaces that row and an empty string clears it. Lowercase `O`/`o` draw once into the frame currently being built, which only survives under demos that do not repaint the whole panel.

The `k` clock command uses the same line reassembly, so it works over the UART too.

The ASCII form splits the header from the text at the **first** colon, so the text may contain `,` and `:`. It is reassembled byte-by-byte in `webusb_main.c` (see `line_feed()`) because the UART delivers a single byte per call — multi-byte binary commands cannot be used over the UART for that reason.

### Web / TypeScript

- `tools/marquee-text.py` — stdlib-only Python CLI that sends the ASCII text command to a Pico W over TCP port 4242. Opens one connection per command because the firmware handles only the first command in each TCP segment.

- `web/webusb/src/lib/hub75.ts` — `Pico75` class wrapping the WebUSB API: `connect()`, `disconnect()`, `reboot()`, `sendFrame()`.
- `web/demo/src/lib/pixels.ts` — Browser-side framebuffer matching the firmware layout.
- `web/demo/src/lib/demo.ts` — Browser demo logic (fire, scrollers, blocks).
- `web/demo/src/lib/c64text.ts` / `text.ts` — Font rendering in TypeScript mirroring `c64.h`.

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

Rules:
- For codebase questions, first run `graphify query "<question>"` when graphify-out/graph.json exists. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
