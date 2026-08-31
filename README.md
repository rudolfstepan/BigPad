# BigPad

[Deutsch](README.de.md) | **English**

[![Windows build](https://github.com/rudolfstepan/BigPad/actions/workflows/build.yml/badge.svg)](https://github.com/rudolfstepan/BigPad/actions/workflows/build.yml)

BigPad is an open-source native Windows text and hex editor designed for very large files. It does not load the complete file into memory. A piece table references unchanged ranges in the original file, while only small read windows and newly inserted bytes occupy RAM.

## Features

- Windowed access to multi-gigabyte files
- Streaming, atomic saves
- Text and editable hex modes
- UTF-8, optional UTF-8 BOM, and Windows ANSI
- German and English user interface
- Line numbers and word wrap
- Light and dark themes
- Configurable monospaced fonts and sizes
- Syntax highlighting for C/C++, C#, Assembly, JSON, Python, XML/HTML, and logs
- Drag and drop from Windows Explorer
- Proportional scrollbar and informative status bar
- Persistent per-user settings

## Build

From a Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

MinGW and Ninja are supported as well:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Keyboard and menus

- `Ctrl+N`: new document
- `Ctrl+O`: open file
- `Ctrl+S`: save
- `Ctrl+Shift+S`: save as
- `Mode`: switch between text and hex editor
- `Encoding`: select UTF-8 or Windows ANSI and control the UTF-8 BOM
- `Language`: switch between German and English

UTF-8 BOMs are detected automatically, hidden from the editing area, preserved when saving, and can be enabled or disabled from the Encoding menu.

## Documentation, contributing, and license

See [Architecture](docs/ARCHITECTURE.md) and [Contributing](CONTRIBUTING.md). BigPad is released under the [MIT License](LICENSE).
