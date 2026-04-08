#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fem {

struct DialogFilter {
    std::string description = "Files";
    std::vector<std::string> extensions;
};

std::optional<std::filesystem::path> open_file_dialog(const DialogFilter& filter);
std::vector<std::filesystem::path>   open_files_dialog(const DialogFilter& filter);
std::optional<std::filesystem::path> save_file_dialog(const DialogFilter& filter);
std::optional<std::filesystem::path> open_directory_dialog(const std::string& title);


}
