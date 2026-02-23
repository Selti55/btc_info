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
#define CFG_JSON_DOC_SIZE_HISTORY 8192

#define CFG_URL_COINGECKO "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=eur,usd&include_market_cap=true"
#define CFG_URL_BLOCK_HEIGHT "https://blockchain.info/q/getblockcount"
#define CFG_URL_COINGECKO_MARKET_CHART_BASE "https://api.coingecko.com/api/v3/coins/bitcoin/market_chart?vs_currency="

#define CFG_CHART_HISTORY_DAYS 7
#define CFG_CHART_INTERVAL "daily"

#define CFG_CHART_CURRENCY_EUR 1
#define CFG_CHART_CURRENCY_USD 2
#define CFG_CHART_CURRENCY CFG_CHART_CURRENCY_EUR

#if CFG_CHART_CURRENCY == CFG_CHART_CURRENCY_EUR
#define CFG_CHART_VS_CURRENCY "eur"
#define CFG_CHART_CURRENCY_LABEL "EUR"
#elif CFG_CHART_CURRENCY == CFG_CHART_CURRENCY_USD
#define CFG_CHART_VS_CURRENCY "usd"
#define CFG_CHART_CURRENCY_LABEL "USD"
#else
#error "Ungueltiges CFG_CHART_CURRENCY. Erlaubt: CFG_CHART_CURRENCY_EUR / CFG_CHART_CURRENCY_USD"
#endif

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

// ------------------------
// Profile (Ein-Schalter für Gesamtverhalten)
// ------------------------
// Anwendung:
// - Nur CFG_PROFILE unten ändern.
// - Danach werden Intervallzeiten + Display-Schwelle automatisch gesetzt.
#define CFG_PROFILE_SPARSAM 1
#define CFG_PROFILE_AUSGEWOGEN 2
#define CFG_PROFILE_REAKTIV 3
#define CFG_PROFILE_NACHTMODUS 4

// Aktives Profil:
#define CFG_PROFILE CFG_PROFILE_NACHTMODUS

#if CFG_PROFILE == CFG_PROFILE_SPARSAM
#define CFG_PROFILE_NAME "Sparsam"
#define CFG_PROFILE_FETCH_INTERVAL_DAY_MS (20UL * 60UL * 1000UL)
#define CFG_PROFILE_FETCH_INTERVAL_EVENING_MS (45UL * 60UL * 1000UL)
#define CFG_PROFILE_FETCH_INTERVAL_NIGHT_MS (120UL * 60UL * 1000UL)
#define CFG_PROFILE_DISPLAY_UPDATE_THRESHOLD_PERCENT 0.8f
#elif CFG_PROFILE == CFG_PROFILE_AUSGEWOGEN
#define CFG_PROFILE_NAME "Ausgewogen"
#define CFG_PROFILE_FETCH_INTERVAL_DAY_MS (10UL * 60UL * 1000UL)
#define CFG_PROFILE_FETCH_INTERVAL_EVENING_MS (30UL * 60UL * 1000UL)
#define CFG_PROFILE_FETCH_INTERVAL_NIGHT_MS (90UL * 60UL * 1000UL)
#define CFG_PROFILE_DISPLAY_UPDATE_THRESHOLD_PERCENT 0.5f
#elif CFG_PROFILE == CFG_PROFILE_REAKTIV
#define CFG_PROFILE_NAME "Reaktiv"
#define CFG_PROFILE_FETCH_INTERVAL_DAY_MS (5UL * 60UL * 1000UL)
#define CFG_PROFILE_FETCH_INTERVAL_EVENING_MS (10UL * 60UL * 1000UL)
#define CFG_PROFILE_FETCH_INTERVAL_NIGHT_MS (30UL * 60UL * 1000UL)
#define CFG_PROFILE_DISPLAY_UPDATE_THRESHOLD_PERCENT 0.3f
#elif CFG_PROFILE == CFG_PROFILE_NACHTMODUS
#define CFG_PROFILE_NAME "Nachtmodus"
#define CFG_PROFILE_FETCH_INTERVAL_DAY_MS (10UL * 60UL * 1000UL)
#define CFG_PROFILE_FETCH_INTERVAL_EVENING_MS (30UL * 60UL * 1000UL)
#define CFG_PROFILE_FETCH_INTERVAL_NIGHT_MS (180UL * 60UL * 1000UL)
#define CFG_PROFILE_DISPLAY_UPDATE_THRESHOLD_PERCENT 0.6f
#else
#error "Ungueltiges CFG_PROFILE. Erlaubt: CFG_PROFILE_SPARSAM / AUSGEWOGEN / REAKTIV / NACHTMODUS"
#endif

