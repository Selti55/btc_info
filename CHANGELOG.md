# Changelog

Alle nennenswerten Änderungen an diesem Projekt werden in dieser Datei dokumentiert.

Das Format orientiert sich an [Keep a Changelog](https://keepachangelog.com/de/1.1.0/)
und dieses Projekt verwendet [Semantic Versioning](https://semver.org/lang/de/).

## [Unreleased]

### Added
-

### Changed
-

### Fixed
-

## [0.3.12] - 2026-02-23

### Added
- README um eine kompakte Troubleshooting-Mini-Sektion ergänzt (WLAN, NTP-Zeit, Display-Update, Build-Fehler).

### Changed
- README auf Version `v0.3.12` aktualisiert.

## [0.3.11] - 2026-02-23

### Added
- README um Abschnitt „Quickstart in 60 Sekunden" ergänzt.

### Changed
- README auf Version `v0.3.11` aktualisiert.

## [0.3.10] - 2026-02-23

### Added
- README um eine kompakte TL;DR-Box ergänzt (Standardempfehlung: `CFG_PROFILE_NACHTMODUS`).

### Changed
- README auf Version `v0.3.10` aktualisiert.

## [0.3.9] - 2026-02-23

### Added
- README um eine kurze Entscheidungshilfe ergänzt: „Welchen Modus soll ich wählen?“ (3 Fragen + Empfehlung).

### Changed
- README auf Version `v0.3.9` aktualisiert.

## [0.3.8] - 2026-02-23

### Added
- README um klaren Abschnitt „Vor- und Nachteile der Modi" ergänzt (`sparsam`, `ausgewogen`, `reaktiv`, `nachtmodus`).

### Changed
- README dokumentiert den Default explizit als `CFG_PROFILE_NACHTMODUS`.
- README auf Version `v0.3.8` aktualisiert.

## [0.3.7] - 2026-02-23

### Changed
- Standardprofil in `src/main.cpp` auf `CFG_PROFILE_NACHTMODUS` gesetzt.
- README auf Version `v0.3.7` aktualisiert.

## [0.3.6] - 2026-02-23

### Added
- `src/main.cpp` um Ein-Schalter-Profilwahl erweitert (`CFG_PROFILE` mit `SPARSAM`, `AUSGEWOGEN`, `REAKTIV`, `NACHTMODUS`).
- Automatische Ableitung von Abrufintervallen und Display-Schwelle aus dem gewählten Profil ergänzt.
- Serielle Ausgabe zeigt aktives Profil beim Start.

### Changed
- README auf Version `v0.3.6` aktualisiert.
- README um Abschnitt zur Nutzung von `CFG_PROFILE` ergänzt.

## [0.3.5] - 2026-02-23

### Added
- README um Preset-Empfehlungen nach Akkutyp ergänzt (u. a. 18650 mit 2500/3000 mAh).
- Grobe Laufzeitbereiche pro Akku/Preset-Kombination ergänzt.

### Changed
- README auf Version `v0.3.5` aktualisiert.

## [0.3.4] - 2026-02-23

### Added
- Neues Preset `nachtmodus` in der README ergänzt (tagsüber ausgewogen, nachts besonders stromsparend).

### Changed
- README auf Version `v0.3.4` aktualisiert.

## [0.3.3] - 2026-02-23

### Added
- README um eine Preset-Tabelle erweitert: `sparsam`, `ausgewogen`, `reaktiv` mit direkt nutzbaren `CFG_*`-Werten.
- Zusätzliche Praxis-Hinweise ergänzt (erst mit `ausgewogen` starten, dann einzeln feinjustieren).

### Changed
- README auf Version `v0.3.3` aktualisiert.

## [0.3.2] - 2026-02-23

### Added
- README um eine kompakte Cheat-Sheet-Tabelle ergänzt: „Welchen `#define` ändere ich wofür?".
- Konkrete Beispielwerte für typische Tuning-Ziele ergänzt (Akkulaufzeit, Display-Refresh-Rate, WLAN-/HTTP-Robustheit).

### Changed
- README auf Version `v0.3.2` aktualisiert.

## [0.3.1] - 2026-02-23

### Added
- Zentrale Konfigurationssektion am Anfang von `src/main.cpp` eingeführt; Hauptparameter sind jetzt als `#define` gekennzeichnet (Pins, Intervalle, Schwellenwert, Timeouts, NTP/API-URLs).
- Detailliertere Anfänger-Kommentare im Code ergänzt, inklusive Verweise zum Nachlesen (Arduino `#define`, ESP32 Deep Sleep, NTP-Zeit, CoinGecko API).
- README um Abschnitt „Konfiguration für Einsteiger“ erweitert.

### Changed
- README auf Version `v0.3.1` aktualisiert.

## [0.3.0] - 2026-02-23

### Added
- Zeitabhängige Abrufintervalle integriert:
	- 07:00-18:00 Uhr: 10 Minuten
	- 18:00-22:00 Uhr: 30 Minuten
	- 22:00-07:00 Uhr: 90 Minuten
- NTP-Zeitsynchronisierung inkl. deutscher Sommer-/Winterzeit zur Intervallauswahl.
- RTC-gespeicherter Referenzkurs (`EUR`) für Display-Entscheidungen über Deep Sleep hinweg.
- README um eine Laufzeit-/Zyklen-Tabelle (konservativ/typisch/volatil) ergänzt.

### Changed
- Display wird nur noch aktualisiert, wenn die EUR-Kursänderung seit der letzten Anzeige >= 0,5 % ist.
- Blockhöhe ist kein Trigger mehr für Display-Refresh.
- `src/main.cpp` um zusätzliche Anfänger-Kommentare für Ablauf und Entscheidungslogik erweitert.
- README auf Version `v0.3.0` und neue Betriebslogik aktualisiert.

## [0.2.1] - 2026-02-22

### Added
- `src/main.cpp` detailliert kommentiert (Ablauf, Funktionen, Datenfluss).

### Changed
- BTC-EUR-Wert auf dem e-Paper hervorgehoben (größere Schrift, rechtsbündig).
- README auf Version `v0.2.1` und aktuellen Funktionsumfang aktualisiert.

## [0.2.0] - 2026-02-22

### Added
- WLAN-Konfiguration in `include/secrets.h` ausgelagert.
- `include/secrets.h` von Git-Tracking ausgeschlossen.
- BTC-Blockhöhe in die Datenerfassung aufgenommen.
- Moskauzeit (Sats pro USD) berechnet und angezeigt.
- Anzeige auf dem e-Paper in statischen und dynamischen Bereich aufgeteilt.
- Deep-Sleep-Zyklus über 5 Minuten nach jeder Aktualisierung integriert.

### Changed
- README auf aktuellen Funktionsumfang und Version `v0.2.0` aktualisiert.

## [0.1.1] - 2026-02-22

### Added
- Changelog-Grundgerüst (`CHANGELOG.md`) ergänzt.

## [0.1.0] - 2026-02-22

### Added
- Initiale Projektstruktur für PlatformIO/ESP32 angelegt.
- GitHub-Repository verbunden und erster Stand veröffentlicht.

[Unreleased]: https://github.com/Selti55/btc_info/compare/v0.3.12...HEAD
[0.3.12]: https://github.com/Selti55/btc_info/releases/tag/v0.3.12
[0.3.11]: https://github.com/Selti55/btc_info/releases/tag/v0.3.11
[0.3.10]: https://github.com/Selti55/btc_info/releases/tag/v0.3.10
[0.3.9]: https://github.com/Selti55/btc_info/releases/tag/v0.3.9
[0.3.8]: https://github.com/Selti55/btc_info/releases/tag/v0.3.8
[0.3.7]: https://github.com/Selti55/btc_info/releases/tag/v0.3.7
[0.3.6]: https://github.com/Selti55/btc_info/releases/tag/v0.3.6
[0.3.5]: https://github.com/Selti55/btc_info/releases/tag/v0.3.5
[0.3.4]: https://github.com/Selti55/btc_info/releases/tag/v0.3.4
[0.3.3]: https://github.com/Selti55/btc_info/releases/tag/v0.3.3
[0.3.2]: https://github.com/Selti55/btc_info/releases/tag/v0.3.2
[0.3.1]: https://github.com/Selti55/btc_info/releases/tag/v0.3.1
[0.3.0]: https://github.com/Selti55/btc_info/releases/tag/v0.3.0
[0.2.1]: https://github.com/Selti55/btc_info/releases/tag/v0.2.1
[0.2.0]: https://github.com/Selti55/btc_info/releases/tag/v0.2.0
[0.1.1]: https://github.com/Selti55/btc_info/releases/tag/v0.1.1
[0.1.0]: https://github.com/Selti55/btc_info/releases/tag/v0.1.0