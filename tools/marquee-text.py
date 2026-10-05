#!/usr/bin/env python3
"""Set text on a Pico W marquee over the LAN.

Talks to the TCP command listener the firmware opens on port 4242 and sends
the ASCII text command documented in README.org:

    t<x>,<y>,<rrggbb>[,<rrggbb>]:<text>

Examples:
    marquee-text.py -H 192.168.1.50 "HELLO WORLD"
    marquee-text.py -H marquee.lan -x 0 -y 8 -c 00ff00 -b 000040 "12:30"
    marquee-text.py -H marquee.lan --clear "ONLY THIS LINE"
    marquee-text.py -H marquee.lan --clear          # blank the display
    marquee-text.py -H marquee.lan --clock          # set time, show clock
    marquee-text.py -H marquee.lan --scroll -c cyan "HELLO FROM THE C64 "
"""

import argparse
import os
import socket
import sys
import time

DEFAULT_PORT = 4242
MAX_LEN = 32
SCROLL_MAX_LEN = 128
PANEL_WIDTH = 128
PANEL_HEIGHT = 32

COLOURS = {
    "white": "ffffff",
    "red": "ff0000",
    "green": "00ff00",
    "blue": "0000ff",
    "yellow": "ffff00",
    "cyan": "00ffff",
    "magenta": "ff00ff",
    "orange": "ff8800",
    "black": "000000",
}


def colour(value):
    value = value.strip().lower()
    value = COLOURS.get(value, value).lstrip("#")
    if len(value) != 6 or any(c not in "0123456789abcdef" for c in value):
        raise argparse.ArgumentTypeError(
            f"'{value}' is not rrggbb hex or one of: {', '.join(COLOURS)}"
        )
    return value


def send(host, port, payload, timeout):
    # The firmware dispatches each received TCP segment as one command, and
    # only the first command in a segment is handled. A connection per command
    # guarantees the commands are never coalesced into a single segment.
    with socket.create_connection((host, port), timeout=timeout) as sock:
        sock.sendall(payload)
        sock.shutdown(socket.SHUT_WR)
        # Give lwIP a moment to deliver the segment before the close.
        time.sleep(0.05)


def main():
    parser = argparse.ArgumentParser(
        description="Set text on the Pico W LED marquee over TCP.",
        epilog="The font is a C64 charset: uppercase, digits and common "
        "punctuation. Lowercase is shown as uppercase.",
    )
    parser.add_argument(
        "text", nargs="?", help="text to show (max 32 chars, 128 with --scroll)"
    )
    parser.add_argument(
        "-H",
        "--host",
        default=os.environ.get("MARQUEE_HOST"),
        help="marquee IP or hostname (default: $MARQUEE_HOST)",
    )
    parser.add_argument(
        "-p", "--port", type=int, default=DEFAULT_PORT, help="TCP port (default: 4242)"
    )
    parser.add_argument("-x", type=int, default=0, help="x position in pixels (default: 0)")
    parser.add_argument("-y", type=int, default=0, help="y position in pixels (default: 0)")
    parser.add_argument(
        "-c",
        "--color",
        type=colour,
        help="foreground colour, rrggbb or name (default: white; with --scroll, "
        "the scroller's current colour)",
    )
    parser.add_argument(
        "-b",
        "--background",
        type=colour,
        help="background colour; omit for transparent text",
    )
    parser.add_argument(
        "--clear",
        action="store_true",
        help="clear all stored text lines before setting the new one",
    )
    parser.add_argument(
        "--once",
        action="store_true",
        help="draw into a single frame instead of keeping it in text mode",
    )
    parser.add_argument(
        "--scroll",
        action="store_true",
        help="show the text in the sine scroller; without text, restart the scroller",
    )
    parser.add_argument(
        "--clock",
        action="store_true",
        help="set the marquee clock from this machine and show the matrix clock",
    )
    parser.add_argument(
        "--timeout", type=float, default=5.0, help="connect timeout in seconds (default: 5)"
    )
    args = parser.parse_args()

    if not args.host:
        parser.error("no host given; use -H or set MARQUEE_HOST")
    if args.text is None and not (args.clear or args.clock or args.scroll):
        parser.error("nothing to do; give some text, --clear, --clock or --scroll")
    max_len = SCROLL_MAX_LEN if args.scroll else MAX_LEN

    if args.text is not None:
        if any(c in args.text for c in "\r\n"):
            parser.error("text must be a single line")
        if len(args.text) > max_len:
            parser.error(f"text is {len(args.text)} characters; the limit is {max_len}")
        try:
            args.text.encode("ascii")
        except UnicodeEncodeError:
            parser.error("text must be plain ASCII")
        if not args.scroll and not (-PANEL_WIDTH < args.x < PANEL_WIDTH and -PANEL_HEIGHT < args.y < PANEL_HEIGHT):
            print("warning: position is entirely off the panel", file=sys.stderr)

    commands = []
    if args.clear:
        commands.append(b"C")
    if args.scroll:
        # Without text this just re-colours and restarts the current text.
        commands.append(f"r{args.color or ''}:{args.text or ''}\n".encode("ascii"))
    elif args.text is not None:
        header = f"{args.x},{args.y},{args.color or 'ffffff'}"
        if args.background:
            header += f",{args.background}"
        cmd = "o" if args.once else "t"
        commands.append(f"{cmd}{header}:{args.text}\n".encode("ascii"))

    if args.clock:
        # The firmware keeps UTC and applies the time zone itself.
        commands.append(f"k{time.time():.3f}\n".encode("ascii"))
        commands.append(b"M")

    try:
        for payload in commands:
            send(args.host, args.port, payload, args.timeout)
    except OSError as exc:
        print(f"error: could not reach {args.host}:{args.port}: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
