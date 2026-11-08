#include "../src/main.cpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

void require(bool value, const char* text) {
    if (!value) {
        throw std::runtime_error(text);
    }
}

int main() {
    const auto root =
        fs::temp_directory_path() / (L"bigpad-tests-" + std::to_wstring(GetCurrentProcessId()));
    try {
        fs::create_directory(root);
        const auto source = root / L"Quelle-ü.txt";
        const auto destination = root / L"Ziel-ü.txt";
        {
            std::ofstream file(source, std::ios::binary);
            file << "\xEF\xBB\xBFhello\r\nworld";
        }
        Document document;
        std::wstring error;
        require(document.open(source.wstring(), error), "Open Unicode filename");
        require(document.writeBom() && document.size() == 12,
                "Preserve UTF-8 BOM and original line endings");
        document.insert(5, "!");
        require(document.read(0, 6) == "hello!", "Insert at piece boundary");
        document.erase(5, 1);
        document.insert(7, "new ");
        require(document.saveAs(destination.wstring(), error), "Save edited document");
        std::ifstream saved(destination, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(saved)),
                                std::istreambuf_iterator<char>());
        require(bytes == "\xEF\xBB\xBFhello\r\nnew world",
                "Atomic save preserves BOM, CRLF and content");
        require(!document.dirty(), "Successful save clears modified state");
        document.close();
        saved.close();
        fs::remove_all(root);
        std::cout << "PASS BigPad document Unicode, BOM, CRLF, edits and save\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::error_code cleanup;
        fs::remove_all(root, cleanup);
        return 1;
    }
}
