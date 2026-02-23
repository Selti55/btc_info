# BTC Info – Waveshare ESP32-S3 1.54" e-Paper

Neues PlatformIO-Projekt als Startbasis für BTC-Infos auf einem Waveshare ESP32-S3 1.54" e-Paper.

Aktuelle Version: **v0.3.3**

## Enthalten

- Arduino + ESP32-S3 (`esp32-s3-devkitc-1`)
- Bibliothek `GxEPD2`
- CoinGecko BTC-Daten (EUR, USD, Market Cap)
- BTC-Blockhöhe + Moskauzeit (Sats pro USD)
- e-Paper-Ausgabe mit statischem und dynamischem Bereich
- Hervorgehobener BTC-EUR-Wert (größer, rechtsbündig)
- Zeitabhängiger Deep-Sleep-Zyklus (Tag/Abend/Nacht)
- Display-Update nur bei Kursänderung >= 0,5 %
- Zentrale Konfiguration am Anfang von `main.cpp` (alle Hauptparameter als `#define`)
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

## Abruf- und Display-Logik

In `src/main.cpp` werden beim Aufwachen folgende Werte geholt und ausgegeben:

- `btc_price_euro`
- `btc_preis_usd`
- `btc_marktkapitlasierung`
- `btc_blockhoehe`
- `moskauzeit` (Sats pro USD)

### Zeitfenster für den Datenabruf

- **07:00-18:00 Uhr:** alle 10 Minuten
- **18:00-22:00 Uhr:** alle 30 Minuten
- **22:00-07:00 Uhr:** alle 90 Minuten

### Display-Update-Schwelle

Das e-Paper wird nur dann neu gezeichnet, wenn die EUR-Kursänderung seit der letzten **angezeigten** Messung mindestens **0,5 %** beträgt.

- Blockhöhe wird weiterhin abgefragt und im Snapshot geführt.
- Die Blockhöhe ist **kein** Trigger mehr für ein Display-Update.

## Konfiguration für Einsteiger

Alle wichtigen Stellschrauben stehen gesammelt am **Anfang** von `src/main.cpp` als `#define`:

- Display-Pins (`CFG_PIN_*`)
- API-/HTTP-Timeouts (`CFG_HTTP_TIMEOUT_MS`, `CFG_WIFI_CONNECT_TIMEOUT_MS`)
- Zeitfenster (`CFG_FETCH_INTERVAL_*`, `CFG_*_HOUR`)
- Display-Schwelle (`CFG_DISPLAY_UPDATE_THRESHOLD_PERCENT`)
- NTP-/Zeitzonen-Einstellungen (`CFG_TZ_INFO`, `CFG_NTP_SERVER_*`)

Damit kann man das Verhalten ändern, ohne tiefer in die Logik eingreifen zu müssen.

### Cheat-Sheet: Welchen `#define` ändere ich wofür?

| Ziel | Relevanter `#define` | Typischer Wert | Wirkung |
| --- | --- | --- | --- |
| Tagsüber seltener abrufen | `CFG_FETCH_INTERVAL_DAY_MS` | `15UL * 60UL * 1000UL` | Weniger WLAN-Aufweckvorgänge, längere Akku-Laufzeit |
| Abends dichter abrufen | `CFG_FETCH_INTERVAL_EVENING_MS` | `15UL * 60UL * 1000UL` | Mehr Aktualität zwischen 18:00 und 22:00 |
| Nachts sehr sparsam | `CFG_FETCH_INTERVAL_NIGHT_MS` | `120UL * 60UL * 1000UL` | Stärkste Akku-Einsparung in der Nacht |
| Zeitfenster verschieben | `CFG_DAY_START_HOUR`, `CFG_EVENING_START_HOUR`, `CFG_NIGHT_START_HOUR` | `6 / 17 / 22` | Startzeiten für Tag/Abend/Nacht ändern |
| Display häufiger aktualisieren | `CFG_DISPLAY_UPDATE_THRESHOLD_PERCENT` | `0.4f` | Schon kleine Kursänderungen führen zu Refresh |
| Display stärker schonen | `CFG_DISPLAY_UPDATE_THRESHOLD_PERCENT` | `0.7f` | Weniger Refreshs, längere Display-Lebensdauer |
| WLAN robuster bei schwachem Empfang | `CFG_WIFI_CONNECT_TIMEOUT_MS` | `30000UL` | Mehr Zeit zum Verbinden, weniger Fehlzyklen |
| API robuster bei langsamer Antwort | `CFG_HTTP_TIMEOUT_MS` | `15000UL` | Weniger Abbrüche bei langsamer Internetverbindung |
| Fallback-Verhalten bei fehlender Uhrzeit | `CFG_FETCH_INTERVAL_FALLBACK_MS` | `CFG_FETCH_INTERVAL_EVENING_MS` | Intervall, wenn NTP-Zeit nicht verfügbar ist |

