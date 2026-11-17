# gps-test — NEO-6M veri akışı testi

Yalnızca B kartında, GPS modülünün kablolamasını ve NMEA veri akışını doğrular. LoRa kullanılmaz.
Güncel firmware [`gps-lora`](../gps-lora); bu proje GPS sorunlarını tek başına ayıklamak içindir.

## Bağlantı

GPS VCC → **3V3** (Vin/5V DEĞİL) · GND → GND · GPS TX → **D1** (GPIO5) · GPS RX boş

## Kullanım

```bash
pio run -e node_b -t upload
pio device monitor -e node_b
```

## Çıktı

- İlk 20 sn ham NMEA satırları (`NMEA> $GPRMC...`), sonra her 30 sn'de 5 satır.
- Her 2 sn özet: fix durumu, uydu, HDOP, UTC, bayt/sn, checksum sayaçları.

```
[B] NMEA> $GPGGA,,,,,,0,00,99.99,,,,,,*48
[B] fix yok | uydu 0 | HDOP 100.0 | UTC --:--:-- | 162 B/s | cumle ok 18, checksum hata 0
```

| Belirti | Anlamı |
|---------|--------|
| `VERI YOK` | GPS TX → D1, 3V3, GND kablosu |
| checksum hata çok | yanlış baud / gürültü |
| veri var, `fix yok` | anten gökyüzünü görmüyor (iç mekân) |

LED (D4): sönük = veri yok · saniyede kısa yanıp sönme = veri var, fix yok · saniyede uzun yanma = fix var

Doğrulama (17.11.2026, iç mekân): ~162 B/s, 258 cümle, checksum hatası 0.