// Aktive Werte aus dem Profil (werden im restlichen Code verwendet).
#define CFG_FETCH_INTERVAL_DAY_MS CFG_PROFILE_FETCH_INTERVAL_DAY_MS
#define CFG_FETCH_INTERVAL_EVENING_MS CFG_PROFILE_FETCH_INTERVAL_EVENING_MS
#define CFG_FETCH_INTERVAL_NIGHT_MS CFG_PROFILE_FETCH_INTERVAL_NIGHT_MS
#define CFG_FETCH_INTERVAL_FALLBACK_MS CFG_FETCH_INTERVAL_EVENING_MS
#define CFG_DISPLAY_UPDATE_THRESHOLD_PERCENT CFG_PROFILE_DISPLAY_UPDATE_THRESHOLD_PERCENT

// Dynamik für Deep Sleep (bezogen auf das zeitfensterbasierte Basis-Intervall):
// - Große Kursänderung  => kürzerer Sleep
// - Kleine Kursänderung => längerer Sleep
// Der Faktor wird auf diesen Bereich begrenzt, um Extremwerte zu vermeiden.
// Tuning nach Zeitfenster:
// - Tag: reaktiver
// - Abend: ausgewogen
// - Nacht: konservativer (mehr Ruhe)
#define CFG_DYNAMIC_SLEEP_MIN_FACTOR_DAY 0.30f
#define CFG_DYNAMIC_SLEEP_MAX_FACTOR_DAY 1.80f
#define CFG_DYNAMIC_SLEEP_MIN_FACTOR_EVENING 0.40f
#define CFG_DYNAMIC_SLEEP_MAX_FACTOR_EVENING 2.20f
#define CFG_DYNAMIC_SLEEP_MIN_FACTOR_NIGHT 0.85f
#define CFG_DYNAMIC_SLEEP_MAX_FACTOR_NIGHT 3.20f

// Nichtlineare Kurve:
// - Bei ratio > 1.0 (mehr Bewegung als Schwelle): stärkeres Komprimieren (reaktiver)
// - Bei ratio < 1.0 (weniger Bewegung als Schwelle): sanfteres Strecken (stabiler)
// Auswahl über EINEN Schalter:
// - RUHIG: weniger Reaktion (längere Laufzeit)
// - NORMAL: guter Standard
// - TRADING: schnellere Reaktion bei Volatilität
#define CFG_DYNAMIC_CURVE_PRESET_RUHIG 1
#define CFG_DYNAMIC_CURVE_PRESET_NORMAL 2
#define CFG_DYNAMIC_CURVE_PRESET_TRADING 3

#define CFG_DYNAMIC_CURVE_PRESET CFG_DYNAMIC_CURVE_PRESET_NORMAL

