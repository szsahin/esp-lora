# esp-lora

ESP8266 (NodeMCU v3) + Ai-Thinker Ra-02 (SX1278, 433 MHz) ile LoRa haberleşme ve GPS konum aktarımı çalışmaları.

- Dokümantasyon: [Wiki](https://tbx.ofis.cmvteknoloji.com:10443/gitea/MB/esp-lora/wiki) — donanım, kod yükleme (Linux/Windows), LoRa parametreleri, test sonuçları
- Kodlar: `firmware/` altında, geliştirme ortamına göre ayrılmış

## Sistem

| Düğüm | Rol | Donanım |
|-------|-----|---------|
| A | Alıcı (PC'ye USB ile bağlı) | NodeMCU v3 + Ra-02 |
| B | Verici (powerbank ile taşınabilir) | NodeMCU v3 + Ra-02 + NEO-6M GPS (3.3 V) |

B, GPS konumunu her 5 sn'de 20 baytlık bir LoRa paketiyle A'ya gönderir; A'nın çıktısı `gps-izle` ile canlı izlenir.

## Projeler

| Klasör | Ne yapar | Durum |
|--------|----------|-------|
| [`firmware/platformio/lora-test`](firmware/platformio/lora-test) | Ra-02 bağlantı testi, B → A metin paketi, RSSI/SNR | Doğrulandı (16.11.2026) |
| [`firmware/platformio/gps-test`](firmware/platformio/gps-test) | NEO-6M NMEA veri akışı testi (yalnızca B) | Doğrulandı (17.11.2026) |
| [`firmware/platformio/gps-lora`](firmware/platformio/gps-lora) | **Güncel firmware:** B'nin GPS konumunu LoRa ile A'ya gönderme | Dış mekânda doğrulandı (17.11.2026) |

## Araçlar

| Script | Kısayol (Linux) | Ne yapar |
|--------|-----------------|----------|
| [`tools/gps_monitor.py`](tools/gps_monitor.py) | `gps-izle` | A üzerinden B'nin GPS paketlerini canlı izler, mesafe/RSSI gösterir, CSV kaydeder |
| [`tools/dual_monitor.py`](tools/dual_monitor.py) | `lora-izle` | İki kartın seri çıktısını aynı anda gösterir |
| [`tools/wiring_diagram.py`](tools/wiring_diagram.py) | — | `hardware/` altındaki bağlantı çizimini (SVG) üretir |

## Klasör yapısı

```
firmware/
  platformio/   PlatformIO projeleri (her biri kendi platformio.ini + README.md ile)
  esp-idf/      ESP-IDF projeleri (ileride ESP32 için, şu an boş)
  arduino/      Arduino IDE sketch'leri (şu an boş)
  common/       Ortamlar arası paylaşılan kod (şu an boş)
hardware/       Bağlantı çizimi (SVG/PNG)
tools/          İzleme ve çizim script'leri
docs/olcumler/  Saha testlerinin ham CSV kayıtları
logs/           gps-izle'nin yerel kayıtları (git'e eklenmez)
```

## Hızlı başlangıç

```bash
cd firmware/platformio/gps-lora
pio run -e node_b -t upload     # B: verici + GPS
pio run -e node_a -t upload     # A: alıcı
cd ../../.. && python tools/gps_monitor.py      # Linux kısayolu: gps-izle
```

Ayrıntı ve Windows adımları: wiki **Kod Yükleme**.

## Kurallar

- Her proje kendi klasöründe, kendi `README.md`'si ile durur (donanım, pinler, nasıl derlenir).
- Kütüphane sürümleri `platformio.ini`'de sabitlenir.
- Gizli bilgiler (Wi-Fi şifresi, LoRaWAN AppKey, token vb.) commit edilmez; `secrets.h.example` / `sdkconfig.defaults` örneği bırakılır.
