# Contributing to BigPad

Danke fuer dein Interesse an BigPad. Fehlerberichte und Pull Requests sind
willkommen.

## Lokaler Build

Voraussetzungen sind Windows, CMake 3.20 oder neuer und ein C++20-Compiler.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Vor einem Pull Request muss der Release-Build ohne Fehler durchlaufen. Bitte
halte Aenderungen klein und bewahre das zentrale Versprechen des Projekts:
Dateien duerfen fuer Anzeige und Bearbeitung nicht vollstaendig in den
Arbeitsspeicher geladen werden.

## Pull Requests

- Beschreibe Problem, Loesung und manuelle Tests.
- Fuege keine Build-Artefakte oder persoenlichen IDE-Dateien hinzu.
- Nutze fuer neue Funktionen nach Moeglichkeit nur die sichtbaren Dateifenster.
- Aendere das Dateiformat beim Speichern nicht unbemerkt.

