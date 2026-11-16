// Ra-02 (SX1278) donanım testi — NodeMCU v3 (ESP8266)
//
// 1) SPI kablolama: RegVersion (0x42) okunur, 0x12 beklenir.
// 2) RadioLib ile modül başlatılır.
// 3) ROLE_TX=1: her 5 sn'de bir sayaçlı paket gönderir.
//    ROLE_TX=0: gelen paketleri RSSI / SNR / frekans hatası ile yazar.
//
// Seri çıktı 115200 baud, satırlar "[A]" / "[B]" önekli.
//
// Kart üstü mavi LED (GPIO2 / D4, ESP-12 üzerindeki) ile düğüm tanıma:
//   Açılışta: A = tek yanıp sönme x3, B = çift yanıp sönme x3
//   Verici:   her paket gönderiminde 2 kısa yanıp sönme
//   Alıcı:    her doğru pakette 1 uzun yanma (400 ms), CRC hatasında 5 hızlı yanıp sönme
//   Hata (SX1278 yok / begin hatası): sürekli hızlı yanıp sönme
// Not: NodeMCU'nun ikinci LED'i D0'da; D0 Ra-02 RST olduğu için kullanılmıyor.

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

// NODE_ID build flag'den tırnaksız gelir (-D NODE_ID=A); Windows/Linux kabuk farkı olmasın diye
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

// Pinler (wiki: Donanım)
constexpr uint8_t PIN_NSS  = 15;  // D8
constexpr uint8_t PIN_DIO0 = 4;   // D2
constexpr uint8_t PIN_RST  = 16;  // D0
constexpr uint8_t PIN_LED  = 2;   // D4, kart üstü LED (aktif LOW)

// LoRa parametreleri — TR 433.05–434.79 MHz, ~10 dBm
constexpr float   FREQ_MHZ  = 434.0;
constexpr float   BW_KHZ    = 125.0;
constexpr uint8_t SF        = 9;
constexpr uint8_t CR        = 7;     // 4/7
constexpr uint8_t SYNC_WORD = 0x12;  // özel ağ
constexpr int8_t  POWER_DBM = 10;
constexpr uint16_t PREAMBLE = 8;

constexpr uint32_t TX_INTERVAL_MS = 5000;

SX1278 radio = new Module(PIN_NSS, PIN_DIO0, PIN_RST, RADIOLIB_NC);

volatile bool dio0Flag = false;
IRAM_ATTR void onDio0() { dio0Flag = true; }

#define LOG(...) do { Serial.printf("[" NODE_NAME "] " __VA_ARGS__); Serial.println(); } while (0)

// ---- Bloklamayan LED desen motoru
// Desen: ms cinsinden süreler, sırayla YANIK, SÖNÜK, YANIK, SÖNÜK ...
static const uint16_t *ledPat = nullptr;
static uint8_t ledLen = 0, ledIdx = 0, ledRepeat = 0;
static uint32_t ledT = 0;

static void ledWrite(bool on) { digitalWrite(PIN_LED, on ? LOW : HIGH); }

static void ledPlay(const uint16_t *pat, uint8_t len, uint8_t repeat = 1) {
  ledPat = pat; ledLen = len; ledIdx = 0; ledRepeat = repeat;
  ledT = millis();
  ledWrite(true);
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

static const uint16_t PAT_ID_A[]   = {200, 800};                 // tek
static const uint16_t PAT_ID_B[]   = {200, 200, 200, 800};       // çift
static const uint16_t PAT_TX[]     = {80, 120, 80, 120};         // 2 kısa
static const uint16_t PAT_RX_OK[]  = {400, 100};                 // 1 uzun
static const uint16_t PAT_RX_ERR[] = {50, 50, 50, 50, 50, 50, 50, 50, 50, 50};
static const uint16_t PAT_FAIL[]   = {100, 100};                 // sürekli hızlı

#define LED_PLAY(p, rep) ledPlay(p, sizeof(p) / sizeof(p[0]), rep)

static void ledIdentify() {
  // Açılışta bloklayarak oynat (radyo başlamadan önce, ~3 sn)
  if (NODE_NAME[0] == 'B') LED_PLAY(PAT_ID_B, 3); else LED_PLAY(PAT_ID_A, 3);
  while (ledPat) { ledUpdate(); delay(5); }
}

static uint8_t readRegVersion() {
  // Modülü donanımsal resetle, sonra 0x42'yi doğrudan SPI ile oku
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, LOW);
  delay(2);
  digitalWrite(PIN_RST, HIGH);
  delay(10);

  pinMode(PIN_NSS, OUTPUT);
  digitalWrite(PIN_NSS, HIGH);
  SPI.begin();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_NSS, LOW);
  SPI.transfer(0x42 & 0x7F);        // okuma: MSB = 0
  uint8_t v = SPI.transfer(0x00);
  digitalWrite(PIN_NSS, HIGH);
  SPI.endTransaction();
  return v;
}

