#!/usr/bin/env python3
"""Generate the editable vector wiring diagram; uses only Python's standard library.

Run from any directory: python3 tools/generate_wiring.py
Pin labels describe electrical connections, not physical header positions.
"""
from html import escape
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "wiring.svg"
OUT.parent.mkdir(parents=True, exist_ok=True)

INK = "#182c3d"
MUTED = "#536575"
RED = "#bd343b"
GND = "#344454"
SDA = "#176bb3"
SCL = "#9c6c04"
UART = "#7650ab"
PALE = "#f6f8fa"
parts = [f'''<svg xmlns="http://www.w3.org/2000/svg" width="1800" height="1380" viewBox="0 0 1800 1380" role="img" aria-labelledby="title description">
<title id="title">ESP32-C6 four-sensor demonstration wiring</title>
<desc id="description">Three light sensors share I2C on GPIO23 SDA and GPIO22 SCL. All four sensors receive 3.3 volts at VIN and common ground. BNO085 uses UART-RVC: SDA or TX to GPIO20 RX, P0 to 3.3 volts, P1 to ground. Boxes use logical pin names, not physical pin positions.</desc>
<defs><marker id="rxArrow" viewBox="0 0 12 12" refX="10" refY="6" markerWidth="4" markerHeight="4" orient="auto"><path d="M 0 0 L 12 6 L 0 12 Z" fill="{UART}"/></marker></defs>
<rect width="1800" height="1380" fill="white"/>
<g font-family="Arial, Helvetica, sans-serif" fill="{INK}">
''']


def text(x, y, value, size=22, color=INK, weight=400, anchor="start"):
    parts.append(f'<text x="{x}" y="{y}" font-size="{size}" fill="{color}" font-weight="{weight}" text-anchor="{anchor}">{escape(value)}</text>')


def rect(x, y, width, height, fill="white", stroke="#cfdae3", radius=15):
    parts.append(f'<rect x="{x}" y="{y}" width="{width}" height="{height}" rx="{radius}" fill="{fill}" stroke="{stroke}" stroke-width="2"/>')


def line(x1, y1, x2, y2, color, width=4, extra=""):
    parts.append(f'<path d="M{x1},{y1} L{x2},{y2}" fill="none" stroke="{color}" stroke-width="{width}" stroke-linecap="round" {extra}/>')


def dot(x, y, color, radius=6):
    parts.append(f'<circle cx="{x}" cy="{y}" r="{radius}" fill="{color}"/>')


def pin(x, y, color):
    parts.append(f'<circle cx="{x}" cy="{y}" r="6" fill="white" stroke="{color}" stroke-width="3"/>')


def bus_drop(x, y1, y2, crossing_ys, color):
    """Arched crossing with a white underlay: no electrical connection."""
    d = f"M{x},{y1}"
    for cy in crossing_ys:
        if y1 < cy < y2:
            d += f" L{x},{cy - 9} C{x + 12},{cy - 9} {x + 12},{cy + 9} {x},{cy + 9}"
    d += f" L{x},{y2}"
    parts.append(f'<path d="{d}" fill="none" stroke="white" stroke-width="9"/>')
    parts.append(f'<path d="{d}" fill="none" stroke="{color}" stroke-width="3.5"/>')
    pin(x, y1, color)
    dot(x, y2, color)


text(60, 67, "ESP32-C6  /  four-sensor demo", 42, weight=700)
text(60, 108, "TCS3448 + TSL2585 + BH1750 on I2C  •  BNO085 on UART-RVC", 24, MUTED)
rect(60, 135, 1680, 58, "#eef4f8", "#dbe5ec", 10)
text(82, 172, "Assumed board: ESP32-C6-DevKitM-1. GPIO numbers are chip GPIO labels, not header positions.", 22, weight=600)

text(60, 240, "01", 26, SDA, 700)
text(115, 240, "Shared I2C bus", 27, weight=700)
text(1740, 240, "Dots = joined wires. Arched crossings = separate wires.", 19, MUTED, anchor="end")

