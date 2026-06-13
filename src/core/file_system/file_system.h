#ifndef FEM_FILE_SYSTEM_H
#define FEM_FILE_SYSTEM_H

#include <vector>
#include <string>
#include <functional>
#include <unordered_set>
#include <filesystem>

namespace fem {

using DirectoryEntry = std::filesystem::directory_entry;

enum class DirectoryIteratorType : uint8_t {
    DEFAULT,
    RECURSIVE
};

class FileStream {
public:
    FileStream() = default;
    FileStream(FILE* file);
    ~FileStream();

    void close();
    uint64_t read(void* data, uint64_t size, uint64_t count);
    uint64_t write(const void* data, uint64_t size, uint64_t count);
    uint64_t size() const;

    bool is_valid() { return file_; }

private:
    FILE* file_ = nullptr;
};

class FileSystem {
public:
    using ForEachCallback = std::function<void(const DirectoryEntry&)>; 

    static void init();

    // Creates directory using random name if projectPath is empty. The passed project path must be a relative path to the engine root.
    static void set_project_path(const std::string& project_path);
    static std::string get_project_path() { return s_project_path_; }
    static std::string get_project_name();
    static bool is_project_existed(const std::string& project_path);

    static std::string_view get_source_path() { return SOURCE_DIR; }

    static FileStream* open(const std::string& str_path, const char* mode);
    static bool close(FileStream* stream);

    static void read(const std::string& path, uint8_t** out_data, uint64_t* outSize);
    static void read(const std::string& path, std::vector<uint8_t>& out_data);
    static void read(const std::string& path, std::string& out_data);
    static void read(const std::string& path, uint64_t size, uint8_t* out_data);
    static void read(const std::string& path, uint64_t size, std::vector<uint8_t>& out_data);

    static void write(const std::string& path, const uint8_t* data, uint64_t size);
    static void write(const std::string& path, const std::vector<uint8_t>& data);
    static void write(const std::string& path, const std::string& data);

    static std::string get_file_name(const std::string& path);
    static std::string get_file_extension(const std::string& path);
    
    static std::string get_relative_path(const std::string& root_path, const std::string& target_path);
    static std::string get_absolute_path(const std::string& root_path, const std::string& relative_path);

    static std::string absolute_path_from_source(const std::string& relative_path);

    static bool is_relative(const std::string& path);
    static bool is_absolute(const std::string& path);
    static bool has_extension(const std::string& path);
    static bool exists(const std::string& absolute_path);
    static bool exists(const std::string& root_path, const std::string& relative_path);

    static uint64_t get_last_write_time(const std::string& absolute_path);

    static void for_each_file(
        const std::string& path, 
        const std::unordered_set<std::string>& extensions,
        const ForEachCallback& callback,
        DirectoryIteratorType iterator_type = DirectoryIteratorType::DEFAULT
    );

    static std::string open_file_dialog(const std::string& window_name, const std::vector<std::string>& extensions = {}, const std::string& base_path = {});
    static void open_files_dialog(const std::string& window_name, const std::vector<std::string>& extensions, std::vector<std::string>& out_files, const std::string& base_path = {});
    static std::string save_file_dialog(const std::string& window_name, const std::vector<std::string>& extensions = {}, const std::string& base_path = {});
    static std::string open_directory_dialog(const std::string& window_name, const std::string& base_directory = "");
    static std::vector<std::string> build_extension_filter();

    // If newName is not an absolute path, only filename will be changed
    static std::string rename_file(const std::string& old_absolute_path, const std::string& new_name);
    static void create_directories(std::string path);

private:
    inline static std::string s_project_path_ = "";

    static void read_internal(FileStream* stream, const std::string& path, uint8_t* data, uint64_t size);
    static FileStream* create_read_stream(const std::string& path, uint64_t& out_size);

    static std::string build_filter_string(const std::vector<std::string>& extensions, const std::string& description = "Files");
};

}

#endif // FEM_FILE_SYSTEM_H