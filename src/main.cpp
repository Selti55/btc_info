#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"

// -----------------------------------------------------------------------------
// KONFIGURATION (alle anpassbaren Parameter als #define)
// -----------------------------------------------------------------------------
// Nachlesen:
// - Arduino C/C++ Präprozessor (#define): https://www.arduino.cc/reference/en/language/structure/further-syntax/define/
// - ESP32 Deep Sleep: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/sleep_modes.html
// - NTP / Zeitfunktionen (ESP32 Arduino):
//   https://docs.espressif.com/projects/arduino-esp32/en/latest/api/time.html
// - CoinGecko API: https://www.coingecko.com/en/api/documentation
//
// Idee: Hier oben stehen ALLE zentralen Stellschrauben.
// Du musst für typische Anpassungen später kaum im restlichen Code suchen.
// -----------------------------------------------------------------------------

// ------------------------
// Hardware / Display
// ------------------------
#define CFG_PIN_EPD_CS 10
#define CFG_PIN_EPD_DC 11
#define CFG_PIN_EPD_RST 12
#define CFG_PIN_EPD_BUSY 13

// Display-Init-Parameter von GxEPD2:
// init(serial_baud, initial, reset_duration, pulldown_rst_mode)
#define CFG_DISPLAY_INIT_BAUD 115200
#define CFG_DISPLAY_INIT_INITIAL true
#define CFG_DISPLAY_INIT_RESET_DURATION 2
#define CFG_DISPLAY_INIT_PULDOWN_RST false

// ------------------------
// Serielle Ausgabe
// ------------------------
#define CFG_SERIAL_BAUD 115200
#define CFG_BOOT_DELAY_MS 300

// ------------------------
// WLAN / HTTP / API
// ------------------------
#define CFG_WIFI_CONNECT_TIMEOUT_MS 20000UL
#define CFG_WIFI_CONNECT_RETRY_DELAY_MS 500UL
#define CFG_HTTP_TIMEOUT_MS 10000UL
#define CFG_JSON_DOC_SIZE 512

#define CFG_URL_COINGECKO "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=eur,usd&include_market_cap=true"
#define CFG_URL_BLOCK_HEIGHT "https://blockchain.info/q/getblockcount"

// ------------------------
// Zeitsteuerung (lokale Zeit)
// ------------------------
#define CFG_TZ_INFO "CET-1CEST,M3.5.0/02,M10.5.0/03"
#define CFG_NTP_SERVER_PRIMARY "pool.ntp.org"
#define CFG_NTP_SERVER_SECONDARY "time.nist.gov"
#define CFG_NTP_MAX_ATTEMPTS 8
#define CFG_NTP_SINGLE_ATTEMPT_TIMEOUT_MS 1000UL

#define CFG_DAY_START_HOUR 7
#define CFG_EVENING_START_HOUR 18
#define CFG_NIGHT_START_HOUR 22

#define CFG_FETCH_INTERVAL_DAY_MS (10UL * 60UL * 1000UL)
#define CFG_FETCH_INTERVAL_EVENING_MS (30UL * 60UL * 1000UL)
#define CFG_FETCH_INTERVAL_NIGHT_MS (90UL * 60UL * 1000UL)
#define CFG_FETCH_INTERVAL_FALLBACK_MS CFG_FETCH_INTERVAL_EVENING_MS

// ------------------------
// Display-Refresh-Regel
// ------------------------
#define CFG_DISPLAY_UPDATE_THRESHOLD_PERCENT 0.5f

// -----------------------------------------------------------------------------
// Hardware-Objekte
// -----------------------------------------------------------------------------
// Waveshare ESP32-S3 1.54" e-Paper (Standard-Pinbelegung)
// Wenn dein Board anders verdrahtet ist, nur CFG_PIN_* oben anpassen.

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
  GxEPD2_154_D67(CFG_PIN_EPD_CS, CFG_PIN_EPD_DC, CFG_PIN_EPD_RST, CFG_PIN_EPD_BUSY));

