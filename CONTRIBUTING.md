# Contributing to BigPad

[Deutsch](CONTRIBUTING.de.md) | **English**

Bug reports and pull requests are welcome.

## Local build

BigPad requires Windows, CMake 3.20 or newer, and a C++20 compiler.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Before opening a pull request, ensure the Release build succeeds. Keep changes focused and preserve the core guarantee: displaying and editing a file must not require loading the complete file into memory.

## Pull requests

- Describe the problem, solution, and manual tests.
- Do not commit build artifacts or personal IDE files.
- New processing features should operate on visible file windows whenever possible.
- Do not silently change the file encoding or line endings when saving.
- Add both German and English UI strings for user-visible features.
