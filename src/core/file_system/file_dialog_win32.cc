#include "file_dialog.h"

#if defined(_WIN32)

#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <sstream>

namespace fem {

static std::string build_filter_string(const DialogFilter& filter) {
    const auto& extensions = filter.extensions;
    const auto& description = filter.description;

    if (extensions.empty()) {
        std::string s = "All Files";
        s.push_back('\0');
        s += "*.*";
        s.push_back('\0');
        s.push_back('\0');
        return s;
    }

    std::ostringstream desc_stream;
    std::ostringstream ext_stream;

    desc_stream << description << " (";
    bool first = true;
    for (const auto& ext : extensions) {
        if (!first) desc_stream << ", ";
        desc_stream << "*." << ext;
        first = false;
    }
    desc_stream << ")";

    first = true;
    for (const auto& ext : extensions) {
        if (!first) ext_stream << ";";
        ext_stream << "*." << ext;
        first = false;
    }

    std::string s;
    s += desc_stream.str(); s.push_back('\0');
    s += ext_stream.str();  s.push_back('\0');
    s += "All Files";       s.push_back('\0');
    s += "*.*";             s.push_back('\0');
    s.push_back('\0');
    return s;
}

std::optional<std::filesystem::path> open_file_dialog(const DialogFilter& filter) {
    const std::string filter_str = build_filter_string(filter);

    char fileName[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.lpstrFilter  = filter_str.c_str();
    ofn.lpstrFile    = fileName;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle   = "Open File";

    if (GetOpenFileNameA(&ofn)) return std::filesystem::path(fileName);
    return std::nullopt;
}

std::vector<std::filesystem::path> open_files_dialog(const DialogFilter& filter) {
    const std::string filter_str = build_filter_string(filter);

    char buffer[4096] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.lpstrFilter  = filter_str.c_str();
    ofn.lpstrFile    = buffer;
    ofn.nMaxFile     = sizeof(buffer);
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    ofn.lpstrTitle   = "Open Files";

    std::vector<std::filesystem::path> out;

    if (!GetOpenFileNameA(&ofn)) return out;

    char* ptr = buffer;
    std::string directory = ptr;
    ptr += directory.size() + 1;

    if (*ptr == '\0') {
        out.emplace_back(directory);
        return out;
    }

    while (*ptr) {
        out.emplace_back(directory + "\\" + ptr);
        ptr += std::strlen(ptr) + 1;
    }
    return out;
}

std::optional<std::filesystem::path> save_file_dialog(const DialogFilter& filter) {
    const std::string filter_str = build_filter_string(filter);

    char fileName[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = filter_str.c_str();
    ofn.lpstrFile   = fileName;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle  = "Save File";

    if (GetSaveFileNameA(&ofn)) return std::filesystem::path(fileName);
    return std::nullopt;
}

std::optional<std::filesystem::path> open_directory_dialog(const std::string& title) {
    BROWSEINFOA bi = {};
    bi.lpszTitle = title.c_str();
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST item = SHBrowseForFolderA(&bi);
    if (!item) return std::nullopt;

    char folderPath[MAX_PATH] = {};
    const BOOL ok = SHGetPathFromIDListA(item, folderPath);
    CoTaskMemFree(item);

    if (!ok) return std::nullopt;
    return std::filesystem::path(folderPath);
}

} // namespace fem::ui

#endif // _WIN32
