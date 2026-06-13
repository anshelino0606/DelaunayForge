#include "file_system.h"
// #include "core/logger.h"
// #include "core/platform/platform.h"
#include <random>
#include <fstream>
#include <cassert>
#include <sstream>

#include "file_dialog.h"

#ifdef WIN32
    #include <windows.h>
    #include <commdlg.h>
    #include <shlobj.h>
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_MAC
        #include <CoreFoundation/CoreFoundation.h>
        #ifdef __OBJC__
            #include <AppKit/AppKit.h>
            #include <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
        #endif
    #endif
#elif defined(__linux__)
    #include <unistd.h>
    #include <limits.h>
    #include <cstdlib>
    #include <sys/wait.h>
    #include <array>
    #include <memory>
#endif

namespace fem {

FileStream::FileStream(FILE* file) : file_(file) {
    assert(file && "FileStream: file pointer cannot be null");
}

FileStream::~FileStream()
{
    close();
}

void FileStream::close() {
    if (file_)
        fclose(file_);

    file_ = nullptr;
}

uint64_t FileStream::read(void* data, uint64_t size, uint64_t count) {
    assert(file_ && "FileStream::read(): file is invalid");
    assert(data && "FileStream::read(): data pointer is null");
    return fread(data, size, count, file_);
}

uint64_t FileStream::write(const void* data, uint64_t size, uint64_t count) {
    assert(file_ && "FileStream::write(): file is invalid");
    assert(data && "FileStream::write(): data pointer is null");
    return fwrite(data, size, count, file_);
}

uint64_t FileStream::size() const {
    assert(file_ && "FileStream::size(): file is invalid.");
    long position = ftell(file_);
    fseek(file_, 0, SEEK_END);
    long size = ftell(file_);
    fseek(file_, position, SEEK_SET);
    return (uint64_t)size;
}

void FileSystem::init() {
    // Maybe will need this one?
}

void FileSystem::set_project_path(const std::string& project_path) {
    if (is_relative(project_path)) {
        std::printf("Project path can't be relative!\n");
        return;
    }

    s_project_path_ = project_path;
}

std::string FileSystem::get_project_name() {
    std::filesystem::path path(s_project_path_);
    return path.filename().string();
}

bool FileSystem::is_project_existed(const std::string& project_path) {
    return exists(project_path) && std::filesystem::is_directory(project_path);
}

FileStream* FileSystem::open(const std::string& str_path, const char* mode) {
    std::filesystem::path path = std::filesystem::path(str_path);

    if (!std::filesystem::exists(path) && strcmp(mode, "wb") != 0)
    {
        std::printf("Path %s does not exist!\n", str_path.c_str());
        return nullptr;
    }

    if (!path.has_extension())
    {
        std::printf("No extension in %s\n", str_path.c_str());
        return nullptr;
    }

    FILE* file = std::fopen(path.string().c_str(), mode);

    if (!file)
    {
        std::printf("Failed to opend file by path %s\n", str_path.c_str());
        return nullptr;
    }

    return new FileStream(file);
}

bool FileSystem::close(FileStream* stream) {
    if (!stream || !stream->is_valid())
    {
        if (stream)
            delete stream;

        return false;
    }
    stream->close();
    delete stream;

    return true;
}

void FileSystem::read(const std::string& path, uint8_t** out_data, uint64_t* out_size) {
    FileStream* stream = create_read_stream(path, *out_size);
    if (!stream)
        return;

    *out_data = new uint8_t[*out_size];

    read_internal(stream, path, *out_data, *out_size);
}

void FileSystem::read(const std::string& path, std::vector<uint8_t>& out_data) {
    uint64_t size;

    FileStream* stream = create_read_stream(path, size);
    if (!stream)
        return;

    out_data.resize(size);

    read_internal(stream, path, out_data.data(), out_data.size());
}

void FileSystem::read(const std::string& path, std::string& out_data) {
    uint64_t size;

    FileStream* stream = create_read_stream(path, size);
    if (!stream)
        return;

    out_data.resize(size);

    read_internal(stream, path, (uint8_t*)out_data.data(), out_data.size());
}

void FileSystem::read(const std::string& path, uint64_t size, uint8_t* out_data) {
    assert(out_data);
    FileStream* stream = open(path, "rb");

    size_t read_elements = stream->read(out_data, sizeof(uint8_t), size);
    assert(read_elements != 0);

    bool after_close = close(stream);
    assert(after_close && "Error while closing stream");
}

void FileSystem::read(const std::string& path, uint64_t size, std::vector<uint8_t>& out_data) {
    uint64_t offset = 0;

    if (size >= out_data.size())
    {
        offset = out_data.size();
        out_data.resize(out_data.size() + size);
    }
    
    FileStream* stream = open(path, "rb");
    size_t read_elements = stream->read(out_data.data() + offset, sizeof(uint8_t), size);
    assert(read_elements != 0);

    bool after_close = close(stream);
    assert(after_close && "Error while closing stream");
}

void FileSystem::write(const std::string& path, const uint8_t* data, uint64_t size) {
    assert(data);
    assert(size);

    FileStream* stream = open(path, "wb");
    stream->write(data, sizeof(uint8_t), size);
    close(stream);
}

void FileSystem::write(const std::string& path, const std::vector<uint8_t>& data) {
    assert(!data.empty());

    write(path, data.data(), data.size());
}

void FileSystem::write(const std::string& path, const std::string& data) {
    assert(!data.empty());

    write(path, (const uint8_t*)data.data(), data.size());
}

std::string FileSystem::get_file_name(const std::string& path) {
    std::string str_path = std::filesystem::path(path.c_str()).filename().string();
    if (str_path.find(".") != std::string::npos)
        str_path.erase(str_path.find_last_of("."), str_path.size());
    return str_path;
}

std::string FileSystem::get_file_extension(const std::string& path) {
    std::string extension = std::filesystem::path(path.c_str()).extension().string();
    extension.erase(0, 1);
    return extension;
}

std::string FileSystem::get_relative_path(const std::string& root_path, const std::string& target_path) {
    std::filesystem::path relative_path = std::filesystem::relative(target_path, root_path);
    return relative_path.string().c_str();
}

std::string FileSystem::get_absolute_path(const std::string& root_path, const std::string& relative_path) {
    std::filesystem::path base_path(root_path);
    std::filesystem::path target_path(relative_path);
    return (base_path / target_path).string();
}

std::string FileSystem::absolute_path_from_source(const std::string& relative_path) {
    return std::format("{}/{}", get_source_path(), relative_path);
}

bool FileSystem::is_relative(const std::string& path) {
    return std::filesystem::path(path).is_relative();
}

bool FileSystem::is_absolute(const std::string& path) {
    return std::filesystem::path(path).is_absolute();
}

bool FileSystem::has_extension(const std::string& path) {
    return std::filesystem::path(path).has_extension();
}

bool FileSystem::exists(const std::string& absolute_path) {
    return std::filesystem::exists(absolute_path);
}

bool FileSystem::exists(const std::string& root_path, const std::string& relative_path) {
    return std::filesystem::exists(get_absolute_path(root_path, relative_path));
}

void FileSystem::for_each_file(
    const std::string& path, 
    const std::unordered_set<std::string>& extensions,
    const ForEachCallback& callback,
    DirectoryIteratorType iterator_type
) {
    if (is_relative(path)) {
        std::printf("FileSystem::for_each_file(): Path %s is relative!\n", path.c_str());
        return;
    }

    if (!exists(path)) {
        std::printf("FileSystem::for_each_file(): Path %s does not exist.\n", path.c_str());
        return;
    }

    switch (iterator_type) {
    case DirectoryIteratorType::DEFAULT: {
        for (auto& dir_entry : std::filesystem::directory_iterator(path)) {
            std::string path = dir_entry.path().string();

            if (dir_entry.is_directory() || !extensions.contains(get_file_extension(path)))
                continue;
    
            callback(dir_entry);
        }

        break;
    }
    case DirectoryIteratorType::RECURSIVE: {
        for (auto& dir_entry : std::filesystem::recursive_directory_iterator(path)) {
            std::string path = dir_entry.path().string();

            if (dir_entry.is_directory() || !extensions.contains(get_file_extension(path)))
                continue;
    
            callback(dir_entry);
        }

        break;
    }
    }

}


std::string FileSystem::open_file_dialog(
    const std::string& window_name, 
    const std::vector<std::string>& extensions, 
    const std::string& base_path
) {
    DialogFilter filter;
    filter.description = window_name;
    filter.extensions = extensions;

    if (auto path = fem::open_file_dialog(filter)) {
        return path->string();
    }

    return {};
}


void FileSystem::open_files_dialog(
    const std::string& window_name, 
    const std::vector<std::string>& extensions, 
    std::vector<std::string>& out_files, 
    const std::string& base_path
) {
    DialogFilter filter;
    filter.description = window_name;
    filter.extensions = extensions;

    out_files.clear();

    std::vector<std::filesystem::path> paths = fem::open_files_dialog(filter);
    out_files.reserve(paths.size());

    for (const auto& path : paths) {
        out_files.push_back(path.string());
    }
}

std::string FileSystem::save_file_dialog(
    const std::string& window_name, 
    const std::vector<std::string>& extensions, 
    const std::string& base_path
) {
    DialogFilter f;
    f.description = window_name;
    f.extensions = extensions;

    if (auto p = fem::save_file_dialog(f)) { 
        return p->string();
    }

    return {};
}

std::string FileSystem::open_directory_dialog(const std::string& window_name, const std::string& base_directory) {
    if (auto p = fem::open_directory_dialog(window_name)) {
        return p->string();
    }
    return {};
}

void FileSystem::read_internal(FileStream* stream, const std::string& path, uint8_t* data, uint64_t size) {
    size_t read_elements = stream->read(data, sizeof(uint8_t), size);
    assert(read_elements == size && "Data is invalid");
    assert(data && "Data is invalid");

    bool afterClose = close(stream);
    assert(afterClose && "Error while closing stream");
}

FileStream* FileSystem::create_read_stream(const std::string& path, uint64_t& out_size) {
    FileStream* stream = open(path, "rb");

    out_size = stream->size();
    if (!out_size) {
        std::printf("File %s is empty\n", path.c_str());
        return nullptr;
    }

    return stream;
}

std::string FileSystem::rename_file(const std::string& old_absolute_path, const std::string& new_name) {
    if (is_absolute(new_name)) {
        if (exists(old_absolute_path))
            std::filesystem::rename(old_absolute_path, new_name);

        return new_name;
    }

    std::filesystem::path old_path(old_absolute_path);
    std::filesystem::path new_path = old_path.parent_path() / (new_name + old_path.extension().string());

    if (exists(old_absolute_path))
        std::filesystem::rename(old_path, new_path);

    return new_path.string();
}

void FileSystem::create_directories(std::string path) {
    if (is_relative(path)) {
        std::printf("FileSystem::create_directories(): Path must be absolute!\n");
    }

    if (exists(path))
        return;

    std::filesystem::create_directories(path);
}

uint64_t FileSystem::get_last_write_time(const std::string& absolute_path) {
    auto time = std::filesystem::last_write_time(absolute_path.c_str());
    return std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count();
}

std::string FileSystem::build_filter_string(const std::vector<std::string>& extensions, const std::string& description) {
    if (extensions.empty()) {
        // Default "All Files" filter string
        std::string filter_string = "All Files";
        filter_string.push_back('\0');
        filter_string += "*.*";
        filter_string.push_back('\0');
        filter_string.push_back('\0');
        return filter_string;
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

    std::string filter_string;
    filter_string += desc_stream.str();
    filter_string.push_back('\0');
    filter_string += ext_stream.str();
    filter_string.push_back('\0');
    filter_string += "All Files";
    filter_string.push_back('\0');
    filter_string += "*.*";
    filter_string.push_back('\0');
    filter_string.push_back('\0');

    return filter_string;
}

}