# BTC Info – Waveshare ESP32-S3 1.54" e-Paper

Neues PlatformIO-Projekt als Startbasis für BTC-Infos auf einem Waveshare ESP32-S3 1.54" e-Paper.

Aktuelle Version: **v0.2.1**

## Enthalten

- Arduino + ESP32-S3 (`esp32-s3-devkitc-1`)
- Bibliothek `GxEPD2`
- CoinGecko BTC-Daten (EUR, USD, Market Cap)
- BTC-Blockhöhe + Moskauzeit (Sats pro USD)
- e-Paper-Ausgabe mit statischem und dynamischem Bereich
- Hervorgehobener BTC-EUR-Wert (größer, rechtsbündig)
- Deep-Sleep-Zyklus (5 Minuten)
- Ausführlich kommentierter `main.cpp` für einfachere Wartung

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

## CoinGecko (alle 5 Minuten)

In `src/main.cpp` werden beim Aufwachen folgende Werte geholt und ausgegeben. Danach geht der ESP32 für 5 Minuten in Deep Sleep:

- `btc_price_euro`
- `btc_preis_usd`
- `btc_marktkapitlasierung`
- `btc_blockhoehe`
- `moskauzeit` (Sats pro USD)

### WLAN eintragen

In `include/secrets.h` diese beiden Konstanten setzen:

- `WIFI_SSID`
- `WIFI_PASSWORD`

`include/secrets.h` ist in `.gitignore` eingetragen und wird nicht zu GitHub gepusht.
