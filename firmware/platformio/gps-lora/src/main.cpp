// GPS + LoRa — NodeMCU v3 (ESP8266) + Ra-02 (SX1278) + NEO-6M (yalnızca B)
//
// B (ROLE_TX=1): GPS'i okur (D1, 9600), her 5 sn'de 20 baytlık ikili paket gönderir.
// A (ROLE_TX=0): paketi çözer, tek satır "anahtar=değer" yazar (tools/gps_monitor.py okur).
//
// Paket (little-endian, 20 bayt):
//   0 magic 'G' | 1 düğüm | 2-3 seq | 4 bayrak (b0 fix, b1 saat, b2 GPS veri akıyor)
//   5 uydu | 6 HDOP x10 | 7-10 enlem x1e6 | 11-14 boylam x1e6 | 15-16 yükseklik m
//   17 saat | 18 dk | 19 sn (UTC)
// Airtime SF9/BW125/CR4/7, 20 bayt ≈ 226 ms → 5 sn'de %4.5 (sınır %10).
//
// LED (D4/GPIO2):
//   Açılış: A tek / B çift yanıp sönme x3
//   B her gönderim: fix VAR = 1 uzun (500 ms), fix YOK = 2 kısa, GPS verisi yok = 4 hızlı
//   A her paket: 1 uzun; CRC hatası 5 hızlı; arıza sürekli hızlı

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

#define XSTR(x) STR(x)
#define STR(x) #x
#ifdef NODE_ID
#define NODE_NAME XSTR(NODE_ID)
#else
#define NODE_NAME "?"
#endif
#ifndef ROLE_TX
#define ROLE_TX 0
#endif

#if ROLE_TX
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>
#endif

constexpr uint8_t PIN_NSS  = 15;  // D8
constexpr uint8_t PIN_DIO0 = 4;   // D2
constexpr uint8_t PIN_RST  = 16;  // D0
constexpr uint8_t PIN_LED  = 2;   // D4 (aktif LOW)
constexpr uint8_t PIN_GPS_RX = 5; // D1 <- GPS TX

constexpr float   FREQ_MHZ  = 434.0;
constexpr float   BW_KHZ    = 125.0;
constexpr uint8_t SF        = 9;
constexpr uint8_t CR        = 7;
constexpr uint8_t SYNC_WORD = 0x12;
constexpr int8_t  POWER_DBM = 10;
constexpr uint16_t PREAMBLE = 8;
constexpr uint32_t TX_INTERVAL_MS = 5000;

struct __attribute__((packed)) GpsPacket {
  uint8_t  magic;
  uint8_t  node;
  uint16_t seq;
  uint8_t  flags;
  uint8_t  sats;
  uint8_t  hdop10;
  int32_t  lat_e6;
  int32_t  lng_e6;
  int16_t  alt_m;
  uint8_t  hh, mm, ss;
};
static_assert(sizeof(GpsPacket) == 20, "paket 20 bayt olmali");
constexpr uint8_t F_FIX = 1, F_TIME = 2, F_GPSDATA = 4;

SX1278 radio = new Module(PIN_NSS, PIN_DIO0, PIN_RST, RADIOLIB_NC);
volatile bool dio0Flag = false;
IRAM_ATTR void onDio0() { dio0Flag = true; }

#define LOG(...) do { Serial.printf("[" NODE_NAME "] " __VA_ARGS__); Serial.println(); } while (0)

// ---- Bloklamayan LED desen motoru
static const uint16_t *ledPat = nullptr;
static uint8_t ledLen = 0, ledIdx = 0, ledRepeat = 0;
static uint32_t ledT = 0;
static void ledWrite(bool on) { digitalWrite(PIN_LED, on ? LOW : HIGH); }
static void ledPlay(const uint16_t *pat, uint8_t len, uint8_t repeat = 1) {
  ledPat = pat; ledLen = len; ledIdx = 0; ledRepeat = repeat; ledT = millis(); ledWrite(true);
}
static void ledUpdate() {
  if (!ledPat) return;
  if (millis() - ledT < ledPat[ledIdx]) return;
  ledT = millis();
  if (++ledIdx >= ledLen) {
    ledIdx = 0;
    if (ledRepeat != 255 && --ledRepeat == 0) { ledPat = nullptr; ledWrite(false); return; }
  }
  ledWrite(ledIdx % 2 == 0);
}
static const uint16_t PAT_ID_A[]   = {200, 800};
static const uint16_t PAT_ID_B[]   = {200, 200, 200, 800};
static const uint16_t PAT_FIX[]    = {500, 100};
static const uint16_t PAT_NOFIX[]  = {80, 120, 80, 120};
static const uint16_t PAT_NOGPS[]  = {40, 60, 40, 60, 40, 60, 40, 60};
static const uint16_t PAT_RX_OK[]  = {400, 100};
static const uint16_t PAT_RX_ERR[] = {50, 50, 50, 50, 50, 50, 50, 50, 50, 50};
static const uint16_t PAT_FAIL[]   = {100, 100};
#define LED_PLAY(p, rep) ledPlay(p, sizeof(p) / sizeof(p[0]), rep)

