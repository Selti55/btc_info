#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>
#include <esp_system.h>
#include "secrets.h"

// -----------------------------------------------------------------------------
// CODE-LANDKARTE (für schnellen Einstieg)
// -----------------------------------------------------------------------------
// 1) Konfiguration (#define):
//    Alle Standardwerte (Pins, API, Profile, Sleep-Logik, Portal) stehen ganz oben.
//
// 2) Runtime-Einstellungen (AppSettings + Preferences):
//    - loadSettingsFromPreferences(): lädt gespeicherte Werte oder Defaults.
//    - saveSettingsToPreferences(): speichert Formularwerte dauerhaft.
//    - sanitizeSettings(): begrenzt Werte auf sichere Bereiche.
//
// 3) Konfigurations-Portal (Reset-Setup):
//    - runConfigPortalIfNeeded(): startet SoftAP + lokale Webseite bei Reset.
//    - handleConfigSave(): übernimmt Formularwerte.
//    - handleFactoryReset(): löscht gespeicherte Einstellungen.
//
// 4) Datenabruf:
//    - fetchBtcMarketData(), fetchBtcBlockHeight(), fetchBtcHistory7d().
//
// 5) Berechnung + Anzeige:
//    - shouldUpdateDisplay(), calculateDynamicSleepIntervalMs().
//    - drawBtcScreen(): zeichnet Werte + 7-Tage-Chart auf ePaper.
//
// 6) setup()-Ablauf:
//    Einstellungen laden -> optional Portal -> WLAN/NTP -> Daten holen ->
//    ggf. Display-Update -> Deep Sleep.
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// KONFIGURATION (alle anpassbaren Parameter als #define)
// -----------------------------------------------------------------------------
// Nachlesen:
// - Arduino C/C++ Präprozessor (#define): https://www.arduino.cc/reference/en/language/structure/further-syntax/define/
// - ESP32 Deep Sleep: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/sleep_modes.html
// - NTP / Zeitfunktionen (ESP32 Arduino):
//   https://docs.espressif.com/projects/arduino-esp32/en/latest/api/time.html
// - CoinGecko API: https://www.coingecko.com/en/api/documentation
// - ESP32 WebServer (lokale Konfig-Seite):
//   https://github.com/espressif/arduino-esp32/tree/master/libraries/WebServer
// - ESP32 Preferences/NVS (dauerhafte Speicherung):
//   https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html
// - ESP32 SoftAP (eigenes WLAN des Geräts):
//   https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html
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

// Konfigurations-Portal nach Reset
// Verhalten:
// - true  -> Bei normalem Reset startet zuerst eine Setup-Webseite.
// - false -> Gerät startet sofort in den normalen Datenmodus.
#define CFG_CONFIG_PORTAL_ON_RESET true
#define CFG_CONFIG_PORTAL_TIMEOUT_MS 180000UL
#define CFG_CONFIG_PORTAL_AP_SSID "BTC-INFO-SETUP"
#define CFG_CONFIG_PORTAL_AP_PASSWORD "btcinfo24"

// Optional: Portal nur öffnen, wenn ein Taster beim Booten gehalten wird.
// -1 deaktiviert den Taster-Trigger.
#define CFG_CONFIG_PORTAL_TRIGGER_PIN 0
#define CFG_CONFIG_PORTAL_TRIGGER_ACTIVE_LOW true
#define CFG_CONFIG_PORTAL_TRIGGER_HOLD_MS 1200UL

// ------------------------
// WLAN / HTTP / API
// ------------------------
#define CFG_WIFI_CONNECT_TIMEOUT_MS 20000UL
#define CFG_WIFI_CONNECT_RETRY_DELAY_MS 500UL
#define CFG_HTTP_TIMEOUT_MS 10000UL
#define CFG_JSON_DOC_SIZE 512
#define CFG_JSON_DOC_SIZE_HISTORY 8192

#define CFG_URL_COINGECKO "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=eur,usd&include_market_cap=true"
#define CFG_URL_COINBASE_PRICE_USD "https://api.coinbase.com/v2/prices/BTC-USD/spot"
#define CFG_URL_COINBASE_PRICE_EUR "https://api.coinbase.com/v2/prices/BTC-EUR/spot"
#define CFG_URL_BLOCK_HEIGHT "https://blockchain.info/q/getblockcount"
#define CFG_URL_COINGECKO_MARKET_CHART_BASE "https://api.coingecko.com/api/v3/coins/bitcoin/market_chart?vs_currency="

#define CFG_HTTP_RETRY_COUNT 2
#define CFG_HTTP_RETRY_BACKOFF_MS 600UL

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

// Display-Lebensdauer / Ghosting:
// Alle N Display-Updates wird ein Full-Refresh erzwungen,
// dazwischen werden partielle Updates genutzt.
#define CFG_DISPLAY_FULL_REFRESH_EVERY_N_UPDATES 8

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

struct AppSettings
{
  // WLAN-Zugangsdaten, die das Gerät für Internet-Zugriff nutzt.
  String wifiSsid;
  String wifiPassword;

  // Logik-/Profil-Schalter.
  uint8_t profile;
  uint8_t dynamicPreset;
  uint8_t chartCurrency;

  // Zeitfenster (Tag/Abend/Nacht).
  int dayStartHour;
  int eveningStartHour;
  int nightStartHour;

  // Abrufintervalle je Zeitfenster.
  unsigned long fetchIntervalDayMs;
  unsigned long fetchIntervalEveningMs;
  unsigned long fetchIntervalNightMs;

  // Display-Update-Schwelle und Dynamikgrenzen.
  float displayUpdateThresholdPercent;
  float dynamicSleepMinFactorDay;
  float dynamicSleepMaxFactorDay;
  float dynamicSleepMinFactorEvening;
  float dynamicSleepMaxFactorEvening;
  float dynamicSleepMinFactorNight;
  float dynamicSleepMaxFactorNight;
  float dynamicSleepCurveExpHigh;
  float dynamicSleepCurveExpLow;
};

AppSettings g_settings;
Preferences g_preferences;
WebServer g_configServer(80);
bool g_configSaved = false;
bool g_factoryResetRequested = false;

const char *getChartVsCurrency(uint8_t chartCurrency);