#if CFG_DYNAMIC_CURVE_PRESET == CFG_DYNAMIC_CURVE_PRESET_RUHIG
#define CFG_DYNAMIC_CURVE_PRESET_NAME "ruhig"
#define CFG_DYNAMIC_SLEEP_CURVE_EXP_HIGH 1.15f
#define CFG_DYNAMIC_SLEEP_CURVE_EXP_LOW 1.05f
#elif CFG_DYNAMIC_CURVE_PRESET == CFG_DYNAMIC_CURVE_PRESET_NORMAL
#define CFG_DYNAMIC_CURVE_PRESET_NAME "normal"
#define CFG_DYNAMIC_SLEEP_CURVE_EXP_HIGH 1.35f
#define CFG_DYNAMIC_SLEEP_CURVE_EXP_LOW 1.20f
#elif CFG_DYNAMIC_CURVE_PRESET == CFG_DYNAMIC_CURVE_PRESET_TRADING
#define CFG_DYNAMIC_CURVE_PRESET_NAME "trading"
#define CFG_DYNAMIC_SLEEP_CURVE_EXP_HIGH 1.70f
#define CFG_DYNAMIC_SLEEP_CURVE_EXP_LOW 1.30f
#else
#error "Ungueltiges CFG_DYNAMIC_CURVE_PRESET. Erlaubt: RUHIG / NORMAL / TRADING"
#endif

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
  float chartHistory7d[CFG_CHART_HISTORY_DAYS];
  uint8_t chartPointsCount;
  bool pricesOk;
  bool blockHeightOk;
  bool chartHistoryOk;
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

