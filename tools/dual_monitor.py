#!/usr/bin/env python3
"""İki düğümün seri çıktısını aynı anda okur. Kullanım: dual_monitor.py <saniye> [--reset]"""
import sys, time, threading, serial

PORTS = {
    "A": "/dev/serial/by-path/pci-0000:0b:00.0-usb-0:2:1.0-port0",
    "B": "/dev/serial/by-path/pci-0000:00:1a.0-usb-0:1.1:1.0-port0",
}
dur = float(sys.argv[1]) if len(sys.argv) > 1 else 30
reset = "--reset" in sys.argv
t0 = time.time()
lock = threading.Lock()


def run(name, port):
    s = serial.Serial(port, 115200, timeout=0.2)
    if reset:  # NodeMCU otomatik reset: DTR=GPIO0 high, RTS=EN pulse
        s.dtr = False
        s.rts = True
        time.sleep(0.1)
        s.rts = False
    buf = b""
    while time.time() - t0 < dur:
        buf += s.read(256)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            txt = line.decode("utf-8", "replace").strip()
            if txt.startswith("["):
                with lock:
                    print(f"{time.time()-t0:6.1f}s  {name}| {txt}", flush=True)
    s.close()


th = [threading.Thread(target=run, args=kv) for kv in PORTS.items()]
[t.start() for t in th]
[t.join() for t in th]
