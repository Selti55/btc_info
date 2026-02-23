# BTC Info – Waveshare ESP32-S3 1.54" e-Paper

Neues PlatformIO-Projekt als Startbasis für BTC-Infos auf einem Waveshare ESP32-S3 1.54" e-Paper.

Aktuelle Version: **v0.3.19**

> **TL;DR**
> Wenn du nicht lange einstellen willst: nutze den Default `CFG_PROFILE_NACHTMODUS`.
> Das ist in der Praxis meist der beste Kompromiss aus Aktualität am Tag und Stromsparen in der Nacht.

## Inhaltsverzeichnis

- [Quickstart in 60 Sekunden](#quickstart-in-60-sekunden)
- [Troubleshooting](#troubleshooting)
- [Logik](#logik)
- [Konfiguration](#konfiguration)
- [Presets](#presets)
- [Akku-Empfehlung](#akku-empfehlung)

## Quickstart in 60 Sekunden

1. WLAN in `include/secrets.h` eintragen (`WIFI_SSID`, `WIFI_PASSWORD`).
2. Optional Profil wählen in `src/main.cpp` über `#define CFG_PROFILE ...`.
3. Firmware bauen: `pio run`.
4. Auf das Board flashen: `pio run -t upload`.
5. Live-Logs prüfen: `pio device monitor` (zeigt u. a. aktives Profil und Display-Updates).

## Troubleshooting

### Wenn A, dann B (ultrakurz)

- **Wenn** `WLAN-Verbindung fehlgeschlagen.` **dann** WLAN-Daten prüfen + 2.4 GHz aktivieren.
- **Wenn** `NTP-Zeit nicht verfuegbar...` **dann** Internet prüfen (Fallback ist normal).
- **Wenn** oft `Kein Display-Update...` **dann** Schwelle ist vermutlich zu hoch oder Markt ruhig.

- **Kein WLAN:** `WIFI_SSID` und `WIFI_PASSWORD` in `include/secrets.h` prüfen; Router/Signalstärke testen.
- **Keine NTP-Zeit:** Internetverbindung prüfen; Gerät fällt auf Fallback-Intervall zurück.
- **Kein Display-Update:** prüfen, ob Kursänderung die gesetzte Schwelle erreicht (`CFG_DISPLAY_UPDATE_THRESHOLD_PERCENT`).
- **Build-Fehler:** zuerst `pio run` erneut starten; danach Bibliotheken und `platformio.ini` prüfen.

### Beispiel-Logzeilen (und Bedeutung)

- `NTP-Zeit nicht verfuegbar, nutze Fallback-Intervall.` → Keine gültige Uhrzeit vom NTP-Server; Gerät nutzt `CFG_FETCH_INTERVAL_FALLBACK_MS`.
- `Kein Display-Update: Kursaenderung = 0.213% (< 0.600%).` → Alles ok; Schwelle wurde bewusst nicht erreicht, daher kein Refresh.
- `Display-Update: Kursaenderung = 0.942% (Schwelle 0.600%).` → Alles ok; Schwelle überschritten, Display wird neu gezeichnet.
- `WLAN-Verbindung fehlgeschlagen.` → `WIFI_SSID`/`WIFI_PASSWORD` in `include/secrets.h` prüfen, 2.4-GHz-WLAN aktivieren, Abstand/Signalstärke am Router testen.

### Was ist normal? / Wann eingreifen?

- **Normal:** Viele `Kein Display-Update...`-Meldungen bei höherer Schwelle oder ruhigem Markt.
- **Normal:** Gelegentlich `NTP-Zeit nicht verfuegbar...` bei kurzer Internetstörung.
- **Eingreifen:** `WLAN-Verbindung fehlgeschlagen.` über mehrere Zyklen hintereinander.
- **Eingreifen:** Dauerhaft nur `n/a`-Werte bei Preis/Blockhöhe trotz stabilem WLAN.

## Enthalten

- Arduino + ESP32-S3 (`esp32-s3-devkitc-1`)
- Bibliothek `GxEPD2`
- CoinGecko BTC-Daten (EUR, USD, Market Cap)
- BTC-Blockhöhe + Moskauzeit (Sats pro USD)
- e-Paper-Ausgabe mit statischem und dynamischem Bereich
- Hervorgehobener BTC-EUR-Wert (größer, rechtsbündig)
- Zeitabhängiger Deep-Sleep-Zyklus (Tag/Abend/Nacht)
- Dynamischer Deep-Sleep-Faktor nach Kursänderung (nichtlineare Kurve)
- Ein-Schalter-Dynamik-Preset (`ruhig` / `normal` / `trading`)
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

## Logik

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

### Dynamischer Deep Sleep (neu)

Das Basis-Intervall aus dem Zeitfenster (Tag/Abend/Nacht) wird zusätzlich dynamisch skaliert:

- **hohe prozentuale Kursänderung** → **kürzerer** Deep Sleep
- **niedrige prozentuale Kursänderung** → **längerer** Deep Sleep

Die Dynamik arbeitet mit einer **nichtlinearen Kurve** und separaten Grenzen für Tag/Abend/Nacht.
So bleibt das System tagsüber reaktiv und nachts deutlich ruhiger.

## Konfiguration

Alle wichtigen Stellschrauben stehen gesammelt am **Anfang** von `src/main.cpp` als `#define`:

- Display-Pins (`CFG_PIN_*`)
- API-/HTTP-Timeouts (`CFG_HTTP_TIMEOUT_MS`, `CFG_WIFI_CONNECT_TIMEOUT_MS`)
- Zeitfenster (`CFG_FETCH_INTERVAL_*`, `CFG_*_HOUR`)
- Display-Schwelle (`CFG_DISPLAY_UPDATE_THRESHOLD_PERCENT`)
- NTP-/Zeitzonen-Einstellungen (`CFG_TZ_INFO`, `CFG_NTP_SERVER_*`)
- Profil-Auswahl (`CFG_PROFILE`)
- Dynamik-Preset-Auswahl (`CFG_DYNAMIC_CURVE_PRESET`)

Damit kann man das Verhalten ändern, ohne tiefer in die Logik eingreifen zu müssen.

### Ein-Schalter-Profilwahl (`CFG_PROFILE`)

Du kannst das Gesamtverhalten jetzt mit **einer** Einstellung steuern:

- `CFG_PROFILE_SPARSAM`
- `CFG_PROFILE_AUSGEWOGEN`
- `CFG_PROFILE_REAKTIV`
- `CFG_PROFILE_NACHTMODUS`

In `src/main.cpp` einfach die Zeile anpassen:

`#define CFG_PROFILE CFG_PROFILE_NACHTMODUS`

**Standard ab Version v0.3.7: `CFG_PROFILE_NACHTMODUS`**

Der Code setzt damit automatisch:

- `CFG_FETCH_INTERVAL_DAY_MS`
- `CFG_FETCH_INTERVAL_EVENING_MS`
- `CFG_FETCH_INTERVAL_NIGHT_MS`
- `CFG_DISPLAY_UPDATE_THRESHOLD_PERCENT`

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

### Presets

Diese Presets sind fertige Startpunkte. Werte direkt in den passenden `CFG_*`-Defines setzen.

| Preset | `CFG_FETCH_INTERVAL_DAY_MS` | `CFG_FETCH_INTERVAL_EVENING_MS` | `CFG_FETCH_INTERVAL_NIGHT_MS` | `CFG_DISPLAY_UPDATE_THRESHOLD_PERCENT` | Typisches Ziel |
| --- | --- | --- | --- | --- | --- |
| Sparsam | `20UL * 60UL * 1000UL` | `45UL * 60UL * 1000UL` | `120UL * 60UL * 1000UL` | `0.8f` | Maximale Akku- und Display-Schonung |
| Ausgewogen | `10UL * 60UL * 1000UL` | `30UL * 60UL * 1000UL` | `90UL * 60UL * 1000UL` | `0.5f` | Gute Balance aus Aktualität und Laufzeit |
| Reaktiv | `5UL * 60UL * 1000UL` | `10UL * 60UL * 1000UL` | `30UL * 60UL * 1000UL` | `0.3f` | Möglichst schnelle sichtbare Kursreaktion |
| Nachtmodus | `10UL * 60UL * 1000UL` | `30UL * 60UL * 1000UL` | `180UL * 60UL * 1000UL` | `0.6f` | Tagsüber normal, nachts besonders stromsparend |

Praxis-Tipp: Erst mit Ausgewogen starten. Danach nur eine Stellschraube gleichzeitig ändern und 24-48 Stunden beobachten.

### Dynamik-Presets (`CFG_DYNAMIC_CURVE_PRESET`)

Zusätzlich zu `CFG_PROFILE` gibt es für die nichtlineare Sleep-Kurve einen Ein-Schalter:

- `CFG_DYNAMIC_CURVE_PRESET_RUHIG`
- `CFG_DYNAMIC_CURVE_PRESET_NORMAL`
- `CFG_DYNAMIC_CURVE_PRESET_TRADING`

Empfehlung für den Alltag: `CFG_DYNAMIC_CURVE_PRESET_NORMAL`.
`TRADING` reagiert aggressiver bei Volatilität, `RUHIG` ist akkusparender.

### Vor- und Nachteile der Modi

#### Sparsam
- **Vorteile:** maximale Akku-Laufzeit, sehr wenige Display-Refreshs, ideal für lange Offline-Phasen.
- **Nachteile:** deutlich langsamere Reaktion auf Kursbewegungen, weniger „Live“-Gefühl.

#### Ausgewogen
- **Vorteile:** guter Mittelweg zwischen Aktualität und Verbrauch, alltagstauglich für die meisten Setups.
- **Nachteile:** nicht so sparsam wie `sparsam`, nicht so schnell wie `reaktiv`.

#### Reaktiv
- **Vorteile:** schnellste sichtbare Kursreaktion, häufige Updates für aktive Beobachtung.
- **Nachteile:** kürzere Akku-Laufzeit, deutlich mehr Display-Refresh-Zyklen.

#### Nachtmodus
- **Vorteile:** tagsüber brauchbare Aktualität, nachts starke Einsparung; sehr guter Praxis-Kompromiss.
- **Nachteile:** nachts trägere Anzeige, bei nächtlicher Volatilität weniger Zwischenstände sichtbar.

### Welchen Modus soll ich wählen? (3 schnelle Fragen)

1. **Willst du maximale Laufzeit und schaust nur gelegentlich?** → `sparsam`
2. **Willst du einen stabilen Alltag-Kompromiss?** → `ausgewogen`
3. **Ist dir schnelle Reaktion wichtiger als Akku?** → `reaktiv`

Wenn du nachts besonders sparen willst, tagsüber aber normal schauen möchtest, nimm `nachtmodus` (Default).

### Akku-Empfehlung

Die Werte sind grobe Praxisbereiche und hängen stark von WLAN-Qualität, Temperatur und API-Latenz ab.

| Akku | Empfohlenes Preset | Typische Laufzeit | Kommentar |
| --- | --- | --- | --- |
| 18650 2500 mAh | Ausgewogen | ca. 58-86 Tage | Guter Startpunkt für Alltag und stabile Anzeige |
| 18650 2500 mAh | Nachtmodus | ca. 65-95 Tage | Wenn nachts maximale Einsparung wichtig ist |
| 18650 3000 mAh | Ausgewogen | ca. 70-103 Tage | Ähnliches Verhalten wie 2500 mAh, nur mit Reserve |
| 18650 3000 mAh | Sparsam | ca. 85-125 Tage | Fokus auf maximale Laufzeit statt Reaktivität |
| Powerbank/Netzbetrieb | Reaktiv | stark abhängig von Quelle | Sinnvoll, wenn schnelle Kursreaktion wichtiger als Akku ist |

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
