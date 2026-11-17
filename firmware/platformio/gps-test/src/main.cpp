// NEO-6M GPS testi — NodeMCU v3 (ESP8266), düğüm B
//
// Bağlantı: GPS VCC -> 3V3 (Vin/5V DEĞİL), GND -> GND, GPS TX -> D1, GPS RX boş.
// 1) Veri var mı: GPS TX -> D1 (GPIO5), SoftwareSerial 9600 baud.
//    Hiç bayt gelmezse: "VERI YOK" -> TX kablosu / besleme / baud kontrol et.
// 2) NMEA geçerli mi: checksum hatası sayılır (çok hata = yanlış baud / gürültü).
// 3) Konum: uydu sayısı, HDOP, enlem/boylam, yükseklik, UTC saat (her 2 sn özet).
//
// Ham NMEA satırları "NMEA>" önekiyle ilk 20 sn ve sonra her 30 sn'de bir 5 satır yazılır.
// Kart üstü LED (D4/GPIO2): veri yok = sönük, veri var/fix yok = her sn kısa yanıp sönme,
//                           fix var = her sn uzun yanma.
// Seri çıktı 115200 baud, satırlar "[B]" önekli.

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>

#define XSTR(x) STR(x)
#define STR(x) #x
#ifdef NODE_ID
#define NODE_NAME XSTR(NODE_ID)
#else
#define NODE_NAME "?"
#endif

constexpr uint8_t  PIN_GPS_RX = 5;   // D1 <- GPS TX
constexpr uint8_t  PIN_LED    = 2;   // D4, aktif LOW
constexpr uint32_t GPS_BAUD   = 9600;

SoftwareSerial gpsSerial;
TinyGPSPlus gps;

#define LOG(...) do { Serial.printf("[" NODE_NAME "] " __VA_ARGS__); Serial.println(); } while (0)

static char line[100];
static uint8_t lineLen = 0;
static uint32_t rawUntil = 20000, nextRawBurst = 0;
static uint8_t rawBurstLeft = 0;
static uint32_t bytesTotal = 0, bytesLastSec = 0, lastByteMs = 0;

static void handleRawLine() {
  line[lineLen] = 0;
  bool show = millis() < rawUntil || rawBurstLeft > 0;
  if (show && lineLen > 0) {
    Serial.printf("[" NODE_NAME "] NMEA> %s\n", line);
    if (millis() >= rawUntil && rawBurstLeft > 0) rawBurstLeft--;
  }
  lineLen = 0;
}

static void ledUpdate() {
  uint32_t ph = millis() % 1000;
  bool on;
  if (millis() - lastByteMs > 3000 || lastByteMs == 0) on = false;
  else if (gps.location.isValid() && gps.location.age() < 3000) on = ph < 600;
  else on = ph < 60;
  digitalWrite(PIN_LED, on ? LOW : HIGH);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);
  delay(300);
  Serial.println();
  LOG("=== NEO-6M GPS testi | D1(GPIO5) <- GPS TX | %lu baud ===", (unsigned long)GPS_BAUD);
  gpsSerial.begin(GPS_BAUD, SWSERIAL_8N1, PIN_GPS_RX, -1, false, 256);
  if (!gpsSerial) LOG("HATA: SoftwareSerial baslatilamadi");
  LOG("Ilk 20 sn ham NMEA yazilacak, sonra her 2 sn ozet");
}

void loop() {
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    bytesTotal++; bytesLastSec++; lastByteMs = millis();
    gps.encode(c);
    if (c == '\n' || c == '\r') { if (lineLen) handleRawLine(); }
    else if (lineLen < sizeof(line) - 1) line[lineLen++] = c;
  }

  static uint32_t lastReport = 0;
  if (millis() - lastReport >= 2000) {
    lastReport = millis();
    if (bytesTotal == 0) {
      LOG("VERI YOK (%lu sn): GPS TX -> D1, VCC -> 3V3, GND kontrol et",
          (unsigned long)(millis() / 1000));
    } else if (millis() - lastByteMs > 3000) {
      LOG("VERI KESILDI (son bayt %lu ms once)", (unsigned long)(millis() - lastByteMs));
    } else {
      char pos[80];
      if (gps.location.isValid())
        snprintf(pos, sizeof(pos), "FIX %.6f, %.6f | yuks %.1f m",
                 gps.location.lat(), gps.location.lng(), gps.altitude.meters());
      else
        snprintf(pos, sizeof(pos), "fix yok");
      char tm[16] = "--:--:--";
      if (gps.time.isValid() && gps.time.age() < 5000 && (gps.time.value() != 0 || gps.date.isValid()))
        snprintf(tm, sizeof(tm), "%02u:%02u:%02u", gps.time.hour(), gps.time.minute(), gps.time.second());
      LOG("%s | uydu %lu | HDOP %.1f | UTC %s | %lu B/s | cumle ok %lu, checksum hata %lu",
          pos, gps.satellites.isValid() ? (unsigned long)gps.satellites.value() : 0UL,
          gps.hdop.isValid() ? gps.hdop.hdop() : 99.9, tm,
          (unsigned long)(bytesLastSec / 2), (unsigned long)gps.passedChecksum(),
          (unsigned long)gps.failedChecksum());
    }
    bytesLastSec = 0;
  }

  if (millis() >= rawUntil && millis() >= nextRawBurst) {
    nextRawBurst = millis() + 30000;
    rawBurstLeft = 5;
  }
  ledUpdate();
  yield();
}