boxes = [
    (80, "ESP32-C6", "Development board", "USB powers the board", ["3V3", "GND", "23 / SDA", "22 / SCL"]),
    (520, "TCS3448", "Adafruit #6525", "I2C address 0x59", ["VIN", "GND", "SDA", "SCL"]),
    (960, "TSL2585", "Adafruit #6524", "I2C address 0x39", ["VIN", "GND", "SDA", "SCL"]),
    (1400, "BH1750", "Adafruit #4681", "I2C address 0x23", ["VIN", "GND", "SDA", "SCL"]),
]
ys = [470, 515, 560, 605]
colors = [RED, GND, SDA, SCL]
for y, color, label in zip(ys, colors, ["3V3", "GND", "SDA", "SCL"]):
    text(74, y + 7, label, 19, color, 700, "end")
    line(96, y, 1720, y, color)
for x, title, subtitle, detail, labels in boxes:
    rect(x, 275, 320, 145, PALE)
    text(x + 160, 313, title, 29, weight=700, anchor="middle")
    text(x + 160, 342, subtitle, 20, MUTED, anchor="middle")
    text(x + 160, 371, detail, 19, MUTED, anchor="middle")
    for index, (dx, label, color, target_y) in enumerate(zip([40, 117, 200, 279], labels, colors, ys)):
        text(x + dx, 405, label, 17, color, 700, "middle")
        bus_drop(x + dx, 420, target_y, ys[:index], color)

text(80, 646, "All three light sensors share these four wires. STEMMA QT daisy-chaining is optional for these three only.", 21, MUTED)

line(60, 682, 1740, 682, "#dbe3e9", 2)
text(60, 732, "02", 26, UART, 700)
text(115, 732, "BNO085 UART-RVC", 27, weight=700)
text(1740, 732, "115200 baud  •  8 data bits, no parity, 1 stop bit", 20, MUTED, anchor="end")

rect(80, 765, 460, 365, PALE)
rect(1120, 765, 600, 365, PALE)
text(110, 807, "ESP32-C6", 30, weight=700)
text(110, 837, "Same development board as above", 21, MUTED)
text(1150, 807, "BNO085  /  Adafruit #4754", 29, weight=700)
text(1150, 837, "Mode selectors: P0 HIGH, P1 LOW", 22, UART, 600)

# Power rails split outside the BNO085 board, with explicit junction dots.
text(514, 885, "3V3", 23, RED, 700, "end")
line(540, 878, 1120, 878, RED)
line(820, 878, 820, 923, RED)
line(820, 923, 1120, 923, RED)
dot(820, 878, RED)
text(1142, 885, "VIN", 23, RED, 700)
text(1142, 930, "P0 / PS0", 23, RED, 700)

text(514, 985, "GND", 23, GND, 700, "end")
line(540, 978, 1120, 978, GND)
line(820, 978, 820, 1023, GND)
line(820, 1023, 1120, 1023, GND)
dot(820, 978, GND)
text(1142, 985, "GND", 23, GND, 700)
text(1142, 1030, "P1 / PS1", 23, GND, 700)

text(514, 1090, "GPIO20 / RX", 23, UART, 700, "end")
line(1120, 1083, 542, 1083, UART, extra='marker-end="url(#rxArrow)"')
text(1142, 1090, "SDA / TX (sensor output)", 23, UART, 700)
text(830, 1066, "Orientation + acceleration", 20, UART, anchor="middle")
for x, y, color in [(540, 878, RED), (540, 978, GND), (540, 1083, UART), (1120, 878, RED), (1120, 923, RED), (1120, 978, GND), (1120, 1023, GND), (1120, 1083, UART)]:
    pin(x, y, color)

rect(60, 1160, 1680, 165, "#f8fafb", "#dbe3e9", 10)
text(82, 1193, "Connection notes", 23, weight=700)
text(82, 1227, "Use 3.3 V at every sensor VIN and one common GND. Leave each sensor's regulator-output pin unconnected.", 21, MUTED)
text(82, 1258, "BNO085: leave SCL/RX, DI, CS, INT, RST and BT open; do not connect its STEMMA QT socket to the I2C bus.", 21, MUTED)
text(82, 1289, "Keep P1 jumper open; set P0/P1 with power off, then power-cycle. Keep light-sensor extra pins / address at defaults.", 21, MUTED)
text(60, 1358, "Logical wiring diagram • Pin order is illustrative • See README.md for setup, pin table and official sources", 18, MUTED)
parts.append("</g></svg>\n")
OUT.write_text("\n".join(parts), encoding="utf-8")
print(OUT)
