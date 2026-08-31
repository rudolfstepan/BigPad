# BigPad

[![Windows build](https://github.com/rudolfstepan/BigPad/actions/workflows/build.yml/badge.svg)](https://github.com/rudolfstepan/BigPad/actions/workflows/build.yml)

Open-Source-Windows-Editor fuer sehr grosse Text- und Binaerdateien.

BigPad ist ein schlanker nativer Windows-Texteditor fuer sehr grosse UTF-8- und
ASCII-Dateien. Die geoeffnete Datei wird nicht komplett eingelesen. Eine
Piece-Table verweist auf Bereiche der Originaldatei; nur kleine Lesefenster und
die seit dem Oeffnen eingefuegten Zeichen liegen im Arbeitsspeicher.

## Bauen

Mit Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Alternativ mit MinGW/Ninja:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Bedienung

- `Strg+N`: neues Dokument, `Strg+O`: Datei oeffnen
- Eine Datei aus dem Explorer auf das Editorfenster ziehen: direkt oeffnen
- `Strg+S`: speichern, `Strg+Umschalt+S`: speichern unter
- Pfeile, Bild auf/ab, Pos1/Ende, Mausrad und Scrollbar zur Navigation
- Eingabe, Ruecktaste und Entf zum Bearbeiten
- `Ansicht > Zeilennummern`: Nummernspalte ein- oder ausblenden
- `Ansicht > Virtueller Zeilenumbruch`: lange Zeilen an der Fensterbreite umbrechen
- `Ansicht > Heller/Dunkler Modus`: Farbschema des Editors umschalten
- `Schrift`: Schriftart und Schriftgroesse auswaehlen
- `Hilfe > Ueber BigPad`: Programminformationen anzeigen
- `Syntax`: schnelles Highlighting automatisch oder manuell fuer C/C++, C#, Assembler, JSON, Python, XML/HTML und Logs
- Proportionale Scrollbar fuer Position und sichtbaren Ausschnitt der gesamten Datei
- Statusleiste mit Zeile/Spalte, Byteposition, Dateigroesse, UTF-8, Syntaxmodus und Speicherstatus
- Umschaltbarer Hex-Editor mit 64-Bit-Offsets, Hex-/ASCII-Ansicht und nibbleweiser Bearbeitung
- Persistente Benutzereinstellungen unter `HKEY_CURRENT_USER\\Software\\BigPad`

Beim Speichern werden alle Pieces sequenziell in eine temporaere Datei neben
dem Ziel geschrieben. Erst danach ersetzt BigPad atomar die Zieldatei. Der RAM-
Bedarf ist damit von der Groesse der Originaldatei weitgehend unabhaengig; er
waechst im Wesentlichen mit den neuen Einfuegungen und der Zahl der Aenderungen.

Hinweis: Der Renderer interpretiert Text als UTF-8. Zeilenumbrueche bleiben in
ihren vorhandenen Bytes erhalten; neu erzeugte Zeilen verwenden CRLF.

## Mitwirken und Lizenz

Technische Hintergruende stehen in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md),
Hinweise fuer Beitraege in [CONTRIBUTING.md](CONTRIBUTING.md). BigPad steht
unter der [MIT-Lizenz](LICENSE).
