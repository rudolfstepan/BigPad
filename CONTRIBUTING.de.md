# Zu BigPad beitragen

**Deutsch** | [English](CONTRIBUTING.md)

Fehlerberichte und Pull Requests sind willkommen.

## Lokaler Build

BigPad benoetigt Windows, CMake 3.20 oder neuer und einen C++20-Compiler.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Vor einem Pull Request muss der Release-Build erfolgreich sein. Aenderungen sollen fokussiert bleiben und das Kernversprechen bewahren: Anzeige und Bearbeitung duerfen nicht das Laden der gesamten Datei in den Arbeitsspeicher erfordern.

## Pull Requests

- Problem, Loesung und manuelle Tests beschreiben.
- Keine Build-Artefakte oder persoenlichen IDE-Dateien committen.
- Neue Verarbeitungsfunktionen sollen moeglichst nur auf sichtbaren Dateifenstern arbeiten.
- Encoding und Zeilenenden beim Speichern nicht unbemerkt aendern.
- Fuer sichtbare Funktionen deutsche und englische UI-Texte ergaenzen.