// -----------------------------------------------------------------------------
// Datenmodell
// -----------------------------------------------------------------------------
// Kapselt einen vollständigen Mess-/Abrufzyklus.
// Damit lassen sich Logging und Display-Zeichnen sauber in einem Objekt bündeln.
struct BtcSnapshot
{
  float btcPriceEuro;
  float btcPriceUsd;
  double btcMarketCapUsd;
  uint32_t btcBlockHeight;
  uint32_t moscowTime;
  bool pricesOk;
  bool blockHeightOk;
};

// RTC_DATA_ATTR: Diese Variablen bleiben über Deep Sleep hinweg erhalten.
// So wissen wir beim nächsten Wakeup, welcher EUR-Kurs zuletzt wirklich
// auf dem Display sichtbar war.
RTC_DATA_ATTR float g_lastDisplayedPriceEur = NAN;
RTC_DATA_ATTR bool g_hasDisplayedPrice = false;

// -----------------------------------------------------------------------------
// Netzwerk
// -----------------------------------------------------------------------------
// Baut bei Bedarf eine WLAN-Verbindung auf.
// Rückgabewert: true bei aktiver Verbindung, sonst false.
bool connectWifi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return true;
  }

  if (String(WIFI_SSID).length() == 0 || String(WIFI_PASSWORD).length() == 0)
  {
    Serial.println("WLAN nicht konfiguriert. Bitte WIFI_SSID/WIFI_PASSWORD setzen.");
    return false;
  }

  Serial.printf("Verbinde mit WLAN: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < CFG_WIFI_CONNECT_TIMEOUT_MS)
  {
    delay(CFG_WIFI_CONNECT_RETRY_DELAY_MS);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("WLAN verbunden. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("WLAN-Verbindung fehlgeschlagen.");
  return false;
}

// Holt Preis- und Market-Cap-Daten über die CoinGecko-API.
// Ausgabeparameter werden nur bei erfolgreichem Parse mit sinnvollen Werten befüllt.
bool fetchBtcMarketData(float &btcPriceEur, float &btcPriceUsd, double &btcMarketCapUsd)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return false;
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  if (!http.begin(secureClient, CFG_URL_COINGECKO))
  {
    Serial.println("HTTP begin fehlgeschlagen.");
    return false;
  }

  http.setTimeout(CFG_HTTP_TIMEOUT_MS);

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    Serial.printf("CoinGecko HTTP-Fehler: %d\n", httpCode);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<CFG_JSON_DOC_SIZE> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error)
  {
    Serial.printf("JSON-Fehler: %s\n", error.c_str());
    return false;
  }

  btcPriceEur = doc["bitcoin"]["eur"] | NAN;
  btcPriceUsd = doc["bitcoin"]["usd"] | NAN;
  btcMarketCapUsd = doc["bitcoin"]["usd_market_cap"] | NAN;

  if (isnan(btcPriceEur) || isnan(btcPriceUsd) || isnan(btcMarketCapUsd))
  {
    Serial.println("CoinGecko-Daten unvollständig.");
    return false;
  }

  return true;
}

// Holt die aktuelle Bitcoin-Blockhöhe.
// Verwendet einen sehr einfachen Endpoint, der nur die Blockhöhe als Text liefert.
bool fetchBtcBlockHeight(uint32_t &blockHeight)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return false;
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  if (!http.begin(secureClient, CFG_URL_BLOCK_HEIGHT))
  {
    Serial.println("Blockhöhe: HTTP begin fehlgeschlagen.");
    return false;
  }

  http.setTimeout(CFG_HTTP_TIMEOUT_MS);
  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    Serial.printf("Blockhöhe HTTP-Fehler: %d\n", httpCode);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  payload.trim();
  const long parsedHeight = payload.toInt();
  if (parsedHeight <= 0)
  {
    Serial.println("Blockhöhe konnte nicht geparst werden.");
    return false;
  }

  blockHeight = static_cast<uint32_t>(parsedHeight);
  return true;
}

// Synchronisiert die ESP32-Uhr via NTP.
// Rückgabe: true, wenn eine plausible Uhrzeit gelesen werden konnte.
bool syncClockFromNtp(tm &localTime)
{
  configTzTime(CFG_TZ_INFO, CFG_NTP_SERVER_PRIMARY, CFG_NTP_SERVER_SECONDARY);

  // Mehrere kurze Versuche statt eines langen Blockierens.
  for (int attempt = 0; attempt < CFG_NTP_MAX_ATTEMPTS; ++attempt)
  {
    if (getLocalTime(&localTime, CFG_NTP_SINGLE_ATTEMPT_TIMEOUT_MS))
    {
      return true;
    }
  }

  return false;
}

