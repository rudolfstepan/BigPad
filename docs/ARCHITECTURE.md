# Architecture

[Deutsch](ARCHITECTURE.de.md) | **English**

## Virtual document

`Document` uses a piece table. A piece references either an unchanged byte range in the original file or newly inserted bytes. The original remains open and is read through a small 1 MiB cache. File size therefore does not determine RAM consumption.

## Display

The native Win32 renderer materializes only visible lines or hex rows. Scrolling, syntax highlighting, line numbers, and status information operate on this window. UTF-8 BOM bytes are detected at open time and excluded from the logical document.

## Encodings

The text view supports UTF-8 and the active Windows ANSI code page. UTF-8 BOM output is independently selectable and preserved after automatic detection. The hex view always exposes logical content bytes without the UTF-8 BOM.

## Editing and saving

Insertions and deletions split or merge pieces. Saving streams every piece into a temporary file beside the target and writes the selected BOM first. Windows replaces the target only after a successful flush; the document is never assembled into a large RAM buffer.

## Repository layout

```text
BigPad/
|-- .github/workflows/   Windows CI
|-- docs/                English and German architecture docs
|-- assets/              application icon sources`n|-- src/                 C++ source
|-- resource.h/.rc       Windows application resources`n|-- CMakeLists.txt       build configuration
|-- CONTRIBUTING*.md     bilingual contribution guides
|-- LICENSE              MIT license
`-- README*.md           bilingual project overview
```
