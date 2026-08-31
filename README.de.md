# BigPad

**Deutsch** | [English](README.md)

[![Windows-Build](https://github.com/rudolfstepan/BigPad/actions/workflows/build.yml/badge.svg)](https://github.com/rudolfstepan/BigPad/actions/workflows/build.yml)

BigPad ist ein nativer Open-Source-Text- und Hex-Editor fuer Windows, der fuer sehr grosse Dateien ausgelegt ist. Die Datei wird nicht vollstaendig in den Arbeitsspeicher geladen. Eine Piece-Table verweist auf unveraenderte Bereiche der Originaldatei; nur kleine Lesefenster und neu eingefuegte Bytes belegen RAM.

## Funktionen

- Fensterweiser Zugriff auf Dateien mit mehreren Gigabyte
- Streamendes, atomares Speichern
- Textmodus und editierbarer Hex-Modus
- UTF-8, optionales UTF-8-BOM und Windows-ANSI
- Deutsche und englische Benutzeroberflaeche
- Zeilennummern und virtueller Zeilenumbruch
- Helles und dunkles Farbschema
- Konfigurierbare Monospace-Schriften und Schriftgroessen
- Syntax-Highlighting fuer C/C++, C#, Assembler, JSON, Python, XML/HTML und Logs
- Drag-and-drop aus dem Windows Explorer
- Proportionale Scrollbar und informative Statusleiste
- Persistente Benutzereinstellungen

## Build

In einer Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Alternativ mit MinGW und Ninja:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Tastatur und Menues

- `Strg+N`: neues Dokument
- `Strg+O`: Datei oeffnen
- `Strg+S`: speichern
- `Strg+Umschalt+S`: speichern unter
- `Modus`: Text- oder Hex-Editor
- `Encoding`: UTF-8 oder Windows-ANSI sowie UTF-8-BOM auswaehlen
- `Sprache`: Deutsch oder English

UTF-8-BOMs werden automatisch erkannt, im Text nicht angezeigt, beim Speichern erhalten und koennen im Encoding-Menue ein- oder ausgeschaltet werden.

## Dokumentation, Beitraege und Lizenz

Siehe [Architektur](docs/ARCHITECTURE.de.md) und [Beitragsrichtlinien](CONTRIBUTING.de.md). BigPad steht unter der [MIT-Lizenz](LICENSE).