// RTC_DATA_ATTR: Diese Variablen bleiben über Deep Sleep hinweg erhalten.
// So wissen wir beim nächsten Wakeup, welcher EUR-Kurs zuletzt wirklich
// auf dem Display sichtbar war.
RTC_DATA_ATTR float g_lastDisplayedPriceEur = NAN;
RTC_DATA_ATTR bool g_hasDisplayedPrice = false;
RTC_DATA_ATTR uint32_t g_displayUpdateCounter = 0;

struct Diagnostics
{
  int lastPriceHttpCode;
  int lastChartHttpCode;
  int lastBlockHttpCode;
  unsigned long lastPlannedSleepMs;
  bool lastPricesOk;
  bool lastChartOk;
  bool lastBlockOk;
};

Diagnostics g_diag = {-1, -1, -1, 0, false, false, false};

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

  if (g_settings.wifiSsid.length() == 0 || g_settings.wifiPassword.length() == 0)
  {
    Serial.println("WLAN nicht konfiguriert. Bitte SSID/Passwort im Konfig-Portal setzen.");
    return false;
  }

  Serial.printf("Verbinde mit WLAN: %s\n", g_settings.wifiSsid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(g_settings.wifiSsid.c_str(), g_settings.wifiPassword.c_str());

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
    g_diag.lastPriceHttpCode = -1;
    return false;
  }

  for (int attempt = 0; attempt <= CFG_HTTP_RETRY_COUNT; ++attempt)
  {
    WiFiClientSecure secureClient;
    secureClient.setInsecure();

    HTTPClient http;
    if (!http.begin(secureClient, CFG_URL_COINGECKO))
    {
      Serial.println("HTTP begin fehlgeschlagen.");
      continue;
    }

    http.setTimeout(CFG_HTTP_TIMEOUT_MS);

    const int httpCode = http.GET();
    g_diag.lastPriceHttpCode = httpCode;
    if (httpCode == HTTP_CODE_OK)
    {
      String payload = http.getString();
      http.end();

      StaticJsonDocument<CFG_JSON_DOC_SIZE> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (error)
      {
        Serial.printf("JSON-Fehler: %s\n", error.c_str());
      }
      else
      {
        btcPriceEur = doc["bitcoin"]["eur"] | NAN;
        btcPriceUsd = doc["bitcoin"]["usd"] | NAN;
        btcMarketCapUsd = doc["bitcoin"]["usd_market_cap"] | NAN;

        if (!isnan(btcPriceEur) && !isnan(btcPriceUsd))
        {
          if (isnan(btcMarketCapUsd))
          {
            btcMarketCapUsd = NAN;
          }

          return true;
        }
      }
    }
    else
    {
      Serial.printf("CoinGecko HTTP-Fehler: %d\n", httpCode);
      http.end();
    }

    if (attempt < CFG_HTTP_RETRY_COUNT)
    {
      delay(CFG_HTTP_RETRY_BACKOFF_MS * static_cast<unsigned long>(attempt + 1));
    }
  }

  Serial.println("Fallback: nutze Coinbase Spot Preise.");

  auto fetchCoinbasePrice = [](const char *url, float &outPrice) -> bool
  {
    for (int attempt = 0; attempt <= CFG_HTTP_RETRY_COUNT; ++attempt)
    {
      WiFiClientSecure secureClient;
      secureClient.setInsecure();

      HTTPClient http;
      if (!http.begin(secureClient, url))
      {
        continue;
      }

      http.setTimeout(CFG_HTTP_TIMEOUT_MS);
      const int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK)
      {
        String payload = http.getString();
        http.end();

        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, payload) == DeserializationError::Ok)
        {
          const char *amount = doc["data"]["amount"] | nullptr;
          if (amount != nullptr)
          {
            outPrice = String(amount).toFloat();
            if (outPrice > 0.0f)
            {
              return true;
            }
          }
        }
      }
      else
      {
        http.end();
      }

      if (attempt < CFG_HTTP_RETRY_COUNT)
      {
        delay(CFG_HTTP_RETRY_BACKOFF_MS * static_cast<unsigned long>(attempt + 1));
      }
    }

    return false;
  };

  float fallbackUsd = NAN;
  float fallbackEur = NAN;

  if (fetchCoinbasePrice(CFG_URL_COINBASE_PRICE_USD, fallbackUsd) && fetchCoinbasePrice(CFG_URL_COINBASE_PRICE_EUR, fallbackEur))
  {
    btcPriceUsd = fallbackUsd;
    btcPriceEur = fallbackEur;
    btcMarketCapUsd = NAN;
    g_diag.lastPriceHttpCode = 200;
    return true;
  }

  return false;
}

// Holt den BTC-Kursverlauf der letzten 7 Tage aus CoinGecko.
// Die Währung ist über CFG_CHART_CURRENCY umschaltbar (EUR/USD).
bool fetchBtcHistory7d(float outHistory[CFG_CHART_HISTORY_DAYS], uint8_t &outCount)
{
  outCount = 0;

  if (WiFi.status() != WL_CONNECTED)
  {
    g_diag.lastChartHttpCode = -1;
    return false;
  }

  const String chartUrl = String(CFG_URL_COINGECKO_MARKET_CHART_BASE) +
                          getChartVsCurrency(g_settings.chartCurrency) +
                          "&days=" +
                          String(CFG_CHART_HISTORY_DAYS) +
                          "&interval=" +
                          CFG_CHART_INTERVAL;

  for (int attempt = 0; attempt <= CFG_HTTP_RETRY_COUNT; ++attempt)
  {
    WiFiClientSecure secureClient;
    secureClient.setInsecure();

    HTTPClient http;
    if (!http.begin(secureClient, chartUrl))
    {
      Serial.println("Chart: HTTP begin fehlgeschlagen.");
      continue;
    }

    http.setTimeout(CFG_HTTP_TIMEOUT_MS);
    const int httpCode = http.GET();
    g_diag.lastChartHttpCode = httpCode;
    if (httpCode == HTTP_CODE_OK)
    {
      String payload = http.getString();
      http.end();

      DynamicJsonDocument doc(CFG_JSON_DOC_SIZE_HISTORY);
      DeserializationError error = deserializeJson(doc, payload);
      if (error)
      {
        Serial.printf("Chart JSON-Fehler: %s\n", error.c_str());
      }
      else
      {
        JsonArray prices = doc["prices"].as<JsonArray>();
        if (!prices.isNull() && prices.size() > 0)
        {
          const size_t totalPoints = prices.size();
          const size_t pointsToUse = min(static_cast<size_t>(CFG_CHART_HISTORY_DAYS), totalPoints);
          const size_t startIndex = totalPoints - pointsToUse;

          for (size_t i = 0; i < pointsToUse; ++i)
          {
            const float value = prices[startIndex + i][1] | NAN;
            if (isnan(value) || value <= 0.0f)
            {
              Serial.println("Chart: ungueltiger Preiswert.");
              outCount = 0;
              break;
            }

            outHistory[i] = value;
            outCount = static_cast<uint8_t>(i + 1);
          }

          if (outCount > 1)
          {
            return true;
          }
        }
      }
    }
    else
    {
      Serial.printf("Chart HTTP-Fehler: %d\n", httpCode);
      http.end();
    }

    if (attempt < CFG_HTTP_RETRY_COUNT)
    {
      delay(CFG_HTTP_RETRY_BACKOFF_MS * static_cast<unsigned long>(attempt + 1));
    }
  }

  return false;
}

