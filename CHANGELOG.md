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

[Unreleased]: https://github.com/Selti55/btc_info/compare/v0.2.1...HEAD
[0.2.1]: https://github.com/Selti55/btc_info/releases/tag/v0.2.1
[0.2.0]: https://github.com/Selti55/btc_info/releases/tag/v0.2.0
[0.1.1]: https://github.com/Selti55/btc_info/releases/tag/v0.1.1
[0.1.0]: https://github.com/Selti55/btc_info/releases/tag/v0.1.0