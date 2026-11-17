#!/usr/bin/env python3
"""NodeMCU v3 (ESP8266) <-> Ai-Thinker Ra-02 (SX1278) bağlantı çizimini üretir.

Kullanım: python3 tools/wiring_diagram.py > hardware/nodemcu-v3-ra02-wiring.svg
"""

W, H = 1080, 810
FONT = "DejaVu Sans, Arial, sans-serif"

# NodeMCU v3 header'ları (anten üstte, USB altta)
LEFT_PINS = ["A0", "RSV", "RSV", "SD3", "SD2", "SD1", "CMD", "SD0",
             "CLK", "GND", "3V3", "EN", "RST", "GND", "Vin"]
RIGHT_PINS = ["D0", "D1", "D2", "D3", "D4", "3V3", "GND", "D5",
              "D6", "D7", "D8", "RX", "TX", "GND", "3V3"]
GPIO = {"D0": "GPIO16", "D1": "GPIO5", "D2": "GPIO4", "D3": "GPIO0",
        "D4": "GPIO2 · LED", "D5": "GPIO14", "D6": "GPIO12", "D7": "GPIO13",
        "D8": "GPIO15", "RX": "GPIO3", "TX": "GPIO1"}

# (NodeMCU pini, Ra-02 pini, Ra-02 pin no, renk, etiket)
WIRES = [
    ("D0", "RESET", 4, "#8e44ad", "RESET"),
    ("D2", "DIO0", 5, "#e67e22", "DIO0 (IRQ)"),
    ("3V3", "3.3V", 3, "#e53935", "3.3 V"),
    ("GND", "GND", 2, "#212121", "GND"),
    ("D5", "SCK", 12, "#1e88e5", "SCK"),
    ("D6", "MISO", 13, "#43a047", "MISO"),
    ("D7", "MOSI", 14, "#fbc02d", "MOSI"),
    ("D8", "NSS", 15, "#00897b", "NSS / CS"),
]
UNUSED = [("ANT", 1), ("DIO1", 6), ("DIO2", 7), ("DIO3", 8),
          ("GND", 9), ("DIO4", 10), ("DIO5", 11), ("GND", 16)]

# geometri
BX, BY, BW = 40, 200, 340          # NodeMCU kartı
PITCH, PY0 = 26, BY + 95
BH = PY0 - BY + PITCH * 15 + 50
RX0 = BX + BW                      # sağ header x
MX, MY, MW = 660, 260, 250         # Ra-02 modülü
MPITCH = 34


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def text(x, y, s, size=13, anchor="start", weight="normal", fill="#222", extra=""):
    return (f'<text x="{x}" y="{y}" font-size="{size}" text-anchor="{anchor}" '
            f'font-weight="{weight}" fill="{fill}" {extra}>{esc(s)}</text>')


out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" '
       f'viewBox="0 0 {W} {H}" font-family="{FONT}">',
       f'<rect width="{W}" height="{H}" fill="#ffffff"/>',
       text(W / 2, 38, "NodeMCU v3 (ESP8266)  ↔  Ai-Thinker Ra-02 (SX1278, 433 MHz)",
            20, "middle", "bold"),
       text(W / 2, 62, "esp-lora · A = alıcı, B = verici · Ra-02 bağlantısı iki düğümde aynı, GPS yalnızca B'de · mantıksal çizim "
            "(Ra-02 fiziksel pad sırası için pin numaralarına bakın)", 12, "middle", fill="#666")]

# --- NodeMCU kartı
out.append(f'<rect x="{BX}" y="{BY}" width="{BW}" height="{BH}" rx="10" fill="#1f2a44" stroke="#0d1426" stroke-width="2"/>')
# ESP-12 modülü + anten
out.append(f'<rect x="{BX+65}" y="{BY+12}" width="{BW-130}" height="62" rx="3" fill="#c9ccd1" stroke="#888"/>')
out.append(f'<path d="M{BX+80} {BY+24} h20 v12 h20 v-12 h20 v12 h20 v-12 h20" fill="none" stroke="#b8860b" stroke-width="3"/>')
out.append(text(BX + BW / 2, BY + 62, "ESP-12E · ESP8266EX", 11, "middle", "bold", "#333"))
# CH340 ve USB
out.append(f'<rect x="{BX+95}" y="{BY+BH-110}" width="70" height="34" rx="2" fill="#111" />')
out.append(text(BX + 130, BY + BH - 88, "CH340", 11, "middle", fill="#ddd"))
out.append(f'<rect x="{BX+BW/2-28}" y="{BY+BH-22}" width="56" height="30" rx="3" fill="#b0b4ba" stroke="#777"/>')
out.append(text(BX + BW / 2, BY + BH - 30, "micro-USB → PC", 11, "middle", fill="#ddd"))
out.append(text(BX + BW / 2, BY + 230, "NodeMCU v3", 18, "middle", "bold", "#fff"))
out.append(text(BX + BW / 2, BY + 252, "(LoLin)", 12, "middle", fill="#ccd"))