// Liefert das nächste Abruf-/Sleep-Intervall anhand der lokalen Stunde.
unsigned long getFetchIntervalMsByHour(int hour)
{
  if (hour >= CFG_DAY_START_HOUR && hour < CFG_EVENING_START_HOUR)
  {
    return CFG_FETCH_INTERVAL_DAY_MS;
  }

  if (hour >= CFG_EVENING_START_HOUR && hour < CFG_NIGHT_START_HOUR)
  {
    return CFG_FETCH_INTERVAL_EVENING_MS;
  }

  return CFG_FETCH_INTERVAL_NIGHT_MS;
}

// Hilfsfunktion für serielle Statusmeldungen.
const char *getTimeWindowLabelByHour(int hour)
{
  if (hour >= CFG_DAY_START_HOUR && hour < CFG_EVENING_START_HOUR)
  {
    return "Tag";
  }

  if (hour >= CFG_EVENING_START_HOUR && hour < CFG_NIGHT_START_HOUR)
  {
    return "Abend";
  }

  return "Nacht";
}

// -----------------------------------------------------------------------------
// Berechnungen / Formatierung
// -----------------------------------------------------------------------------
// Moscow Time = Satoshis pro 1 USD.
// Formel: 100.000.000 sats / BTC-Preis(USD).
uint32_t calculateMoscowTime(float btcPriceUsd)
{
  if (btcPriceUsd <= 0.0f)
  {
    return 0;
  }

  const float satsPerUsd = 100000000.0f / btcPriceUsd;
  return static_cast<uint32_t>(satsPerUsd + 0.5f);
}

// Prozentuale Kursänderung zwischen zwei EUR-Kursen.
// Mathematik: |neu - alt| / alt * 100
// Nachlesen Prozentrechnung:
// https://de.wikipedia.org/wiki/Prozentrechnung
float calculatePercentChange(float oldValue, float newValue)
{
  if (oldValue <= 0.0f)
  {
    return INFINITY;
  }

  return fabsf((newValue - oldValue) / oldValue) * 100.0f;
}

// Entscheidet, ob das Display aktualisiert werden soll.
// Regeln:
// 1) Beim ersten gültigen Kurs immer aktualisieren.
// 2) Ohne gültige Preisdaten ebenfalls aktualisieren (damit "n/a" sichtbar wird).
// 3) Danach nur, wenn die Kursänderung >= CFG_DISPLAY_UPDATE_THRESHOLD_PERCENT ist.
//
// Warum Referenzkurs im RTC-Speicher?
// - Nach Deep Sleep startet der ESP32 softwareseitig neu.
// - RTC_DATA_ATTR-Variablen bleiben aber erhalten und eignen sich ideal,
//   um "letzten angezeigten Kurs" über Sleep-Zyklen hinweg zu merken.
bool shouldUpdateDisplay(const BtcSnapshot &snapshot, float &outPercentChange)
{
  outPercentChange = 0.0f;

  if (!snapshot.pricesOk)
  {
    return true;
  }

  if (!g_hasDisplayedPrice || isnan(g_lastDisplayedPriceEur))
  {
    outPercentChange = INFINITY;
    return true;
  }

  outPercentChange = calculatePercentChange(g_lastDisplayedPriceEur, snapshot.btcPriceEuro);
  return outPercentChange >= CFG_DISPLAY_UPDATE_THRESHOLD_PERCENT;
}

// Formatiert die Market Cap in Milliarden USD für eine kompakte Display-Ausgabe.
String formatMarketCapBillions(double marketCapUsd)
{
  if (isnan(marketCapUsd))
  {
    return "n/a";
  }

  const double valueBn = marketCapUsd / 1000000000.0;
  return String(valueBn, 2) + "B";
}

// Zeichnet einen String rechtsbündig an einer definierten rechten Kante.
// Praktisch für Werte mit wechselnder Ziffernlänge (z. B. BTC-Preis).
void drawRightAlignedText(const String &text, int16_t rightX, int16_t baselineY)
{
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  display.getTextBounds(text, 0, baselineY, &x1, &y1, &w, &h);

  const int16_t startX = rightX - static_cast<int16_t>(w);
  display.setCursor(startX, baselineY);
  display.print(text);
}