// Holt die aktuelle Bitcoin-Blockhöhe.
// Verwendet einen sehr einfachen Endpoint, der nur die Blockhöhe als Text liefert.
bool fetchBtcBlockHeight(uint32_t &blockHeight)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    g_diag.lastBlockHttpCode = -1;
    return false;
  }

  for (int attempt = 0; attempt <= CFG_HTTP_RETRY_COUNT; ++attempt)
  {
    WiFiClientSecure secureClient;
    secureClient.setInsecure();

    HTTPClient http;
    if (!http.begin(secureClient, CFG_URL_BLOCK_HEIGHT))
    {
      Serial.println("Blockhöhe: HTTP begin fehlgeschlagen.");
      continue;
    }

    http.setTimeout(CFG_HTTP_TIMEOUT_MS);
    const int httpCode = http.GET();
    g_diag.lastBlockHttpCode = httpCode;
    if (httpCode == HTTP_CODE_OK)
    {
      String payload = http.getString();
      http.end();

      payload.trim();
      const long parsedHeight = payload.toInt();
      if (parsedHeight > 0)
      {
        blockHeight = static_cast<uint32_t>(parsedHeight);
        return true;
      }

      Serial.println("Blockhöhe konnte nicht geparst werden.");
    }
    else
    {
      Serial.printf("Blockhöhe HTTP-Fehler: %d\n", httpCode);
      http.end();
    }

    if (attempt < CFG_HTTP_RETRY_COUNT)
    {
      delay(CFG_HTTP_RETRY_BACKOFF_MS * static_cast<unsigned long>(attempt + 1));
    }
  }

  return false;
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

const char *getProfileName(uint8_t profile)
{
  switch (profile)
  {
  case CFG_PROFILE_SPARSAM:
    return "Sparsam";
  case CFG_PROFILE_AUSGEWOGEN:
    return "Ausgewogen";
  case CFG_PROFILE_REAKTIV:
    return "Reaktiv";
  case CFG_PROFILE_NACHTMODUS:
    return "Nachtmodus";
  default:
    return "Unbekannt";
  }
}

const char *getDynamicPresetName(uint8_t preset)
{
  switch (preset)
  {
  case CFG_DYNAMIC_CURVE_PRESET_RUHIG:
    return "ruhig";
  case CFG_DYNAMIC_CURVE_PRESET_NORMAL:
    return "normal";
  case CFG_DYNAMIC_CURVE_PRESET_TRADING:
    return "trading";
  default:
    return "normal";
  }
}

const char *getChartCurrencyLabel(uint8_t chartCurrency)
{
  return (chartCurrency == CFG_CHART_CURRENCY_USD) ? "USD" : "EUR";
}

const char *getChartVsCurrency(uint8_t chartCurrency)
{
  return (chartCurrency == CFG_CHART_CURRENCY_USD) ? "usd" : "eur";
}

unsigned long minutesToMs(uint16_t minutes)
{
  return static_cast<unsigned long>(minutes) * 60UL * 1000UL;
}

void applyProfileTemplateToSettings(uint8_t profile, AppSettings &settings)
{
  // Setzt ein komplettes Basis-Setup in einem Schritt.
  // Einsteiger-Tipp: hier ändern, wenn neue Profile hinzukommen sollen.
  settings.profile = profile;

  switch (profile)
  {
  case CFG_PROFILE_SPARSAM:
    settings.fetchIntervalDayMs = minutesToMs(20);
    settings.fetchIntervalEveningMs = minutesToMs(45);
    settings.fetchIntervalNightMs = minutesToMs(120);
    settings.displayUpdateThresholdPercent = 0.8f;
    break;
  case CFG_PROFILE_REAKTIV:
    settings.fetchIntervalDayMs = minutesToMs(5);
    settings.fetchIntervalEveningMs = minutesToMs(10);
    settings.fetchIntervalNightMs = minutesToMs(30);
    settings.displayUpdateThresholdPercent = 0.3f;
    break;
  case CFG_PROFILE_NACHTMODUS:
    settings.fetchIntervalDayMs = minutesToMs(10);
    settings.fetchIntervalEveningMs = minutesToMs(30);
    settings.fetchIntervalNightMs = minutesToMs(180);
    settings.displayUpdateThresholdPercent = 0.6f;
    break;
  case CFG_PROFILE_AUSGEWOGEN:
  default:
    settings.fetchIntervalDayMs = minutesToMs(10);
    settings.fetchIntervalEveningMs = minutesToMs(30);
    settings.fetchIntervalNightMs = minutesToMs(90);
    settings.displayUpdateThresholdPercent = 0.5f;
    settings.profile = CFG_PROFILE_AUSGEWOGEN;
    break;
  }
}