// Holt den BTC-Kursverlauf der letzten 7 Tage aus CoinGecko.
// Die Währung ist über CFG_CHART_CURRENCY umschaltbar (EUR/USD).
bool fetchBtcHistory7d(float outHistory[CFG_CHART_HISTORY_DAYS], uint8_t &outCount)
{
  outCount = 0;

  if (WiFi.status() != WL_CONNECTED)
  {
    return false;
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  const String chartUrl = String(CFG_URL_COINGECKO_MARKET_CHART_BASE) +
                          CFG_CHART_VS_CURRENCY +
                          "&days=" +
                          String(CFG_CHART_HISTORY_DAYS) +
                          "&interval=" +
                          CFG_CHART_INTERVAL;

  HTTPClient http;
  if (!http.begin(secureClient, chartUrl))
  {
    Serial.println("Chart: HTTP begin fehlgeschlagen.");
    return false;
  }

  http.setTimeout(CFG_HTTP_TIMEOUT_MS);
  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    Serial.printf("Chart HTTP-Fehler: %d\n", httpCode);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(CFG_JSON_DOC_SIZE_HISTORY);
  DeserializationError error = deserializeJson(doc, payload);
  if (error)
  {
    Serial.printf("Chart JSON-Fehler: %s\n", error.c_str());
    return false;
  }

  JsonArray prices = doc["prices"].as<JsonArray>();
  if (prices.isNull() || prices.size() == 0)
  {
    Serial.println("Chart: keine Preisdaten vorhanden.");
    return false;
  }

  const size_t totalPoints = prices.size();
  const size_t pointsToUse = min(static_cast<size_t>(CFG_CHART_HISTORY_DAYS), totalPoints);
  const size_t startIndex = totalPoints - pointsToUse;

  for (size_t i = 0; i < pointsToUse; ++i)
  {
    const float value = prices[startIndex + i][1] | NAN;
    if (isnan(value) || value <= 0.0f)
    {
      Serial.println("Chart: ungueltiger Preiswert.");
      return false;
    }

    outHistory[i] = value;
  }

  outCount = static_cast<uint8_t>(pointsToUse);
  return outCount > 1;
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

// Dynamische Anpassung des Deep-Sleep-Intervalls.
// Bezugspunkt ist das Basisintervall aus Tag/Abend/Nacht.
//
// Verhalten (nichtlinear):
// - pct ~= threshold  -> Faktor ~ 1.0 (Basisintervall)
// - pct  > threshold  -> Faktor < 1.0, mit stärkerer Reaktion bei großen Ausschlägen
// - pct  < threshold  -> Faktor > 1.0, mit sanfterer Verlängerung bei kleinen Änderungen
//
// Für den allerersten Messzyklus ohne Referenzkurs bleibt das Basisintervall aktiv.
unsigned long calculateDynamicSleepIntervalMs(unsigned long baseIntervalMs,
                                              float percentChange,
                                              bool hasReferencePrice,
                                              bool pricesOk,
                                              int localHour)
{
  if (!pricesOk || !hasReferencePrice || isnan(percentChange) || isinf(percentChange) ||
      CFG_DISPLAY_UPDATE_THRESHOLD_PERCENT <= 0.0f)
  {
    return baseIntervalMs;
  }

  float minFactor = CFG_DYNAMIC_SLEEP_MIN_FACTOR_EVENING;
  float maxFactor = CFG_DYNAMIC_SLEEP_MAX_FACTOR_EVENING;

  if (localHour >= CFG_DAY_START_HOUR && localHour < CFG_EVENING_START_HOUR)
  {
    minFactor = CFG_DYNAMIC_SLEEP_MIN_FACTOR_DAY;
    maxFactor = CFG_DYNAMIC_SLEEP_MAX_FACTOR_DAY;
  }
  else if (localHour >= CFG_NIGHT_START_HOUR || localHour < CFG_DAY_START_HOUR)
  {
    minFactor = CFG_DYNAMIC_SLEEP_MIN_FACTOR_NIGHT;
    maxFactor = CFG_DYNAMIC_SLEEP_MAX_FACTOR_NIGHT;
  }

  const float ratio = percentChange / CFG_DISPLAY_UPDATE_THRESHOLD_PERCENT;
  float factor = 1.0f;

  if (ratio >= 1.0f)
  {
    factor = 1.0f / powf(ratio, CFG_DYNAMIC_SLEEP_CURVE_EXP_HIGH);
  }
  else
  {
    factor = 1.0f + powf(1.0f - ratio, CFG_DYNAMIC_SLEEP_CURVE_EXP_LOW);
  }

  factor = constrain(factor, minFactor, maxFactor);
  return static_cast<unsigned long>(baseIntervalMs * factor);
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

// Zeichnet eine einfache 7-Tage-Liniengrafik im unteren Displaybereich.
void drawChartHistory7d(const BtcSnapshot &snapshot)
{
  const int16_t chartX = 8;
  const int16_t chartY = 156;
  const int16_t chartW = 184;
  const int16_t chartH = 36;

  display.drawRect(chartX, chartY, chartW, chartH, GxEPD_BLACK);

  display.setFont(&FreeMono9pt7b);
  display.setCursor(10, 154);
  display.print("7T ");
  display.print(CFG_CHART_CURRENCY_LABEL);

  if (!snapshot.chartHistoryOk || snapshot.chartPointsCount < 2)
  {
    display.setCursor(chartX + 58, chartY + 24);
    display.print("n/a");
    return;
  }

  float minValue = snapshot.chartHistory7d[0];
  float maxValue = snapshot.chartHistory7d[0];

  for (uint8_t i = 1; i < snapshot.chartPointsCount; ++i)
  {
    minValue = min(minValue, snapshot.chartHistory7d[i]);
    maxValue = max(maxValue, snapshot.chartHistory7d[i]);
  }

  float span = maxValue - minValue;
  if (span < 0.001f)
  {
    span = 1.0f;
  }

  const int16_t innerLeft = chartX + 2;
  const int16_t innerRight = chartX + chartW - 3;
  const int16_t innerTop = chartY + 2;
  const int16_t innerBottom = chartY + chartH - 3;

  for (uint8_t i = 1; i < snapshot.chartPointsCount; ++i)
  {
    const float prev = snapshot.chartHistory7d[i - 1];
    const float curr = snapshot.chartHistory7d[i];

    const float prevRatio = static_cast<float>(i - 1) / static_cast<float>(snapshot.chartPointsCount - 1);
    const float currRatio = static_cast<float>(i) / static_cast<float>(snapshot.chartPointsCount - 1);

    const int16_t x1 = innerLeft + static_cast<int16_t>((innerRight - innerLeft) * prevRatio + 0.5f);
    const int16_t x2 = innerLeft + static_cast<int16_t>((innerRight - innerLeft) * currRatio + 0.5f);

    const float prevYRatio = (prev - minValue) / span;
    const float currYRatio = (curr - minValue) / span;

    const int16_t y1 = innerBottom - static_cast<int16_t>((innerBottom - innerTop) * prevYRatio + 0.5f);
    const int16_t y2 = innerBottom - static_cast<int16_t>((innerBottom - innerTop) * currYRatio + 0.5f);

    display.drawLine(x1, y1, x2, y2, GxEPD_BLACK);
  }
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

  drawChartHistory7d(snapshot);
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

  if (snapshot.chartHistoryOk)
  {
    Serial.printf("chart_7d_%s: %u punkte\n", CFG_CHART_VS_CURRENCY, snapshot.chartPointsCount);
  }
  else
  {
    Serial.printf("chart_7d_%s: n/a\n", CFG_CHART_VS_CURRENCY);
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
// 5) Nur bei >= Schwellenwert Kursänderung Display aktualisieren
// 6) Deep-Sleep-Intervall dynamisch an Kursänderung anpassen
// 6) WLAN abschalten
// 7) Für das zeitabhängige Intervall in Deep Sleep gehen
void setup()
{
  Serial.begin(CFG_SERIAL_BAUD);
  delay(CFG_BOOT_DELAY_MS);

  Serial.printf("Aktives Profil: %s\n", CFG_PROFILE_NAME);
  Serial.printf("Dynamik-Preset: %s\n", CFG_DYNAMIC_CURVE_PRESET_NAME);

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
      {NAN, NAN, NAN, NAN, NAN, NAN, NAN},
      0,
      false,
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
    snapshot.chartHistoryOk = fetchBtcHistory7d(snapshot.chartHistory7d, snapshot.chartPointsCount);
    if (snapshot.pricesOk)
    {
      snapshot.moscowTime = calculateMoscowTime(snapshot.btcPriceUsd);
    }
  }

  printSnapshot(snapshot);

  float percentChange = 0.0f;
  const bool updateDisplay = shouldUpdateDisplay(snapshot, percentChange);
  const int sleepHour = localTimeValid ? localTime.tm_hour : CFG_EVENING_START_HOUR;

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

  const bool hasReferencePrice = g_hasDisplayedPrice && !isnan(g_lastDisplayedPriceEur);
  const unsigned long dynamicSleepIntervalMs = calculateDynamicSleepIntervalMs(nextFetchIntervalMs,
                                                                                percentChange,
                                                                                hasReferencePrice,
                                                                                snapshot.pricesOk,
                                                                                sleepHour);

  if (dynamicSleepIntervalMs != nextFetchIntervalMs)
  {
    Serial.printf("Dynamischer Sleep aktiv: Basis %lu min -> Neu %lu min (Delta %.3f%%).\n",
                  static_cast<unsigned long>(nextFetchIntervalMs / 60000UL),
                  static_cast<unsigned long>(dynamicSleepIntervalMs / 60000UL),
                  percentChange);
  }
  else
  {
    Serial.println("Dynamischer Sleep: Basisintervall unveraendert.");
  }

  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);

  // Deep Sleep: Mikrosekunden erwartet, deshalb *1000 von ms -> us.
  Serial.printf("Gehe in Deep Sleep fuer %lu Minuten...\n",
                static_cast<unsigned long>(dynamicSleepIntervalMs / 60000UL));
  Serial.flush();
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(dynamicSleepIntervalMs) * 1000ULL);
  esp_deep_sleep_start();
}

// loop bleibt leer, weil das Gerät nach setup() direkt in Deep Sleep geht.
void loop()
{
}