used_mcu = {w[0] for w in WIRES} | {"D1"}
right_y = {}
for i, p in enumerate(LEFT_PINS):
    y = PY0 + i * PITCH
    out.append(f'<circle cx="{BX}" cy="{y}" r="6" fill="#d4af37" stroke="#7a6418"/>')
    out.append(text(BX + 14, y + 4, p, 11, fill="#aab"))
for i, p in enumerate(RIGHT_PINS):
    y = PY0 + i * PITCH
    # 3V3 ve GND'den yalnızca ilkini kullan
    if p in ("3V3", "GND") and p not in right_y and p in used_mcu:
        right_y[p] = y
        active = True
    elif p not in ("3V3", "GND") and p in used_mcu:
        right_y[p] = y
        active = True
    else:
        active = False
    out.append(f'<circle cx="{RX0}" cy="{y}" r="6" fill="{"#d4af37" if active else "#8a7a40"}" stroke="#7a6418"/>')
    label = p + (f" ({GPIO[p]})" if p in GPIO else "")
    out.append(text(RX0 - 14, y + 4, label, 11, "end",
                    "bold" if active else "normal", "#fff" if active else "#889"))

# --- Ra-02 modülü
MH = MPITCH * len(WIRES) + 50
out.append(f'<rect x="{MX}" y="{MY}" width="{MW}" height="{MH}" rx="6" fill="#e8ecef" stroke="#555" stroke-width="2"/>')
out.append(f'<rect x="{MX+120}" y="{MY+30}" width="{MW-140}" height="{MH-120}" rx="4" fill="#c0c6cc" stroke="#888"/>')
out.append(text(MX + 175, MY + 90, "Ra-02", 20, "middle", "bold", "#333"))
out.append(text(MX + 175, MY + 112, "SX1278", 13, "middle", fill="#444"))
out.append(text(MX + 175, MY + 130, "410–525 MHz", 11, "middle", fill="#555"))
ra_y = {}
for i, (_, rp, num, _, _) in enumerate(WIRES):
    y = MY + 30 + i * MPITCH
    ra_y[rp] = y
    out.append(f'<rect x="{MX-6}" y="{y-6}" width="12" height="12" fill="#d4af37" stroke="#7a6418"/>')
    out.append(text(MX + 12, y + 4, f"{num}  {rp}", 12, weight="bold", fill="#222"))
# IPEX anten
ax, ay = MX + MW - 28, MY + MH - 40
out.append(f'<circle cx="{ax}" cy="{ay}" r="9" fill="#d4af37" stroke="#7a6418" stroke-width="2"/>')
out.append(f'<circle cx="{ax}" cy="{ay}" r="3" fill="#7a6418"/>')
out.append(f'<path d="M{ax} {ay-9} C {ax+40} {ay-40}, {ax+60} {ay-90}, {ax+70} {ay-150}" fill="none" stroke="#333" stroke-width="3"/>')
out.append(f'<line x1="{ax+70}" y1="{ay-150}" x2="{ax+70}" y2="{ay-260}" stroke="#333" stroke-width="6" stroke-linecap="round"/>')
out.append(text(ax + 82, ay - 200, "433 MHz", 12, weight="bold"))
out.append(text(ax + 82, ay - 184, "anten", 12))
out.append(text(ax + 82, ay - 168, "(IPEX)", 11, fill="#666"))
un = [f"{n} {p}" for p, n in UNUSED[1:]]
out.append(text(MX + 12, MY + MH - 22, "Kullanılmayan: " + ", ".join(un[:4]), 10, fill="#666"))
out.append(text(MX + 12, MY + MH - 8, ", ".join(un[4:]), 10, fill="#666"))