void applyDynamicPresetToSettings(uint8_t preset, AppSettings &settings)
{
  // Preset steuert nur die Kurven-Exponenten (nicht die Zeitfenster).
  settings.dynamicPreset = preset;

  switch (preset)
  {
  case CFG_DYNAMIC_CURVE_PRESET_RUHIG:
    settings.dynamicSleepCurveExpHigh = 1.15f;
    settings.dynamicSleepCurveExpLow = 1.05f;
    break;
  case CFG_DYNAMIC_CURVE_PRESET_TRADING:
    settings.dynamicSleepCurveExpHigh = 1.70f;
    settings.dynamicSleepCurveExpLow = 1.30f;
    break;
  case CFG_DYNAMIC_CURVE_PRESET_NORMAL:
  default:
    settings.dynamicSleepCurveExpHigh = 1.35f;
    settings.dynamicSleepCurveExpLow = 1.20f;
    settings.dynamicPreset = CFG_DYNAMIC_CURVE_PRESET_NORMAL;
    break;
  }
}

void sanitizeSettings(AppSettings &settings)
{
  // Schützt vor ungültigen Werten aus Web-Formular oder Speicher,
  // damit das Gerät immer mit sicheren Grenzen laufen kann.
  settings.chartCurrency = (settings.chartCurrency == CFG_CHART_CURRENCY_USD) ? CFG_CHART_CURRENCY_USD : CFG_CHART_CURRENCY_EUR;

  settings.dayStartHour = constrain(settings.dayStartHour, 0, 23);
  settings.eveningStartHour = constrain(settings.eveningStartHour, 0, 23);
  settings.nightStartHour = constrain(settings.nightStartHour, 0, 23);

  settings.fetchIntervalDayMs = constrain(settings.fetchIntervalDayMs, minutesToMs(1), minutesToMs(24 * 60));
  settings.fetchIntervalEveningMs = constrain(settings.fetchIntervalEveningMs, minutesToMs(1), minutesToMs(24 * 60));
  settings.fetchIntervalNightMs = constrain(settings.fetchIntervalNightMs, minutesToMs(1), minutesToMs(24 * 60));

  settings.displayUpdateThresholdPercent = constrain(settings.displayUpdateThresholdPercent, 0.1f, 20.0f);

  settings.dynamicSleepMinFactorDay = CFG_DYNAMIC_SLEEP_MIN_FACTOR_DAY;
  settings.dynamicSleepMaxFactorDay = CFG_DYNAMIC_SLEEP_MAX_FACTOR_DAY;
  settings.dynamicSleepMinFactorEvening = CFG_DYNAMIC_SLEEP_MIN_FACTOR_EVENING;
  settings.dynamicSleepMaxFactorEvening = CFG_DYNAMIC_SLEEP_MAX_FACTOR_EVENING;
  settings.dynamicSleepMinFactorNight = CFG_DYNAMIC_SLEEP_MIN_FACTOR_NIGHT;
  settings.dynamicSleepMaxFactorNight = CFG_DYNAMIC_SLEEP_MAX_FACTOR_NIGHT;
}

void loadSettingsFromPreferences()
{
  // Erst sinnvolle Defaults aus Compile-Time-Konfiguration laden,
  // danach ggf. durch persistente Werte überschreiben.
  g_settings.wifiSsid = WIFI_SSID;
  g_settings.wifiPassword = WIFI_PASSWORD;
  g_settings.profile = CFG_PROFILE;
  g_settings.dynamicPreset = CFG_DYNAMIC_CURVE_PRESET;
  g_settings.chartCurrency = CFG_CHART_CURRENCY;
  g_settings.dayStartHour = CFG_DAY_START_HOUR;
  g_settings.eveningStartHour = CFG_EVENING_START_HOUR;
  g_settings.nightStartHour = CFG_NIGHT_START_HOUR;

  applyProfileTemplateToSettings(g_settings.profile, g_settings);
  applyDynamicPresetToSettings(g_settings.dynamicPreset, g_settings);
  sanitizeSettings(g_settings);

  if (!g_preferences.begin("btc_cfg", true))
  {
    Serial.println("Preferences (read) konnte nicht geoeffnet werden.");
    return;
  }

  g_settings.wifiSsid = g_preferences.getString("wifi_ssid", g_settings.wifiSsid);
  g_settings.wifiPassword = g_preferences.getString("wifi_pwd", g_settings.wifiPassword);

  const uint8_t savedProfile = static_cast<uint8_t>(g_preferences.getUChar("profile", g_settings.profile));
  const uint8_t savedDynPreset = static_cast<uint8_t>(g_preferences.getUChar("dyn_preset", g_settings.dynamicPreset));
  const uint8_t savedChartCurrency = static_cast<uint8_t>(g_preferences.getUChar("chart_cur", g_settings.chartCurrency));

  applyProfileTemplateToSettings(savedProfile, g_settings);
  applyDynamicPresetToSettings(savedDynPreset, g_settings);

  g_settings.chartCurrency = savedChartCurrency;
  g_settings.dayStartHour = g_preferences.getInt("h_day", g_settings.dayStartHour);
  g_settings.eveningStartHour = g_preferences.getInt("h_even", g_settings.eveningStartHour);
  g_settings.nightStartHour = g_preferences.getInt("h_night", g_settings.nightStartHour);
  g_settings.fetchIntervalDayMs = static_cast<unsigned long>(g_preferences.getUInt("i_day_m", g_settings.fetchIntervalDayMs / 60000UL)) * 60000UL;
  g_settings.fetchIntervalEveningMs = static_cast<unsigned long>(g_preferences.getUInt("i_even_m", g_settings.fetchIntervalEveningMs / 60000UL)) * 60000UL;
  g_settings.fetchIntervalNightMs = static_cast<unsigned long>(g_preferences.getUInt("i_night_m", g_settings.fetchIntervalNightMs / 60000UL)) * 60000UL;
  g_settings.displayUpdateThresholdPercent = g_preferences.getFloat("disp_thr", g_settings.displayUpdateThresholdPercent);

  g_preferences.end();
  sanitizeSettings(g_settings);
}

void saveSettingsToPreferences(const AppSettings &settings)
{
  // Speichert alle Formularwerte dauerhaft in NVS.
  // Diese Werte werden beim nächsten Reset wieder als Default angezeigt.
  if (!g_preferences.begin("btc_cfg", false))
  {
    Serial.println("Preferences (write) konnte nicht geoeffnet werden.");
    return;
  }

  g_preferences.putString("wifi_ssid", settings.wifiSsid);
  g_preferences.putString("wifi_pwd", settings.wifiPassword);
  g_preferences.putUChar("profile", settings.profile);
  g_preferences.putUChar("dyn_preset", settings.dynamicPreset);
  g_preferences.putUChar("chart_cur", settings.chartCurrency);
  g_preferences.putInt("h_day", settings.dayStartHour);
  g_preferences.putInt("h_even", settings.eveningStartHour);
  g_preferences.putInt("h_night", settings.nightStartHour);
  g_preferences.putUInt("i_day_m", settings.fetchIntervalDayMs / 60000UL);
  g_preferences.putUInt("i_even_m", settings.fetchIntervalEveningMs / 60000UL);
  g_preferences.putUInt("i_night_m", settings.fetchIntervalNightMs / 60000UL);
  g_preferences.putFloat("disp_thr", settings.displayUpdateThresholdPercent);

  g_preferences.end();
}

