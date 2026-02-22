# BTC Info – Waveshare ESP32-S3 1.54" e-Paper

Neues PlatformIO-Projekt als Startbasis für BTC-Infos auf einem Waveshare ESP32-S3 1.54" e-Paper.

## Enthalten

- Arduino + ESP32-S3 (`esp32-s3-devkitc-1`)
- Bibliothek `GxEPD2`
- Start-Screen-Test in `src/main.cpp`

## Wichtige Pins

Aktuell in `src/main.cpp` hinterlegt:

- `CS = GPIO10`
- `DC = GPIO11`
- `RST = GPIO12`
- `BUSY = GPIO13`

Wenn dein Board eine andere Belegung hat, nur diese Konstanten anpassen.

## Build & Upload

```bash
pio run
pio run -t upload
pio device monitor
```

## Nächster Schritt

Als nächstes kann ich dir direkt die BTC-Preisabfrage (z. B. CoinGecko API über WLAN) plus Anzeige auf dem e-Paper einbauen.
