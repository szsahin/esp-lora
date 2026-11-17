# docs

Asıl dokümantasyon [wiki](https://tbx.ofis.cmvteknoloji.com:10443/gitea/MB/esp-lora/wiki)'dedir. Burada yalnızca repo ile birlikte sürümlenmesi gereken görseller/ekler tutulur.

## olcumler/

Saha testlerinin ham `gps-izle` CSV kayıtları. Özetleri wiki **Testler ve Ölçümler** sayfasındadır.

| Dosya | Test |
|-------|------|
| `2026-11-17-gps-lora-dis-mekan.csv` | İlk dış mekân GPS + LoRa testi (B powerbank ile dışarıda, A PC'de) |

Not: bu kayıt `gps-izle`'nin ilk sürümüyle alındı; `dist_m` sütunu ilk fix'e göre (zayıf fix'ler dahil) hesaplandığı için güvenilir değildir, konumdan yeniden hesaplanmalıdır.
