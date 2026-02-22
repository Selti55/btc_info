#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

// Waveshare ESP32-S3 1.54" e-Paper (Standard-Pinbelegung, ggf. anpassen)
constexpr int PIN_EPD_CS = 10;
constexpr int PIN_EPD_DC = 11;
constexpr int PIN_EPD_RST = 12;
constexpr int PIN_EPD_BUSY = 13;

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));

constexpr unsigned long FETCH_INTERVAL_MS = 5UL * 60UL * 1000UL;

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
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < 20000)
  {
    delay(500);
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

bool fetchBtcMarketData(float &btcPriceEur, float &btcPriceUsd, double &btcMarketCapUsd)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return false;
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  const char *url = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=eur,usd&include_market_cap=true";
  if (!http.begin(secureClient, url))
  {
    Serial.println("HTTP begin fehlgeschlagen.");
    return false;
  }

  http.setTimeout(10000);

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    Serial.printf("CoinGecko HTTP-Fehler: %d\n", httpCode);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<512> doc;
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

bool fetchBtcBlockHeight(uint32_t &blockHeight)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return false;
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  const char *url = "https://blockchain.info/q/getblockcount";
  if (!http.begin(secureClient, url))
  {
    Serial.println("Blockhöhe: HTTP begin fehlgeschlagen.");
    return false;
  }

  http.setTimeout(10000);
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

uint32_t calculateMoscowTime(float btcPriceUsd)
{
  if (btcPriceUsd <= 0.0f)
  {
    return 0;
  }

  const float satsPerUsd = 100000000.0f / btcPriceUsd;
  return static_cast<uint32_t>(satsPerUsd + 0.5f);
}

String formatMarketCapBillions(double marketCapUsd)
{
  if (isnan(marketCapUsd))
  {
    return "n/a";
  }

  const double valueBn = marketCapUsd / 1000000000.0;
  return String(valueBn, 2) + "B";
}

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

void drawDynamicValues(const BtcSnapshot &snapshot)
{
  display.setFont(&FreeMono9pt7b);
  display.setTextColor(GxEPD_BLACK);

  display.setCursor(78, 48);
  if (snapshot.pricesOk)
  {
    display.printf("%.2f", snapshot.btcPriceEuro);
  }
  else
  {
    display.print("n/a");
  }

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

void setup()
{
  Serial.begin(115200);
  delay(300);

  display.init(115200, true, 2, false);

  BtcSnapshot snapshot = {
      NAN,
      NAN,
      NAN,
      0,
      0,
      false,
      false};

  if (connectWifi())
  {
    snapshot.pricesOk = fetchBtcMarketData(snapshot.btcPriceEuro, snapshot.btcPriceUsd, snapshot.btcMarketCapUsd);
    snapshot.blockHeightOk = fetchBtcBlockHeight(snapshot.btcBlockHeight);
    if (snapshot.pricesOk)
    {
      snapshot.moscowTime = calculateMoscowTime(snapshot.btcPriceUsd);
    }
  }

  printSnapshot(snapshot);
  drawBtcScreen(snapshot);

  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);

  Serial.println("Gehe in Deep Sleep fuer 5 Minuten...");
  Serial.flush();
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(FETCH_INTERVAL_MS) * 1000ULL);
  esp_deep_sleep_start();
}

void loop()
{
}
