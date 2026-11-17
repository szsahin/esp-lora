# Donanım

Bağlantı çizimi (`nodemcu-v3-ra02-wiring.svg/png`, üretici: `tools/wiring_diagram.py`). Pin tabloları ve gerekçeler wiki'deki **Donanım** sayfasında.

- A ve B: NodeMCU v3 (ESP8266) + Ai-Thinker Ra-02 (SX1278, 433 MHz) — NSS D8, DIO0 D2, RESET D0, SPI D5/D6/D7, 3V3
- Yalnızca B: NEO-6M GPS — VCC **3V3** (Vin/5V değil), GND, GPS TX → D1
