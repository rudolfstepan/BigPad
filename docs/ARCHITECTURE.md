# Architektur

## Virtuelles Dokument

`Document` verwaltet eine Piece-Table. Ein Piece verweist entweder auf einen
unveraenderten Bytebereich der Originaldatei oder auf neu eingegebene Bytes.
Die Originaldatei bleibt geoeffnet und wird ueber einen kleinen 1-MiB-Cache
fensterweise gelesen. Die Dateigroesse ist deshalb nicht die bestimmende
Groesse fuer den RAM-Verbrauch.

## Anzeige

Der native Win32-Renderer materialisiert nur die aktuell sichtbaren Zeilen.
Scrollbar, Textansicht, Hexansicht und Syntax-Highlighting arbeiten auf diesem
Ausschnitt. Zeilennummern werden bei Spruengen bedarfsgesteuert bestimmt und
anschliessend zwischengespeichert.

## Bearbeitung und Speichern

Einfuegungen und Loeschungen teilen oder verbinden Pieces. Beim Speichern
werden die Pieces sequenziell in eine temporaere Datei neben dem Ziel
geschrieben. Erst nach erfolgreichem Schreiben und Flush ersetzt Windows die
Zieldatei. Das Gesamtdokument wird dabei nie zu einem grossen RAM-Puffer
zusammengebaut.

## Verzeichnisstruktur

```text
BigPad/
|-- .github/workflows/   CI-Build
|-- docs/                technische Dokumentation
|-- src/                 C++-Quellcode
|-- CMakeLists.txt       Build-Konfiguration
|-- CONTRIBUTING.md      Beitragsrichtlinien
|-- LICENSE              MIT-Lizenz
`-- README.md            Projektuebersicht und Bedienung
```

