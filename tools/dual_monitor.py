#!/home/mb/.local/share/pio-venv/bin/python
"""İki düğümün seri çıktısını aynı anda okur.

Kullanım: dual_monitor.py [saniye] [--reset]
  saniye  : 0 veya verilmezse süresiz (Ctrl+C ile çık)
  --reset : başlarken iki kartı da resetle
"""
import sys, time, threading, serial

PORTS = {
    "A": "/dev/serial/by-path/pci-0000:0b:00.0-usb-0:2:1.0-port0",
    "B": "/dev/serial/by-path/pci-0000:00:1a.0-usb-0:1.1:1.0-port0",
}
args = [a for a in sys.argv[1:] if not a.startswith("--")]
dur = float(args[0]) if args else 0
reset = "--reset" in sys.argv
t0 = time.time()
lock = threading.Lock()
stop = threading.Event()


def run(name, port):
    try:
        s = serial.Serial(port, 115200, timeout=0.2)
    except serial.SerialException as e:
        with lock:
            print(f"{name}| port açılamadı: {e} (başka bir monitör açık olabilir)", flush=True)
        return
    if reset:  # NodeMCU: DTR=False GPIO0'ı HIGH tutar, RTS darbesi EN'i resetler
        s.dtr = False
        s.rts = True
        time.sleep(0.1)
        s.rts = False
    buf = b""
    while not stop.is_set():
        buf += s.read(256)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            txt = line.decode("utf-8", "replace").strip()
            if txt.startswith("["):
                with lock:
                    print(f"{time.strftime('%H:%M:%S')}  {name}| {txt}", flush=True)
    s.close()


threads = [threading.Thread(target=run, args=kv, daemon=True) for kv in PORTS.items()]
for t in threads:
    t.start()
try:
    while any(t.is_alive() for t in threads):
        if dur and time.time() - t0 >= dur:
            break
        time.sleep(0.2)
except KeyboardInterrupt:
    pass
stop.set()
for t in threads:
    t.join(timeout=1)