// -----------------------------------------------------------------------------
// Display-Ausgabe
// -----------------------------------------------------------------------------
// Statischer Layout-Teil: Rahmen, Titel, Labels.
// Dieser Teil ändert sich zwischen den Zyklen nicht.
void drawStaticLayout()
{
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.drawRect(0, 0, display.width(), display.height(), GxEPD_BLACK);
  display.drawLine(0, 26, display.width(), 26, GxEPD_BLACK);

  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(10, 18);
  display.println("BTC INFO");

  display.setFont(&FreeMono9pt7b);
  display.setCursor(8, 48);
  display.println("EUR:");
  display.setCursor(8, 72);
  display.println("USD:");
  display.setCursor(8, 96);
  display.println("MCap:");
  display.setCursor(8, 120);
  display.println("Block:");
  display.setCursor(8, 144);
  display.println("Moskau:");
}

// Dynamischer Layout-Teil: Messwerte.
// Der EUR-Wert wird hervorgehoben (größere Schrift und rechtsbündig),
// damit der wichtigste Kurswert direkt auffällt.
void drawDynamicValues(const BtcSnapshot &snapshot)
{
  display.setTextColor(GxEPD_BLACK);

  display.setFont(&FreeMonoBold12pt7b);
  if (snapshot.pricesOk)
  {
    drawRightAlignedText(String(snapshot.btcPriceEuro, 2), 194, 56);
  }
  else
  {
    drawRightAlignedText("n/a", 194, 56);
  }

  display.setFont(&FreeMono9pt7b);
  display.setCursor(78, 72);
  if (snapshot.pricesOk)
  {
    display.printf("%.2f", snapshot.btcPriceUsd);
  }
  else
  {
    display.print("n/a");
  }

  display.setCursor(78, 96);
  if (snapshot.pricesOk)
  {
    display.print(formatMarketCapBillions(snapshot.btcMarketCapUsd));
  }
  else
  {
    display.print("n/a");
  }

  display.setCursor(78, 120);
  if (snapshot.blockHeightOk)
  {
    display.printf("%lu", static_cast<unsigned long>(snapshot.btcBlockHeight));
  }
  else
  {
    display.print("n/a");
  }

  display.setCursor(78, 144);
  if (snapshot.pricesOk && snapshot.moscowTime > 0)
  {
    display.printf("%lu sat/$", static_cast<unsigned long>(snapshot.moscowTime));
  }
  else
  {
    display.print("n/a");
  }
}

// Führt die eigentliche e-Paper-Ausgabe aus.
// Wegen der Display-Technik erfolgt das Zeichnen seitenweise (firstPage/nextPage).
void drawBtcScreen(const BtcSnapshot &snapshot)
{
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do
  {
    drawStaticLayout();
    drawDynamicValues(snapshot);
  } while (display.nextPage());
}

// Serielle Diagnoseausgabe für schnelle Kontrolle im Monitor.
// Tipp für Anfänger:
// Mit `pio device monitor` kannst du den Ablauf Schritt für Schritt sehen.
void printSnapshot(const BtcSnapshot &snapshot)
{
  Serial.println("---- BTC Snapshot ----");
  if (snapshot.pricesOk)
  {
    Serial.printf("btc_price_euro: %.2f\n", snapshot.btcPriceEuro);
    Serial.printf("btc_preis_usd: %.2f\n", snapshot.btcPriceUsd);
    Serial.printf("btc_marktkapitlasierung: %.0f\n", snapshot.btcMarketCapUsd);
    Serial.printf("moskauzeit: %lu sat/$\n", static_cast<unsigned long>(snapshot.moscowTime));
  }
  else
  {
    Serial.println("Preis/MCap: n/a");
  }

  if (snapshot.blockHeightOk)
  {
    Serial.printf("btc_blockhoehe: %lu\n", static_cast<unsigned long>(snapshot.btcBlockHeight));
  }
  else
  {
    Serial.println("btc_blockhoehe: n/a");
  }
  Serial.println("----------------------");
}

