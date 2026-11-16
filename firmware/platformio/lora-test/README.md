# lora-test — Ra-02 (SX1278) donanım testi

NodeMCU v3 (ESP8266) + Ra-02 bağlantısını ve iki düğüm arasında LoRa haberleşmesini doğrulayan en basit test.
Pinout: wiki **Donanım** sayfası (NSS D8, DIO0 D2, RST D0, SPI D5/D6/D7). GPS bu testte kullanılmaz.

## Ne test eder?

1. **SPI kablolama:** `RegVersion (0x42)` okunur, `0x12` dönmeli.
2. **RadioLib başlatma:** `radio.begin(...)` hata kodu 0 olmalı.
3. **DIO0 kesmesi:** Verici her paketten sonra "TX tamam (DIO0 kesmesi OK)" yazmalı.
4. **Radyo bağlantısı:** Alıcı paketleri RSSI / SNR / frekans hatasıyla yazmalı.

## Parametreler

434.0 MHz · SF9 · BW 125 kHz · CR 4/7 · sync 0x12 · 10 dBm · CRC açık · 5 sn'de bir paket (~%4 duty cycle)

## Kullanım

```bash
pio run -e node_a -t upload      # A: verici
pio run -e node_b -t upload      # B: alıcı
pio device monitor -e node_b     # alıcı çıktısı
```

`upload_port` / `monitor_port` değerleri `/dev/serial/by-path/...` ile sabitlendi (bkz. Donanım → Kartlar).

## Beklenen çıktı

```
[B] RegVersion(0x42) = 0x12 -> OK (SX1278)
[B] radio.begin OK
[B] Dinleniyor (startReceive=0)
[B] RX "PING A #0" | RSSI -45.0 dBm | SNR 9.5 dB | f_err 1200 Hz | ok=1 hata=0
```

## Hata durumları

| Çıktı | Olası sebep |
|-------|-------------|
| `RegVersion = 0x00` veya `0xFF` | SCK/MISO/MOSI/NSS kablosu, 3.3 V besleme, GND |
| `radio.begin hata kodu -2` | Çip bulunamadı (SPI) |
| `TX zaman asimi: DIO0 kesmesi gelmedi` | DIO0 → D2 kablosu |
| Alıcıda hiç RX yok, verici TX OK | Anten, frekans/SF/sync uyuşmazlığı, mesafe |
| Kart sürekli resetleniyor / boot etmiyor | Besleme yetersiz, GPIO15 (NSS) boot'ta HIGH |
