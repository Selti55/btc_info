#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>

// Waveshare ESP32-S3 1.54" e-Paper (Standard-Pinbelegung, ggf. anpassen)
constexpr int PIN_EPD_CS = 10;
constexpr int PIN_EPD_DC = 11;
constexpr int PIN_EPD_RST = 12;
constexpr int PIN_EPD_BUSY = 13;

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));

void drawBootScreen()
{
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);

    display.setCursor(10, 35);
    display.println("ESP32-S3");
    display.setCursor(10, 60);
    display.println("Waveshare 1.54\"");
    display.setCursor(10, 85);
    display.println("e-Paper OK");
  } while (display.nextPage());
}

void setup()
{
  Serial.begin(115200);
  delay(200);

  display.init(115200, true, 2, false);
  drawBootScreen();

  Serial.println("Display initialisiert.");
}

void loop()
{
  delay(1000);
}