String selectedIf(bool selected)
{
  return selected ? " selected" : "";
}

String escapeHtml(const String &input)
{
  String out = input;
  out.replace("&", "&amp;");
  out.replace("<", "&lt;");
  out.replace(">", "&gt;");
  out.replace("\"", "&quot;");
  return out;
}

bool readIntFormField(const String &field, int &outValue)
{
  if (!g_configServer.hasArg(field))
  {
    return false;
  }

  const String raw = g_configServer.arg(field);
  if (raw.length() == 0)
  {
    return false;
  }

  outValue = raw.toInt();
  return true;
}

bool readFloatFormField(const String &field, float &outValue)
{
  if (!g_configServer.hasArg(field))
  {
    return false;
  }

  const String raw = g_configServer.arg(field);
  if (raw.length() == 0)
  {
    return false;
  }

  outValue = raw.toFloat();
  return true;
}

bool areTimeWindowsPlausible(const AppSettings &settings)
{
  return settings.dayStartHour < settings.eveningStartHour && settings.eveningStartHour < settings.nightStartHour;
}

String buildErrorPage(const String &message)
{
  String html;
  html.reserve(1000);
  html += "<html><body><h3>Konfiguration ungueltig</h3><p>";
  html += escapeHtml(message);
  html += "</p><p><a href='/'>Zurueck</a></p></body></html>";
  return html;
}

String getUptimeString()
{
  const unsigned long totalSeconds = millis() / 1000UL;
  const unsigned long hours = totalSeconds / 3600UL;
  const unsigned long minutes = (totalSeconds % 3600UL) / 60UL;
  const unsigned long seconds = totalSeconds % 60UL;

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%luh %lum %lus", hours, minutes, seconds);
  return String(buffer);
}

String buildStatusPageHtml()
{
  String html;
  html.reserve(2200);
  html += "<html><body><h3>BTC Info Status</h3><table border='1' cellpadding='6' cellspacing='0'>";
  html += "<tr><td>Uptime</td><td>" + getUptimeString() + "</td></tr>";
  html += "<tr><td>WLAN RSSI</td><td>" + String(WiFi.RSSI()) + " dBm</td></tr>";
  html += "<tr><td>Letzter Price HTTP Code</td><td>" + String(g_diag.lastPriceHttpCode) + "</td></tr>";
  html += "<tr><td>Letzter Chart HTTP Code</td><td>" + String(g_diag.lastChartHttpCode) + "</td></tr>";
  html += "<tr><td>Letzter Block HTTP Code</td><td>" + String(g_diag.lastBlockHttpCode) + "</td></tr>";
  html += "<tr><td>Preisabruf OK</td><td>" + String(g_diag.lastPricesOk ? "ja" : "nein") + "</td></tr>";
  html += "<tr><td>Chartabruf OK</td><td>" + String(g_diag.lastChartOk ? "ja" : "nein") + "</td></tr>";
  html += "<tr><td>Blockabruf OK</td><td>" + String(g_diag.lastBlockOk ? "ja" : "nein") + "</td></tr>";
  html += "<tr><td>Geplantes Sleep-Intervall</td><td>" + String(g_diag.lastPlannedSleepMs / 60000UL) + " min</td></tr>";
  html += "</table><p><a href='/'>Zurueck</a></p></body></html>";
  return html;
}

String settingsToJson(const AppSettings &settings)
{
  DynamicJsonDocument doc(1024);
  doc["wifi_ssid"] = settings.wifiSsid;
  doc["wifi_pwd"] = settings.wifiPassword;
  doc["profile"] = settings.profile;
  doc["dyn_preset"] = settings.dynamicPreset;
  doc["chart_cur"] = settings.chartCurrency;
  doc["h_day"] = settings.dayStartHour;
  doc["h_even"] = settings.eveningStartHour;
  doc["h_night"] = settings.nightStartHour;
  doc["i_day_m"] = settings.fetchIntervalDayMs / 60000UL;
  doc["i_even_m"] = settings.fetchIntervalEveningMs / 60000UL;
  doc["i_night_m"] = settings.fetchIntervalNightMs / 60000UL;
  doc["disp_thr"] = settings.displayUpdateThresholdPercent;

  String out;
  serializeJsonPretty(doc, out);
  return out;
}

bool applySettingsFromJsonString(const String &jsonText, String &errorMessage)
{
  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, jsonText);
  if (err)
  {
    errorMessage = "JSON konnte nicht gelesen werden.";
    return false;
  }

  AppSettings updated = g_settings;

  updated.wifiSsid = doc["wifi_ssid"] | updated.wifiSsid;
  updated.wifiPassword = doc["wifi_pwd"] | updated.wifiPassword;

  applyProfileTemplateToSettings(doc["profile"] | updated.profile, updated);
  applyDynamicPresetToSettings(doc["dyn_preset"] | updated.dynamicPreset, updated);

  updated.chartCurrency = doc["chart_cur"] | updated.chartCurrency;
  updated.dayStartHour = doc["h_day"] | updated.dayStartHour;
  updated.eveningStartHour = doc["h_even"] | updated.eveningStartHour;
  updated.nightStartHour = doc["h_night"] | updated.nightStartHour;
  updated.fetchIntervalDayMs = minutesToMs(doc["i_day_m"] | static_cast<int>(updated.fetchIntervalDayMs / 60000UL));
  updated.fetchIntervalEveningMs = minutesToMs(doc["i_even_m"] | static_cast<int>(updated.fetchIntervalEveningMs / 60000UL));
  updated.fetchIntervalNightMs = minutesToMs(doc["i_night_m"] | static_cast<int>(updated.fetchIntervalNightMs / 60000UL));
  updated.displayUpdateThresholdPercent = doc["disp_thr"] | updated.displayUpdateThresholdPercent;

  sanitizeSettings(updated);
  if (!areTimeWindowsPlausible(updated))
  {
    errorMessage = "Zeitfenster ungueltig: Tag < Abend < Nacht muss gelten.";
    return false;
  }

  g_settings = updated;
  saveSettingsToPreferences(g_settings);
  return true;
}