Hinweis: Erst einen Parameter ändern, dann 1-2 Tage beobachten (Display-Refresh-Rate, Akkuverbrauch, WLAN-Stabilität).

### Presets: sparsam, ausgewogen, reaktiv

Diese Presets sind fertige Startpunkte. Werte direkt in den passenden `CFG_*`-Defines setzen.

| Preset | `CFG_FETCH_INTERVAL_DAY_MS` | `CFG_FETCH_INTERVAL_EVENING_MS` | `CFG_FETCH_INTERVAL_NIGHT_MS` | `CFG_DISPLAY_UPDATE_THRESHOLD_PERCENT` | Typisches Ziel |
| --- | --- | --- | --- | --- | --- |
| Sparsam | `20UL * 60UL * 1000UL` | `45UL * 60UL * 1000UL` | `120UL * 60UL * 1000UL` | `0.8f` | Maximale Akku- und Display-Schonung |
| Ausgewogen | `10UL * 60UL * 1000UL` | `30UL * 60UL * 1000UL` | `90UL * 60UL * 1000UL` | `0.5f` | Gute Balance aus Aktualität und Laufzeit |
| Reaktiv | `5UL * 60UL * 1000UL` | `10UL * 60UL * 1000UL` | `30UL * 60UL * 1000UL` | `0.3f` | Möglichst schnelle sichtbare Kursreaktion |

Praxis-Tipp: Erst mit Ausgewogen starten. Danach nur eine Stellschraube gleichzeitig ändern und 24-48 Stunden beobachten.

## Grobe Laufzeit-Abschätzung (2500 mAh, 18650)

Die Tabelle ist als praxisnahe Orientierung gedacht (WLAN-Qualität, API-Antwortzeiten und Temperatur beeinflussen die Werte):

| Szenario | Polling/Tag | Display-Refresh/Tag (0,5%-Schwelle) | Akku-Laufzeit | Display-Zyklen/Jahr |
| --- | ---: | ---: | ---: | ---: |
| Bisher (fix 5 min, immer Refresh) | 288 | 288 | ca. 16-24 Tage | ca. 105.000 |
| Neu konservativ (ruhiger Markt) | 80 | 12-20 | ca. 70-95 Tage | ca. 4.400-7.300 |
| Neu typisch | 80 | 20-45 | ca. 58-86 Tage | ca. 7.300-16.400 |
| Neu volatil | 80 | 45-70 | ca. 50-78 Tage | ca. 16.400-25.600 |

## Empfehlungen

- Diese Konfiguration ist ein sehr guter Kompromiss aus Aktualität, Akku und Display-Schonung.
- Wenn noch mehr Laufzeit nötig ist: Tag-Intervall auf 15 Minuten erhöhen.
- Falls dir zu wenige Display-Updates erscheinen: Schwelle auf 0,4 % senken.
- Falls dir Stabilität wichtiger ist: Schwelle auf 0,7 % erhöhen.

### WLAN eintragen

In `include/secrets.h` diese beiden Konstanten setzen:

- `WIFI_SSID`
- `WIFI_PASSWORD`

`include/secrets.h` ist in `.gitignore` eingetragen und wird nicht zu GitHub gepusht.
