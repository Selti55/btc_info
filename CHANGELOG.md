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

[Unreleased]: https://github.com/Selti55/btc_info/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/Selti55/btc_info/releases/tag/v0.3.0
[0.2.1]: https://github.com/Selti55/btc_info/releases/tag/v0.2.1
[0.2.0]: https://github.com/Selti55/btc_info/releases/tag/v0.2.0
[0.1.1]: https://github.com/Selti55/btc_info/releases/tag/v0.1.1
[0.1.0]: https://github.com/Selti55/btc_info/releases/tag/v0.1.0