bool isConfigPortalTriggerPressed()
{
  if (CFG_CONFIG_PORTAL_TRIGGER_PIN < 0)
  {
    return true;
  }

  pinMode(CFG_CONFIG_PORTAL_TRIGGER_PIN, INPUT_PULLUP);

  const unsigned long startMs = millis();
  while ((millis() - startMs) < CFG_CONFIG_PORTAL_TRIGGER_HOLD_MS)
  {
    const int level = digitalRead(CFG_CONFIG_PORTAL_TRIGGER_PIN);
    const bool pressed = CFG_CONFIG_PORTAL_TRIGGER_ACTIVE_LOW ? (level == LOW) : (level == HIGH);
    if (!pressed)
    {
      return false;
    }
    delay(10);
  }

  return true;
}

String buildConfigPageHtml()
{
  // HTML bewusst kompakt als String aufgebaut,
  // damit keine zusätzlichen Dateien/Filesystem nötig sind.
  String html;
  html.reserve(7600);
  html += "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>BTC Info Setup</title>";
  html += "<style>body{font-family:Arial,sans-serif;max-width:760px;margin:20px auto;padding:0 12px;}";
  html += "label{display:block;margin-top:12px;font-weight:600;}input,select{width:100%;padding:8px;margin-top:4px;}";
  html += "button{margin-top:16px;padding:10px 14px;font-weight:700;}button.danger{background:#fff;border:2px solid #111;}small{color:#555;}</style></head><body>";
  html += "<h2>BTC Info Konfiguration</h2><p>Werte werden dauerhaft gespeichert und beim nächsten Reset als Default geladen.</p>";
  html += "<p><a href='/status'>Statusseite</a> | <a href='/backup'>JSON-Backup herunterladen</a></p>";
  html += "<form method='POST' action='/save'>";

  html += "<label>WLAN SSID</label><input name='wifi_ssid' value='" + g_settings.wifiSsid + "'>";
  html += "<label>WLAN Passwort</label><input type='password' name='wifi_pwd' value='" + g_settings.wifiPassword + "'>";

  html += "<label>Profil</label><select name='profile'>";
  html += "<option value='1'" + selectedIf(g_settings.profile == CFG_PROFILE_SPARSAM) + ">Sparsam</option>";
  html += "<option value='2'" + selectedIf(g_settings.profile == CFG_PROFILE_AUSGEWOGEN) + ">Ausgewogen</option>";
  html += "<option value='3'" + selectedIf(g_settings.profile == CFG_PROFILE_REAKTIV) + ">Reaktiv</option>";
  html += "<option value='4'" + selectedIf(g_settings.profile == CFG_PROFILE_NACHTMODUS) + ">Nachtmodus</option>";
  html += "</select>";

  html += "<label>Dynamik-Preset</label><select name='dyn_preset'>";
  html += "<option value='1'" + selectedIf(g_settings.dynamicPreset == CFG_DYNAMIC_CURVE_PRESET_RUHIG) + ">ruhig</option>";
  html += "<option value='2'" + selectedIf(g_settings.dynamicPreset == CFG_DYNAMIC_CURVE_PRESET_NORMAL) + ">normal</option>";
  html += "<option value='3'" + selectedIf(g_settings.dynamicPreset == CFG_DYNAMIC_CURVE_PRESET_TRADING) + ">trading</option>";
  html += "</select>";

  html += "<label>Chart-Waehrung</label><select name='chart_cur'>";
  html += "<option value='1'" + selectedIf(g_settings.chartCurrency == CFG_CHART_CURRENCY_EUR) + ">EUR</option>";
  html += "<option value='2'" + selectedIf(g_settings.chartCurrency == CFG_CHART_CURRENCY_USD) + ">USD</option>";
  html += "</select>";

  html += "<label>Tag Startstunde (0-23)</label><input type='number' min='0' max='23' name='h_day' value='" + String(g_settings.dayStartHour) + "'>";
  html += "<label>Abend Startstunde (0-23)</label><input type='number' min='0' max='23' name='h_even' value='" + String(g_settings.eveningStartHour) + "'>";
  html += "<label>Nacht Startstunde (0-23)</label><input type='number' min='0' max='23' name='h_night' value='" + String(g_settings.nightStartHour) + "'>";

  html += "<label>Intervall Tag (Minuten)</label><input type='number' min='1' max='1440' name='i_day_m' value='" + String(g_settings.fetchIntervalDayMs / 60000UL) + "'>";
  html += "<label>Intervall Abend (Minuten)</label><input type='number' min='1' max='1440' name='i_even_m' value='" + String(g_settings.fetchIntervalEveningMs / 60000UL) + "'>";
  html += "<label>Intervall Nacht (Minuten)</label><input type='number' min='1' max='1440' name='i_night_m' value='" + String(g_settings.fetchIntervalNightMs / 60000UL) + "'>";

  html += "<label>Display-Schwelle (%)</label><input type='number' step='0.1' min='0.1' max='20' name='disp_thr' value='" + String(g_settings.displayUpdateThresholdPercent, 2) + "'>";

  html += "<button type='submit'>Speichern & Neustarten</button>";
  html += "</form>";

  html += "<h3>JSON Restore</h3>";
  html += "<form method='POST' action='/restore'>";
  html += "<label>Konfiguration als JSON</label><textarea name='config_json' rows='10' style='width:100%;font-family:monospace;'></textarea>";
  html += "<button type='submit'>JSON importieren & Neustarten</button>";
  html += "</form>";

  html += "<form method='POST' action='/factory-reset' onsubmit='return confirm(\"Alle gespeicherten Einstellungen wirklich loeschen?\");'>";
  html += "<button type='submit' class='danger'>Werkseinstellungen wiederherstellen</button>";
  html += "</form>";
  html += "<p><small>Hinweis Werksreset: Loescht gespeichertes WLAN, Profil, Dynamik, Chart-Waehrung, Zeitfenster, Intervalle und Schwellenwert.</small></p>";
  html += "<p><small>AP SSID: " CFG_CONFIG_PORTAL_AP_SSID " | URL: http://192.168.4.1</small></p></body></html>";
  return html;
}