static void halt(const char *why) {
  LOG("DURDU: %s", why);
  LED_PLAY(PAT_FAIL, 255);
  while (true) { ledUpdate(); delay(5); }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  ledWrite(false);
  delay(300);
  Serial.println();
  LOG("=== Ra-02 test | rol: %s | %.3f MHz SF%u BW%.0f CR4/%u %d dBm ===",
      ROLE_TX ? "VERICI" : "ALICI", FREQ_MHZ, SF, BW_KHZ, CR, POWER_DBM);
  LOG("LED: kimlik deseni (%s)", NODE_NAME[0] == 'B' ? "cift yanip sonme" : "tek yanip sonme");
  ledIdentify();

  // 1) SPI kablolama kontrolü
  uint8_t ver = readRegVersion();
  LOG("RegVersion(0x42) = 0x%02X %s", ver,
      ver == 0x12 ? "-> OK (SX1278)" :
      (ver == 0x00 || ver == 0xFF) ? "-> HATA: SPI/besleme/NSS kontrol et" :
      "-> beklenmeyen deger");
  if (ver != 0x12) halt("SX1278 bulunamadi");

  // 2) RadioLib başlatma
  int st = radio.begin(FREQ_MHZ, BW_KHZ, SF, CR, SYNC_WORD, POWER_DBM, PREAMBLE);
  if (st != RADIOLIB_ERR_NONE) {
    LOG("radio.begin hata kodu %d", st);
    halt("RadioLib baslatilamadi");
  }
  radio.setCRC(true);
  LOG("radio.begin OK");

  radio.setDio0Action(onDio0, RISING);

#if ROLE_TX
  LOG("Her %lu ms'de bir paket gonderilecek", (unsigned long)TX_INTERVAL_MS);
#else
  st = radio.startReceive();
  LOG("Dinleniyor (startReceive=%d)", st);
#endif
}

#if ROLE_TX
static uint32_t seq = 0;
static uint32_t lastTx = 0;
static bool txBusy = false;
static uint32_t txStart = 0;

void loop() {
  if (!txBusy && (lastTx == 0 || millis() - lastTx >= TX_INTERVAL_MS)) {
    char msg[48];
    snprintf(msg, sizeof(msg), "PING %s #%lu", NODE_NAME, (unsigned long)seq);
    dio0Flag = false;
    int st = radio.startTransmit(msg);
    txStart = millis();
    lastTx = txStart;
    if (st == RADIOLIB_ERR_NONE) {
      txBusy = true;
      LED_PLAY(PAT_TX, 1);
    } else {
      LOG("TX baslatma hatasi %d", st);
    }
  }

  if (txBusy && dio0Flag) {
    dio0Flag = false;
    txBusy = false;
    radio.finishTransmit();
    LOG("TX #%lu tamam, %lu ms (DIO0 kesmesi OK)", (unsigned long)seq,
        (unsigned long)(millis() - txStart));
    seq++;
  } else if (txBusy && millis() - txStart > 3000) {
    txBusy = false;
    radio.finishTransmit();
    LOG("TX #%lu zaman asimi: DIO0 kesmesi gelmedi -> DIO0 (D2) kablosunu kontrol et",
        (unsigned long)seq);
    seq++;
  }
  ledUpdate();
  yield();
}

#else  // ALICI
static uint32_t rxCount = 0, rxErr = 0;

void loop() {
  if (dio0Flag) {
    dio0Flag = false;
    String s;
    int st = radio.readData(s);
    if (st == RADIOLIB_ERR_NONE) {
      rxCount++;
      LED_PLAY(PAT_RX_OK, 1);
      LOG("RX \"%s\" | RSSI %.1f dBm | SNR %.1f dB | f_err %.0f Hz | ok=%lu hata=%lu",
          s.c_str(), radio.getRSSI(), radio.getSNR(), radio.getFrequencyError(),
          (unsigned long)rxCount, (unsigned long)rxErr);
    } else if (st == RADIOLIB_ERR_CRC_MISMATCH) {
      rxErr++;
      LED_PLAY(PAT_RX_ERR, 1);
      LOG("RX CRC hatasi (ok=%lu hata=%lu)", (unsigned long)rxCount, (unsigned long)rxErr);
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