// -----------------------------------------------------------------------------
// Hauptablauf
// -----------------------------------------------------------------------------
// Ablauf pro Wakeup:
// 1) Display initialisieren
// 2) WLAN verbinden
// 3) Uhrzeit (NTP) synchronisieren -> Intervall bestimmen
// 4) Daten abrufen + berechnen
// 5) Nur bei >= 0,5% Kursänderung Display aktualisieren
// 6) WLAN abschalten
// 7) Für das zeitabhängige Intervall in Deep Sleep gehen
void setup()
{
  Serial.begin(CFG_SERIAL_BAUD);
  delay(CFG_BOOT_DELAY_MS);

  // Display initialisieren.
  // Nachlesen GxEPD2: https://github.com/ZinggJM/GxEPD2
  display.init(CFG_DISPLAY_INIT_BAUD,
               CFG_DISPLAY_INIT_INITIAL,
               CFG_DISPLAY_INIT_RESET_DURATION,
               CFG_DISPLAY_INIT_PULDOWN_RST);

  BtcSnapshot snapshot = {
      NAN,
      NAN,
      NAN,
      0,
      0,
      false,
      false};

  unsigned long nextFetchIntervalMs = CFG_FETCH_INTERVAL_FALLBACK_MS;
  bool localTimeValid = false;
  tm localTime = {};

  // Wichtig für Anfänger:
  // Ohne WLAN keine API-Daten und auch keine NTP-Zeit.
  // Darum hängt die Intervallwahl (Tag/Abend/Nacht) von erfolgreichem WLAN ab.
  if (connectWifi())
  {
    // Uhrzeit holen, damit wir das passende Zeitfenster (Tag/Abend/Nacht) nutzen.
    localTimeValid = syncClockFromNtp(localTime);
    if (localTimeValid)
    {
      nextFetchIntervalMs = getFetchIntervalMsByHour(localTime.tm_hour);
      Serial.printf("Lokale Uhrzeit: %02d:%02d, Zeitfenster: %s\n",
                    localTime.tm_hour,
                    localTime.tm_min,
                    getTimeWindowLabelByHour(localTime.tm_hour));
    }
    else
    {
      Serial.println("NTP-Zeit nicht verfuegbar, nutze Fallback-Intervall.");
    }

    snapshot.pricesOk = fetchBtcMarketData(snapshot.btcPriceEuro, snapshot.btcPriceUsd, snapshot.btcMarketCapUsd);
    snapshot.blockHeightOk = fetchBtcBlockHeight(snapshot.btcBlockHeight);
    if (snapshot.pricesOk)
    {
      snapshot.moscowTime = calculateMoscowTime(snapshot.btcPriceUsd);
    }
  }

  printSnapshot(snapshot);

  float percentChange = 0.0f;
  const bool updateDisplay = shouldUpdateDisplay(snapshot, percentChange);

  if (updateDisplay)
  {
    if (isinf(percentChange))
    {
      Serial.println("Display-Update: erster gueltiger Kurs oder keine vorherige Anzeige.");
    }
    else
    {
      Serial.printf("Display-Update: Kursaenderung = %.3f%% (Schwelle %.2f%%).\n",
                    percentChange,
                    CFG_DISPLAY_UPDATE_THRESHOLD_PERCENT);
    }

    drawBtcScreen(snapshot);

    if (snapshot.pricesOk)
    {
      g_lastDisplayedPriceEur = snapshot.btcPriceEuro;
      g_hasDisplayedPrice = true;
    }
  }
  else
  {
    Serial.printf("Kein Display-Update: Kursaenderung = %.3f%% (< %.2f%%).\n",
                  percentChange,
                  CFG_DISPLAY_UPDATE_THRESHOLD_PERCENT);
  }

  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);

  // Deep Sleep: Mikrosekunden erwartet, deshalb *1000 von ms -> us.
  Serial.printf("Gehe in Deep Sleep fuer %lu Minuten...\n",
                static_cast<unsigned long>(nextFetchIntervalMs / 60000UL));
  Serial.flush();
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(nextFetchIntervalMs) * 1000ULL);
  esp_deep_sleep_start();
}

// loop bleibt leer, weil das Gerät nach setup() direkt in Deep Sleep geht.
void loop()
{
}
