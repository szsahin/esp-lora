# PlatformIO projeleri

Her proje ayrı klasörde, kendi `platformio.ini` ve `README.md` dosyasıyla. Ortamlar: `node_a` (A, alıcı), `node_b` (B, verici).

| Proje | Açıklama |
|-------|----------|
| [`lora-test`](lora-test) | Ra-02 bağlantı ve B → A haberleşme testi |
| [`gps-test`](gps-test) | NEO-6M NMEA veri akışı testi (yalnızca B) |
| [`gps-lora`](gps-lora) | **Güncel:** B'nin GPS konumunu LoRa ile A'ya gönderme |

```bash
cd <proje>
pio run -e node_a -t upload      # Windows / farklı port: --upload-port COM5
pio device monitor -e node_a
```
