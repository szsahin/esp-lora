# Araçlar

Linux'ta `~/.platformio/penv/bin/python` (veya pyserial kurulu herhangi bir Python), Windows'ta `%USERPROFILE%\.platformio\penv\Scripts\python.exe` ile çalıştırılır. Gerekli: `pyserial`.

## gps_monitor.py — GPS izleme (`gps-izle`)

`gps-lora` firmware'i yüklü A kartının çıktısını okur; B'nin konumunu, uydu/HDOP, RSSI/SNR, paket kaybı ve referansa uzaklığı canlı gösterir.

```bash
python tools/gps_monitor.py                             # Linux: varsayılan A portu (by-path)
python tools/gps_monitor.py --port COM5                 # Windows
python tools/gps_monitor.py --ref 38.342700,38.276100   # mesafe A'nın bilinen konumuna göre
python tools/gps_monitor.py --reset --no-log
```

- Mesafe yalnızca iyi fix'lerde (uydu ≥ 5, HDOP ≤ 3.0); `--ref` yoksa referans = ilk 5 iyi fix'in ortalaması.
- Kayıt: `logs/gps-YYYYmmdd-HHMMSS.csv` (git'e eklenmez; saklanacak ölçümler `docs/olcumler/` altına kopyalanır).
- Ctrl+C ile çıkışta özet: alınan/kayıp paket, CRC hatası, en zayıf RSSI, en uzak mesafe.

## dual_monitor.py — iki kartı birlikte izleme (`lora-izle`)

```bash
python tools/dual_monitor.py [saniye] [--reset] [--a PORT] [--b PORT]
```

## wiring_diagram.py — bağlantı çizimi

```bash
python3 tools/wiring_diagram.py > hardware/nodemcu-v3-ra02-wiring.svg
rsvg-convert -w 1620 hardware/nodemcu-v3-ra02-wiring.svg -o hardware/nodemcu-v3-ra02-wiring.png
```

Bir port aynı anda tek programda açık olabilir: yüklemeden önce izleme araçlarını kapatın.