static void halt(const char *why) {
  LOG("DURDU: %s", why);
  LED_PLAY(PAT_FAIL, 255);
  while (true) { ledUpdate(); delay(5); }
}

static uint8_t readRegVersion() {
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, LOW); delay(2); digitalWrite(PIN_RST, HIGH); delay(10);
  pinMode(PIN_NSS, OUTPUT); digitalWrite(PIN_NSS, HIGH);
  SPI.begin();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_NSS, LOW);
  SPI.transfer(0x42 & 0x7F);
  uint8_t v = SPI.transfer(0x00);
  digitalWrite(PIN_NSS, HIGH);
  SPI.endTransaction();
  return v;
}

#if ROLE_TX
SoftwareSerial gpsSerial;
TinyGPSPlus gps;
static uint32_t lastGpsByte = 0;
#endif

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  ledWrite(false);
  delay(300);
  Serial.println();
  LOG("=== GPS+LoRa | rol: %s | %.3f MHz SF%u BW%.0f CR4/%u %d dBm ===",
      ROLE_TX ? "VERICI+GPS" : "ALICI", FREQ_MHZ, SF, BW_KHZ, CR, POWER_DBM);
  if (NODE_NAME[0] == 'B') LED_PLAY(PAT_ID_B, 3); else LED_PLAY(PAT_ID_A, 3);
  while (ledPat) { ledUpdate(); delay(5); }

  uint8_t ver = readRegVersion();
  LOG("RegVersion(0x42) = 0x%02X %s", ver, ver == 0x12 ? "-> OK (SX1278)" : "-> HATA");
  if (ver != 0x12) halt("SX1278 bulunamadi");
  int st = radio.begin(FREQ_MHZ, BW_KHZ, SF, CR, SYNC_WORD, POWER_DBM, PREAMBLE);
  if (st != RADIOLIB_ERR_NONE) { LOG("radio.begin hata kodu %d", st); halt("RadioLib baslatilamadi"); }
  radio.setCRC(true);
  radio.setDio0Action(onDio0, RISING);
  LOG("radio.begin OK");

#if ROLE_TX
  gpsSerial.begin(9600, SWSERIAL_8N1, PIN_GPS_RX, -1, false, 256);
  LOG("GPS: D1(GPIO5) 9600 baud | her %lu ms paket (%u bayt)",
      (unsigned long)TX_INTERVAL_MS, (unsigned)sizeof(GpsPacket));
#else
  st = radio.startReceive();
  LOG("Dinleniyor (startReceive=%d)", st);
#endif
}

#if ROLE_TX
static uint16_t seq = 0;
static uint32_t lastTx = 0, txStart = 0;
static bool txBusy = false;

