# Architektur

**Deutsch** | [English](ARCHITECTURE.md)

## Virtuelles Dokument

`Document` verwendet eine Piece-Table. Ein Piece verweist entweder auf einen unveraenderten Bytebereich der Originaldatei oder auf neu eingefuegte Bytes. Die Originaldatei bleibt geoeffnet und wird ueber einen kleinen 1-MiB-Cache gelesen. Die Dateigroesse bestimmt daher nicht den RAM-Verbrauch.

## Anzeige

Der native Win32-Renderer materialisiert nur sichtbare Textzeilen oder Hex-Zeilen. Scrollen, Syntax-Highlighting, Zeilennummern und Statusinformationen arbeiten auf diesem Fenster. UTF-8-BOM-Bytes werden beim Oeffnen erkannt und aus dem logischen Dokument ausgeblendet.

## Encodings

Die Textansicht unterstuetzt UTF-8 und die aktive Windows-ANSI-Codepage. Die UTF-8-BOM-Ausgabe ist unabhaengig auswaehlbar und wird nach automatischer Erkennung beibehalten. Die Hexansicht zeigt immer die logischen Inhaltsbytes ohne UTF-8-BOM.

## Bearbeitung und Speichern

Einfuegungen und Loeschungen teilen oder verbinden Pieces. Beim Speichern werden alle Pieces in eine temporaere Datei neben dem Ziel gestreamt; der ausgewaehlte BOM wird zuerst geschrieben. Windows ersetzt das Ziel erst nach erfolgreichem Flush. Das Dokument wird nie in einem grossen RAM-Puffer zusammengesetzt.

## Repository-Struktur

```text
BigPad/
|-- .github/workflows/   Windows-CI
|-- docs/                englische und deutsche Architektur
|-- src/                 C++-Quellcode
|-- CMakeLists.txt       Build-Konfiguration
|-- CONTRIBUTING*.md     zweisprachige Beitragsrichtlinien
|-- LICENSE              MIT-Lizenz
`-- README*.md           zweisprachige Projektuebersicht
```
