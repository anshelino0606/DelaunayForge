#include "file_dialog.h"

#if defined(__linux__)

#include <array>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

namespace fem {

static std::string shell_escape_single_quotes(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('\'');
    for (char c : s) {
        if (c == '\'') out += "'\"'\"'";
        else out.push_back(c);
    }
    out.push_back('\'');
    return out;
}

static std::string exec_read_all(const std::string& cmd) {
    std::array<char, 256> buf{};
    std::string result;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};

    while (fgets(buf.data(), (int)buf.size(), pipe)) {
        result += buf.data();
    }
    pclose(pipe);

    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

static bool has_tool(const char* name) {
    std::string cmd = "command -v ";
    cmd += name;
    cmd += " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

static std::string zenity_filter(const DialogFilter& filter) {
    if (filter.extensions.empty()) return " --file-filter='All files|*'";

    std::ostringstream exts;
    for (size_t i = 0; i < filter.extensions.size(); ++i) {
        if (i) exts << " ";
        exts << "*." << filter.extensions[i];
    }

    std::ostringstream out;
    out << " --file-filter="
        << shell_escape_single_quotes(filter.description + " (" + exts.str() + ")|" + exts.str())
        << " --file-filter='All files|*'";
    return out.str();
}

static std::string kdialog_filter(const DialogFilter& filter) {
    if (filter.extensions.empty()) return "*";

    std::ostringstream exts;
    for (size_t i = 0; i < filter.extensions.size(); ++i) {
        if (i) exts << " ";
        exts << "*." << filter.extensions[i];
    }
    return exts.str();
}

std::optional<std::filesystem::path> open_file_dialog(const DialogFilter& filter) {
    if (has_tool("zenity")) {
        std::string cmd = "zenity --file-selection --title=" +
                          shell_escape_single_quotes("Open File") + zenity_filter(filter);
        auto r = exec_read_all(cmd);
        if (r.empty()) return std::nullopt;
        return std::filesystem::path(r);
    }
    if (has_tool("kdialog")) {
        std::string cmd = "kdialog --getopenfilename . " +
                          shell_escape_single_quotes(kdialog_filter(filter)) +
                          " --title " + shell_escape_single_quotes("Open File");
        auto r = exec_read_all(cmd);
        if (r.empty()) return std::nullopt;
        return std::filesystem::path(r);
    }
    return std::nullopt;
}

std::vector<std::filesystem::path> open_files_dialog(const DialogFilter& filter) {
    std::vector<std::filesystem::path> out;

    if (has_tool("zenity")) {
        std::string cmd = "zenity --file-selection --multiple --separator='|' --title=" +
                          shell_escape_single_quotes("Open Files") + zenity_filter(filter);
        auto r = exec_read_all(cmd);
        if (r.empty()) return out;

        std::istringstream ss(r);
        std::string item;
        while (std::getline(ss, item, '|')) {
            if (!item.empty()) out.emplace_back(item);
        }
        return out;
    }

    if (has_tool("kdialog")) {
        std::string cmd = "kdialog --getopenfilename . " +
                          shell_escape_single_quotes(kdialog_filter(filter)) +
                          " --multiple --separate-output --title " +
                          shell_escape_single_quotes("Open Files");
        auto r = exec_read_all(cmd);
        if (r.empty()) return out;

        std::istringstream ss(r);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty()) out.emplace_back(line);
        }
        return out;
    }

    return out;
}

std::optional<std::filesystem::path> save_file_dialog(const DialogFilter& filter) {
    if (has_tool("zenity")) {
        std::string cmd = "zenity --file-selection --save --confirm-overwrite --title=" +
                          shell_escape_single_quotes("Save File") + zenity_filter(filter);
        auto r = exec_read_all(cmd);
        if (r.empty()) return std::nullopt;
        return std::filesystem::path(r);
    }
    if (has_tool("kdialog")) {
        std::string cmd = "kdialog --getsavefilename . " +
                          shell_escape_single_quotes(kdialog_filter(filter)) +
                          " --title " + shell_escape_single_quotes("Save File");
        auto r = exec_read_all(cmd);
        if (r.empty()) return std::nullopt;
        return std::filesystem::path(r);
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> open_directory_dialog() {
    if (has_tool("zenity")) {
        auto r = exec_read_all("zenity --file-selection --directory --title='Select Folder'");
        if (r.empty()) return std::nullopt;
        return std::filesystem::path(r);
    }
    if (has_tool("kdialog")) {
        auto r = exec_read_all("kdialog --getexistingdirectory . --title 'Select Folder'");
        if (r.empty()) return std::nullopt;
        return std::filesystem::path(r);
    }
    return std::nullopt;
}

} // namespace fem

#endif // __linux__
