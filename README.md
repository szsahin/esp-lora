# esp-lora

ESP mikroişlemcileri (ESP32 ailesi) ile LoRa haberleşme çalışmaları.

- Dokümantasyon: [Wiki](https://tbx.ofis.cmvteknoloji.com:10443/gitea/MB/esp-lora/wiki)
- Kodlar: `firmware/` altında, kullanılan geliştirme ortamına göre ayrılmış.

## Klasör yapısı

```
firmware/
  esp-idf/      ESP-IDF projeleri (idf.py)
  platformio/   PlatformIO projeleri (platformio.ini)
  arduino/      Arduino IDE / arduino-cli sketch'leri
  common/       Ortamlar arası paylaşılan kod (protokol, paket formatı vb.)
hardware/       Şemalar, pin haritaları, modül datasheet linkleri
tools/          Yardımcı script'ler (log okuma, paket çözümleme, test)
docs/           Repo içinde tutulması gereken görseller/ekler
```

## Kurallar

- Her proje kendi klasöründe, kendi `README.md`'si ile durur (donanım, pinler, nasıl derlenir).
- Gizli bilgiler (Wi-Fi şifresi, LoRaWAN AppKey vb.) commit edilmez; `secrets.h.example` / `sdkconfig.defaults` örneği bırakılır.