void loop() {
  while (gpsSerial.available()) { gps.encode(gpsSerial.read()); lastGpsByte = millis(); }

  if (!txBusy && (lastTx == 0 || millis() - lastTx >= TX_INTERVAL_MS)) {
    GpsPacket p = {};
    p.magic = 'G'; p.node = NODE_NAME[0]; p.seq = seq;
    bool gpsAlive = lastGpsByte && millis() - lastGpsByte < 3000;
    bool fix = gps.location.isValid() && gps.location.age() < 5000;
    bool tmv = gps.time.isValid() && gps.time.age() < 5000 &&
               (gps.time.value() != 0 || gps.date.isValid());
    if (gpsAlive) p.flags |= F_GPSDATA;
    if (fix) {
      p.flags |= F_FIX;
      p.lat_e6 = (int32_t)lround(gps.location.lat() * 1e6);
      p.lng_e6 = (int32_t)lround(gps.location.lng() * 1e6);
      p.alt_m  = gps.altitude.isValid() ? (int16_t)lround(gps.altitude.meters()) : 0;
    }
    if (tmv) { p.flags |= F_TIME; p.hh = gps.time.hour(); p.mm = gps.time.minute(); p.ss = gps.time.second(); }
    p.sats = gps.satellites.isValid() ? (uint8_t)min<uint32_t>(gps.satellites.value(), 255) : 0;
    p.hdop10 = gps.hdop.isValid() ? (uint8_t)min(gps.hdop.hdop() * 10.0, 255.0) : 255;

    dio0Flag = false;
    int st = radio.startTransmit((uint8_t *)&p, sizeof(p));
    txStart = lastTx = millis();
    if (st == RADIOLIB_ERR_NONE) {
      txBusy = true;
      if (!gpsAlive) LED_PLAY(PAT_NOGPS, 1); else if (fix) LED_PLAY(PAT_FIX, 1); else LED_PLAY(PAT_NOFIX, 1);
    } else LOG("TX baslatma hatasi %d", st);
    if (fix)
      LOG("TX #%u | FIX %.6f,%.6f alt %d m | uydu %u HDOP %.1f", seq, p.lat_e6 / 1e6, p.lng_e6 / 1e6,
          p.alt_m, p.sats, p.hdop10 / 10.0);
    else
      LOG("TX #%u | fix yok | uydu %u | GPS veri %s | cumle ok %lu hata %lu", seq, p.sats,
          gpsAlive ? "VAR" : "YOK", (unsigned long)gps.passedChecksum(), (unsigned long)gps.failedChecksum());
    seq++;
  }
  if (txBusy && dio0Flag) { dio0Flag = false; txBusy = false; radio.finishTransmit(); }
  else if (txBusy && millis() - txStart > 3000) {
    txBusy = false; radio.finishTransmit(); LOG("TX zaman asimi: DIO0 kesmesi gelmedi");
  }
  ledUpdate();
  yield();
}

#else  // ALICI
static uint32_t rxOk = 0, rxErr = 0;

void loop() {
  if (dio0Flag) {
    dio0Flag = false;
    size_t len = radio.getPacketLength();
    uint8_t buf[64];
    int st = radio.readData(buf, min(len, sizeof(buf)));
    float rssi = radio.getRSSI(), snr = radio.getSNR(), ferr = radio.getFrequencyError();
    if (st == RADIOLIB_ERR_NONE && len == sizeof(GpsPacket) && buf[0] == 'G') {
      GpsPacket p; memcpy(&p, buf, sizeof(p));
      rxOk++;
      LED_PLAY(PAT_RX_OK, 1);
      char tm[12] = "--:--:--";
      if (p.flags & F_TIME) snprintf(tm, sizeof(tm), "%02u:%02u:%02u", p.hh, p.mm, p.ss);
      LOG("GPS node=%c seq=%u fix=%u lat=%.6f lon=%.6f alt=%d sat=%u hdop=%.1f utc=%s gpsdata=%u "
          "rssi=%.1f snr=%.1f ferr=%.0f ok=%lu err=%lu",
          p.node, p.seq, (p.flags & F_FIX) ? 1 : 0, p.lat_e6 / 1e6, p.lng_e6 / 1e6, p.alt_m, p.sats,
          p.hdop10 / 10.0, tm, (p.flags & F_GPSDATA) ? 1 : 0, rssi, snr, ferr,
          (unsigned long)rxOk, (unsigned long)rxErr);
    } else if (st == RADIOLIB_ERR_CRC_MISMATCH) {
      rxErr++;
      LED_PLAY(PAT_RX_ERR, 1);
      LOG("CRC hatasi rssi=%.1f snr=%.1f ok=%lu err=%lu", rssi, snr, (unsigned long)rxOk, (unsigned long)rxErr);
    } else if (st == RADIOLIB_ERR_NONE) {
      rxErr++;
      LOG("Bilinmeyen paket len=%u rssi=%.1f", (unsigned)len, rssi);
    } else {
      rxErr++;
      LOG("RX hata kodu %d", st);
    }
    radio.startReceive();
  }
  ledUpdate();
  yield();
}
#endif