void handleConfigSave()
{
  // Nimmt Formularwerte entgegen, validiert sie und speichert sie dauerhaft.
  AppSettings updated = g_settings;

  updated.wifiSsid = g_configServer.arg("wifi_ssid");
  updated.wifiPassword = g_configServer.arg("wifi_pwd");

  const uint8_t profile = static_cast<uint8_t>(g_configServer.arg("profile").toInt());
  applyProfileTemplateToSettings(profile, updated);

  const uint8_t dynPreset = static_cast<uint8_t>(g_configServer.arg("dyn_preset").toInt());
  applyDynamicPresetToSettings(dynPreset, updated);

  updated.chartCurrency = static_cast<uint8_t>(g_configServer.arg("chart_cur").toInt());
  updated.dayStartHour = g_configServer.arg("h_day").toInt();
  updated.eveningStartHour = g_configServer.arg("h_even").toInt();
  updated.nightStartHour = g_configServer.arg("h_night").toInt();

  int dayMinutes = 0;
  int eveningMinutes = 0;
  int nightMinutes = 0;
  if (!readIntFormField("i_day_m", dayMinutes) || !readIntFormField("i_even_m", eveningMinutes) || !readIntFormField("i_night_m", nightMinutes))
  {
    g_configServer.send(400, "text/html", buildErrorPage("Intervall-Felder fehlen oder sind leer."));
    return;
  }

  updated.fetchIntervalDayMs = minutesToMs(static_cast<uint16_t>(dayMinutes));
  updated.fetchIntervalEveningMs = minutesToMs(static_cast<uint16_t>(eveningMinutes));
  updated.fetchIntervalNightMs = minutesToMs(static_cast<uint16_t>(nightMinutes));

  float threshold = 0.0f;
  if (!readFloatFormField("disp_thr", threshold))
  {
    g_configServer.send(400, "text/html", buildErrorPage("Display-Schwelle fehlt oder ist ungueltig."));
    return;
  }
  updated.displayUpdateThresholdPercent = threshold;

  sanitizeSettings(updated);
  if (!areTimeWindowsPlausible(updated))
  {
    g_configServer.send(400, "text/html", buildErrorPage("Zeitfenster ungueltig: Tag < Abend < Nacht muss gelten."));
    return;
  }

  if (updated.wifiSsid.length() == 0)
  {
    g_configServer.send(400, "text/html", buildErrorPage("WLAN SSID darf nicht leer sein."));
    return;
  }

  g_settings = updated;
  saveSettingsToPreferences(g_settings);

  g_configSaved = true;
  g_configServer.send(200,
                      "text/html",
                      "<html><body><h3>Gespeichert.</h3><p>Der ESP32 startet neu.</p></body></html>");
}

void handleConfigBackup()
{
  g_configServer.send(200, "application/json", settingsToJson(g_settings));
}

void handleConfigRestore()
{
  if (!g_configServer.hasArg("config_json") || g_configServer.arg("config_json").length() == 0)
  {
    g_configServer.send(400, "text/html", buildErrorPage("JSON-Text fehlt."));
    return;
  }

  String errorMessage;
  if (!applySettingsFromJsonString(g_configServer.arg("config_json"), errorMessage))
  {
    g_configServer.send(400, "text/html", buildErrorPage(errorMessage));
    return;
  }

  g_configSaved = true;
  g_factoryResetRequested = false;
  g_configServer.send(200,
                      "text/html",
                      "<html><body><h3>JSON importiert.</h3><p>Der ESP32 startet neu.</p></body></html>");
}

void handleFactoryReset()
{
  // Löscht den kompletten Preferences-Namespace des Projekts.
  // Danach greifen wieder die Compile-Time-Defaults aus dem Code.
  if (!g_preferences.begin("btc_cfg", false))
  {
    g_configServer.send(500, "text/html", "<html><body><h3>Fehler</h3><p>Preferences konnten nicht geoeffnet werden.</p></body></html>");
    return;
  }

  g_preferences.clear();
  g_preferences.end();

  g_factoryResetRequested = true;
  g_configSaved = true;

  g_configServer.send(200,
                      "text/html",
                      "<html><body><h3>Werkseinstellungen wiederhergestellt.</h3><p>Der ESP32 startet neu.</p></body></html>");
}

bool shouldStartConfigPortalOnThisBoot()
{
  // Portal nur bei "echtem" Reset starten.
  // Bei Wakeup aus Timer-Deep-Sleep soll der normale Zyklus direkt laufen.
  if (!CFG_CONFIG_PORTAL_ON_RESET)
  {
    return false;
  }

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER)
  {
    return false;
  }

  return isConfigPortalTriggerPressed();
}

void runConfigPortalIfNeeded()
{
  // Startet ein lokales WLAN (SoftAP) und stellt eine kleine Setup-Webseite bereit.
  // Nach Speichern oder Werksreset wird neu gestartet.
  if (!shouldStartConfigPortalOnThisBoot())
  {
    return;
  }

  Serial.println("Starte Konfigurations-Portal (Reset erkannt)...");

  WiFi.mode(WIFI_AP);
  if (String(CFG_CONFIG_PORTAL_AP_PASSWORD).length() >= 8)
  {
    WiFi.softAP(CFG_CONFIG_PORTAL_AP_SSID, CFG_CONFIG_PORTAL_AP_PASSWORD);
  }
  else
  {
    WiFi.softAP(CFG_CONFIG_PORTAL_AP_SSID);
  }

  Serial.print("Portal-IP: ");
  Serial.println(WiFi.softAPIP());

  g_configSaved = false;
  g_factoryResetRequested = false;
  g_configServer.on("/", HTTP_GET, []()
                    { g_configServer.send(200, "text/html", buildConfigPageHtml()); });
  g_configServer.on("/status", HTTP_GET, []()
                    { g_configServer.send(200, "text/html", buildStatusPageHtml()); });
  g_configServer.on("/backup", HTTP_GET, handleConfigBackup);
  g_configServer.on("/restore", HTTP_POST, handleConfigRestore);
  g_configServer.on("/save", HTTP_POST, handleConfigSave);
  g_configServer.on("/factory-reset", HTTP_POST, handleFactoryReset);
  g_configServer.begin();

  const unsigned long startMs = millis();
  while (!g_configSaved && (millis() - startMs) < CFG_CONFIG_PORTAL_TIMEOUT_MS)
  {
    g_configServer.handleClient();
    delay(10);
  }

  g_configServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);

  if (g_configSaved)
  {
    if (g_factoryResetRequested)
    {
      Serial.println("Werkseinstellungen wiederhergestellt. Neustart...");
    }
    else
    {
      Serial.println("Konfiguration gespeichert. Neustart...");
    }
    delay(800);
    ESP.restart();
  }
  else
  {
    Serial.println("Konfigurations-Portal Timeout. Starte mit vorhandenen Einstellungen.");
  }
}

