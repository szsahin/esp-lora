#!/home/mb/.local/share/pio-venv/bin/python
"""Alıcı (A) üzerinden B'nin GPS paketlerini canlı izler (firmware: gps-lora).

Kullanım: gps_monitor.py [--port PORT] [--reset] [--no-log]
  --port   : A'nın seri portu (Windows'ta ör. --port COM5). Verilmezse Linux by-path varsayılanı.
  --reset  : başlarken A'yı resetle
  --no-log : CSV kaydı tutma (varsayılan: logs/gps-YYYYmmdd-HHMMSS.csv)
Ctrl+C ile çık; çıkarken özet yazılır.
Gerekli: pyserial (pip install pyserial)
"""
import csv, math, os, re, sys, time
import serial

PORT = "/dev/serial/by-path/pci-0000:0b:00.0-usb-0:2:1.0-port0"
argv = sys.argv[1:]
if "--port" in argv:
    PORT = argv[argv.index("--port") + 1]
RESET = "--reset" in argv
LOG = "--no-log" not in argv
SILENCE_WARN_S = 15

if os.name == "nt":
    os.system("")  # Windows terminalinde ANSI renkleri aç
G, Y, R, C, D, B0, X = ("\033[32m", "\033[33m", "\033[31m", "\033[36m", "\033[2m", "\033[1m", "\033[0m")

KV = re.compile(r"(\w+)=(\S+)")


def haversine_m(a, b):
    la1, lo1, la2, lo2 = map(math.radians, (a[0], a[1], b[0], b[1]))
    h = math.sin((la2 - la1) / 2) ** 2 + math.cos(la1) * math.cos(la2) * math.sin((lo2 - lo1) / 2) ** 2
    return 2 * 6371000 * math.asin(math.sqrt(h))


def now():
    return time.strftime("%H:%M:%S")


def main():
    try:
        ser = serial.Serial(PORT, 115200, timeout=0.5)
    except serial.SerialException as e:
        print(f"{R}Port açılamadı: {PORT}\n  {e}\n  Port başka programda açık olabilir (pio monitor, lora-izle).{X}")
        sys.exit(1)
    if RESET:
        ser.dtr = False
        ser.rts = True
        time.sleep(0.1)
        ser.rts = False

    writer = f = path = None
    if LOG:
        logdir = os.path.join(os.path.dirname(os.path.dirname(os.path.realpath(__file__))), "logs")
        os.makedirs(logdir, exist_ok=True)
        path = os.path.join(logdir, time.strftime("gps-%Y%m%d-%H%M%S.csv"))
        f = open(path, "w", newline="")
        writer = csv.writer(f)
        writer.writerow(["pc_time", "seq", "fix", "lat", "lon", "alt", "sat", "hdop", "utc",
                         "rssi", "snr", "ferr", "dist_m", "lost"])

    print(f"{B0}GPS izleme{X} | port {PORT}" + (f" | kayıt {path}" if LOG else ""))
    print(f"{D}B her 5 sn'de bir paket gönderir. Mesafe = ilk fix noktasına uzaklık. Ctrl+C: çık{X}\n")

    start = None          # ilk fix noktası
    last_seq = None
    n_ok = n_lost = n_crc = 0
    max_d = 0.0
    rssi_min = None
    last_rx = time.time()
    last_warn = 0
    t0 = time.time()
    buf = b""

    try:
        while True:
            buf += ser.read(512)
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = raw.decode(errors="replace").strip()
                if not line:
                    continue
                if " GPS " not in line:
                    if "CRC" in line or "hata" in line.lower():
                        n_crc += "CRC" in line
                        print(f"{now()} {R}{line}{X}")
                    elif line.startswith("[A]"):
                        print(f"{now()} {D}{line}{X}")
                    continue

                kv = dict(KV.findall(line))
                seq = int(kv["seq"])
                lost = 0
                if last_seq is not None:
                    gap = (seq - last_seq) & 0xFFFF
                    if 1 < gap < 1000:
                        lost = gap - 1
                    elif gap == 0 or gap >= 1000:
                        print(f"{now()} {Y}B yeniden başlamış (seq {last_seq} -> {seq}){X}")
                last_seq = seq
                n_ok += 1
                n_lost += lost
                last_rx = time.time()
                rssi, snr = float(kv["rssi"]), float(kv["snr"])
                rssi_min = rssi if rssi_min is None else min(rssi_min, rssi)
                fix = kv["fix"] == "1"
                lat, lon = float(kv["lat"]), float(kv["lon"])
                dist = ""
                sig_col = G if rssi > -100 else (Y if rssi > -115 else R)
                sig = f"{sig_col}RSSI {rssi:6.1f} dBm SNR {snr:5.1f}{X}"
                lost_s = f" {R}(kayıp {lost}){X}" if lost else ""
                if fix:
                    if start is None:
                        start = (lat, lon)
                        print(f"{now()} {G}{B0}İLK FIX: {lat:.6f}, {lon:.6f} — başlangıç noktası{X}")
                    d = haversine_m(start, (lat, lon))
                    max_d = max(max_d, d)
                    dist = f"{d:.0f}"
                    pos = (f"{G}FIX{X} {lat:.6f},{lon:.6f} alt {kv['alt']:>4} m | uydu {kv['sat']:>2} "
                           f"HDOP {kv['hdop']:>4} | mesafe {B0}{d:6.0f} m{X}")
                else:
                    why = "GPS VERİSİ YOK (kablo/besleme)" if kv.get("gpsdata") == "0" else "fix bekleniyor"
                    pos = f"{Y}fix yok{X} | uydu {kv['sat']:>2} | {why}"
                print(f"{now()} #{seq:<5} {pos} | {sig} | UTC {kv['utc']}{lost_s}")
                if writer and f:
                    writer.writerow([time.strftime("%Y-%m-%d %H:%M:%S"), seq, int(fix), kv["lat"], kv["lon"],
                                     kv["alt"], kv["sat"], kv["hdop"], kv["utc"], kv["rssi"], kv["snr"],
                                     kv["ferr"], dist, lost])
                    f.flush()

            silent = time.time() - last_rx
            if silent > SILENCE_WARN_S and time.time() - last_warn > 10:
                last_warn = time.time()
                print(f"{now()} {R}{silent:.0f} sn'dir paket yok (menzil dışı / B kapalı / pil?){X}")
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
        dur = time.time() - t0
        total = n_ok + n_lost
        print(f"\n{B0}Özet{X} ({dur/60:.1f} dk): alınan {n_ok}, kayıp {n_lost}"
              + (f" (%{100*n_lost/total:.1f})" if total else "") + f", CRC hatası {n_crc}"
              + (f", en zayıf RSSI {rssi_min:.1f} dBm" if rssi_min is not None else "")
              + (f", en uzak mesafe {max_d:.0f} m" if start else ""))
        if LOG:
            print(f"Kayıt: {path}")


if __name__ == "__main__":
    main()