# --- kablolar (ortogonal, her biri ayrı dikey kanal)
x_start = RX0 + 6
for i, (mp, rp, num, col, lbl) in enumerate(WIRES):
    y1, y2 = right_y[mp], ra_y[rp]
    xm = RX0 + 50 + i * 24
    stroke = col
    out.append(f'<path d="M{x_start} {y1} H{xm} V{y2} H{MX-6}" fill="none" stroke="{stroke}" '
               f'stroke-width="3.2" stroke-linejoin="round" stroke-linecap="round"/>')
    out.append(f'<circle cx="{x_start}" cy="{y1}" r="3.5" fill="{stroke}"/>')
    out.append(text(MX - 14, y2 - 6, lbl, 10.5, "end", "bold", col if col != "#fbc02d" else "#b8860b"))

# --- GPS modülü (yalnızca verici düğüm)
GX, GY, GW, GH = MX, 92, MW + 60, 120
GCOL = "#6d4c41"
out.append(f'<rect x="{GX}" y="{GY}" width="{GW}" height="{GH}" rx="6" fill="#eef5ee" stroke="#2e7d32" stroke-width="2" stroke-dasharray="7 4"/>')
out.append(text(GX + GW - 12, GY + 22, "GPS (NEO-6M)", 14, "end", "bold", "#2e7d32"))
out.append(text(GX + GW - 12, GY + 82, "yalnızca VERİCİ (B)", 11, "end", "bold", "#2e7d32"))
out.append(text(GX + GW - 12, GY + 100, "donanımda doğrulandı (3.3 V)", 11, "end", fill="#2e7d32"))
gp = {"VCC": GY + 30, "GND": GY + 54, "TX": GY + 78, "RX": GY + 102}
notes_g = {"VCC": "← NodeMCU 3V3 (5V/Vin DEĞİL)", "GND": "← NodeMCU GND", "TX": "", "RX": "bağlı değil"}
for name, y in gp.items():
    out.append(f'<rect x="{GX-6}" y="{y-6}" width="12" height="12" fill="#d4af37" stroke="#7a6418"/>')
    out.append(text(GX + 12, y + 4, name, 12, weight="bold"))
    if notes_g[name]:
        out.append(text(GX + 50, y + 4, notes_g[name], 11, fill="#555"))
# TX -> D1 kablosu (D0 hattının üzerinden atlama ile)
y1 = right_y["D1"]; yt = gp["TX"]; xv = RX0 + 28; yh = right_y["D0"]
out.append(f'<path d="M{RX0+6} {y1} H{xv} V{yh+7} A7 7 0 0 0 {xv} {yh-7} V{yt} H{GX-6}" fill="none" '
           f'stroke="{GCOL}" stroke-width="3.2" stroke-linejoin="round" stroke-linecap="round"/>')
out.append(f'<circle cx="{RX0+6}" cy="{y1}" r="3.5" fill="{GCOL}"/>')
out.append(text(GX - 14, yt - 6, "GPS TX → D1", 10.5, "end", "bold", GCOL))
out.append(text(GX - 14, yt + 14, "(SoftwareSerial)", 10, "end", fill=GCOL))

# --- notlar
ny = BY + BH + 2
notes = [
    ("⚠ Ra-02 yalnızca 3.3 V ile çalışır — 5V / Vin'e bağlamayın.", "#c62828"),
    ("⚠ 433 MHz anten takılmadan yayın yapmayın (PA zarar görebilir).", "#c62828"),
    ("⚠ GPS 3.3 V ile beslenir (3V3, Ra-02 ile ortak) — Vin/5V kullanmayın.", "#c62828"),
    ("⚠ GPS TX'i RX (GPIO3) pinine bağlamayın — USB-seri (CH340) ile çakışır.", "#c62828"),
    ("• D4 (GPIO2): kart üstü LED, düğüm tanıma (açılışta A tek, B çift yanıp söner).", "#333"),
    ("• D3 (GPIO0) boot pini, boş. RST bilinçli olarak D0'da.", "#333"),
    ("• RadioLib:  SX1278 radio = new Module(15, 4, 16, RADIOLIB_NC);", "#333"),
]
for i, (s, c) in enumerate(notes):
    out.append(text(RX0 + 50, MY + MH + 40 + i * 20, s, 12, fill=c))

out.append("</svg>")
print("\n".join(out))