// Liefert das nächste Abruf-/Sleep-Intervall anhand der lokalen Stunde.
unsigned long getFetchIntervalMsByHour(int hour)
{
  if (hour >= g_settings.dayStartHour && hour < g_settings.eveningStartHour)
  {
    return g_settings.fetchIntervalDayMs;
  }

  if (hour >= g_settings.eveningStartHour && hour < g_settings.nightStartHour)
  {
    return g_settings.fetchIntervalEveningMs;
  }

  return g_settings.fetchIntervalNightMs;
}

// Hilfsfunktion für serielle Statusmeldungen.
const char *getTimeWindowLabelByHour(int hour)
{
  if (hour >= g_settings.dayStartHour && hour < g_settings.eveningStartHour)
  {
    return "Tag";
  }

  if (hour >= g_settings.eveningStartHour && hour < g_settings.nightStartHour)
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
      g_settings.displayUpdateThresholdPercent <= 0.0f)
  {
    return baseIntervalMs;
  }

  float minFactor = g_settings.dynamicSleepMinFactorEvening;
  float maxFactor = g_settings.dynamicSleepMaxFactorEvening;

  if (localHour >= g_settings.dayStartHour && localHour < g_settings.eveningStartHour)
  {
    minFactor = g_settings.dynamicSleepMinFactorDay;
    maxFactor = g_settings.dynamicSleepMaxFactorDay;
  }
  else if (localHour >= g_settings.nightStartHour || localHour < g_settings.dayStartHour)
  {
    minFactor = g_settings.dynamicSleepMinFactorNight;
    maxFactor = g_settings.dynamicSleepMaxFactorNight;
  }

  const float ratio = percentChange / g_settings.displayUpdateThresholdPercent;
  float factor = 1.0f;

  if (ratio >= 1.0f)
  {
    factor = 1.0f / powf(ratio, g_settings.dynamicSleepCurveExpHigh);
  }
  else
  {
    factor = 1.0f + powf(1.0f - ratio, g_settings.dynamicSleepCurveExpLow);
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
  return outPercentChange >= g_settings.displayUpdateThresholdPercent;
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
  display.print(getChartCurrencyLabel(g_settings.chartCurrency));

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
void drawBtcScreen(const BtcSnapshot &snapshot, bool forceFullRefresh)
{
  display.setRotation(0);
  if (forceFullRefresh)
  {
    display.setFullWindow();
  }
  else
  {
    display.setPartialWindow(0, 27, display.width(), display.height() - 27);
  }

  display.firstPage();
  do
  {
    if (forceFullRefresh)
    {
      drawStaticLayout();
      drawDynamicValues(snapshot);
    }
    else
    {
      display.fillRect(0, 27, display.width(), display.height() - 27, GxEPD_WHITE);
      display.setFont(&FreeMono9pt7b);
      display.setTextColor(GxEPD_BLACK);
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
      drawDynamicValues(snapshot);
    }
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
    Serial.printf("chart_7d_%s: %u punkte\n", getChartVsCurrency(g_settings.chartCurrency), snapshot.chartPointsCount);
  }
  else
  {
    Serial.printf("chart_7d_%s: n/a\n", getChartVsCurrency(g_settings.chartCurrency));
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

  loadSettingsFromPreferences();
  runConfigPortalIfNeeded();

  Serial.printf("Aktives Profil: %s\n", getProfileName(g_settings.profile));
  Serial.printf("Dynamik-Preset: %s\n", getDynamicPresetName(g_settings.dynamicPreset));
  Serial.printf("Chart-Waehrung: %s\n", getChartCurrencyLabel(g_settings.chartCurrency));
  Serial.printf("Konfig-Portal AP: %s (Passwort gesetzt: %s)\n",
                CFG_CONFIG_PORTAL_AP_SSID,
                (String(CFG_CONFIG_PORTAL_AP_PASSWORD).length() >= 8) ? "ja" : "nein");

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

  unsigned long nextFetchIntervalMs = g_settings.fetchIntervalEveningMs;
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

    g_diag.lastPricesOk = snapshot.pricesOk;
    g_diag.lastBlockOk = snapshot.blockHeightOk;
    g_diag.lastChartOk = snapshot.chartHistoryOk;

    if (snapshot.pricesOk)
    {
      snapshot.moscowTime = calculateMoscowTime(snapshot.btcPriceUsd);
    }
  }

  printSnapshot(snapshot);

  float percentChange = 0.0f;
  const bool updateDisplay = shouldUpdateDisplay(snapshot, percentChange);
  const int sleepHour = localTimeValid ? localTime.tm_hour : g_settings.eveningStartHour;

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
                    g_settings.displayUpdateThresholdPercent);
    }

    const bool forceFullRefresh = (g_displayUpdateCounter % CFG_DISPLAY_FULL_REFRESH_EVERY_N_UPDATES) == 0;
    drawBtcScreen(snapshot, forceFullRefresh);
    g_displayUpdateCounter++;

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
                  g_settings.displayUpdateThresholdPercent);
  }

  const bool hasReferencePrice = g_hasDisplayedPrice && !isnan(g_lastDisplayedPriceEur);
  const unsigned long dynamicSleepIntervalMs = calculateDynamicSleepIntervalMs(nextFetchIntervalMs,
                                                                                percentChange,
                                                                                hasReferencePrice,
                                                                                snapshot.pricesOk,
                                                                                sleepHour);
  g_diag.lastPlannedSleepMs = dynamicSleepIntervalMs;

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
