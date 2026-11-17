# gps-lora — GPS konumunu LoRa ile gönderme

B (verici) NEO-6M GPS'ten konumu okur ve her 5 sn'de bir 20 baytlık ikili paketle A'ya (alıcı) gönderir.
A paketi çözüp tek satır `anahtar=değer` olarak seri porta yazar; bu satırları [`tools/gps_monitor.py`](../../../tools/gps_monitor.py) (`gps-izle`) okur.

Pinout: wiki **Donanım** sayfası.

| | Bağlantı |
|--|----------|
| Ra-02 (iki düğüm) | NSS D8, DIO0 D2, RESET D0, SCK D5, MISO D6, MOSI D7, 3V3, GND |
| GPS (yalnızca B) | VCC → **3V3** (Vin/5V DEĞİL), GND → GND, GPS TX → **D1** (GPIO5), GPS RX boş |

## Kullanım

```bash
pio run -e node_b -t upload     # B: verici + GPS
pio run -e node_a -t upload     # A: alıcı
gps-izle                        # repo kökünden: python tools/gps_monitor.py [--port COM5]
```

B bir kez yüklendikten sonra PC'ye gerek yoktur; powerbank ile beslenip dışarı götürülebilir.
A PC'de kalır, `gps-izle` ile izlenir.

## Parametreler

434.0 MHz · SF9 · BW 125 kHz · CR 4/7 · sync 0x12 · 10 dBm · CRC açık · 5 sn'de bir paket
Airtime (20 bayt): ≈ 226 ms (hesap) → görev döngüsü ≈ %4.5 (sınır %10). Ayrıntı: wiki **LoRa Parametreleri**.

## Paket formatı (20 bayt, little-endian)

| Bayt | Alan | Tip | Açıklama |
|------|------|-----|----------|
| 0 | magic | uint8 | `'G'` |
| 1 | node | uint8 | gönderen düğüm (`'B'`) |
| 2–3 | seq | uint16 | sıra no (B her açılışta 0'dan başlar) |
| 4 | flags | uint8 | bit0 fix var · bit1 saat geçerli · bit2 GPS'ten veri geliyor |
| 5 | sats | uint8 | kullanılan uydu sayısı |
| 6 | hdop10 | uint8 | HDOP × 10 (255 = bilinmiyor) |
| 7–10 | lat_e6 | int32 | enlem × 10⁶ |
| 11–14 | lng_e6 | int32 | boylam × 10⁶ |
| 15–16 | alt_m | int16 | yükseklik (m) |
| 17–19 | hh, mm, ss | uint8 ×3 | UTC saat |

## LED (kart üstü, D4/GPIO2)

| Durum | A (alıcı) | B (verici) |
|-------|-----------|------------|
| Açılış (3 kez) | tek yanıp sönme | çift yanıp sönme |
| Her paket | 1 uzun | fix VAR: **1 uzun** · fix YOK: **2 kısa** · GPS verisi yok: **4 hızlı** |
| CRC hatası | 5 hızlı | — |
| Arıza (SX1278 yok) | sürekli hızlı | sürekli hızlı |

## Beklenen çıktı

A (`pio device monitor -e node_a`):
```
[A] RegVersion(0x42) = 0x12 -> OK (SX1278)
[A] radio.begin OK
[A] Dinleniyor (startReceive=0)
[A] GPS node=B seq=30 fix=1 lat=38.342681 lon=38.276206 alt=944 sat=7 hdop=1.3 utc=13:22:33 gpsdata=1 rssi=-108.0 snr=4.5 ferr=850 ok=31 err=0
```

B (`pio device monitor -e node_b`, PC'ye bağlıyken):
```
[B] TX #30 | FIX 38.342681,38.276206 alt 944 m | uydu 7 HDOP 1.3
[B] TX #5 | fix yok | uydu 0 | GPS veri VAR | cumle ok 150 hata 0
```

## gps-izle mesafe hesabı

- Mesafe yalnızca **iyi fix**'lerde hesaplanır: uydu ≥ 5 ve HDOP ≤ 3.0. Zayıf fix'ler sarı "zayıf" olarak gösterilir. Sinyal kaybında NEO-6M birkaç saniye tahmini (yanlış) konum verir; bu satırlar mesafeye katılmaz.
- Referans: `--ref ENLEM,BOYLAM` (A'nın konumu) verilirse ona göre; verilmezse ilk 5 iyi fix'in ortalaması. Bu durumda başta B'yi ~1 dk A'nın yanında hareketsiz tutun.
- Her oturum `logs/gps-YYYYmmdd-HHMMSS.csv` dosyasına kaydedilir.

## Hata durumları

| Belirti | Olası sebep |
|---------|-------------|
| B'de 4 hızlı yanıp sönme / `GPS veri YOK` | GPS TX → D1 kablosu, 3V3/GND |
| `fix yok`, uydu 0–3 | Anten gökyüzünü görmüyor; ilk fix açık havada 1–5 dk sürebilir |
| `gps-izle`: "B yeniden başlamış" | Powerbank/kablo kesintisi (seq 0'a döner) |
| `gps-izle`: "… sn'dir paket yok" | Menzil dışı, B kapalı, powerbank düşük akımda kapanmış |
| Port açılamadı | Port başka programda açık (`pio device monitor`, `lora-izle`) |
