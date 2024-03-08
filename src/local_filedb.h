#pragma once
#include <filesystem>
#include "filedb.hh"

class LocalFileDB : public CodeComprehension::FileDB {
public:
    LocalFileDB() = default;

    void add(std::string filename, std::string content)
    {
        m_map.emplace(filename, content);
    }

    virtual std::optional<std::string> get_or_read_from_filesystem(std::string_view filename) const override
    {
        std::string target_filename = std::string{filename};
        if (project_root().has_value() && filename.starts_with(*project_root())) {
            target_filename = std::filesystem::relative(filename, *project_root());
            dbgln("relative path: {}", target_filename.c_str());
        }

        auto result = m_map.find(target_filename);
        if(result == m_map.end())
            return std::nullopt;

        return result->second;
    }

private:
    std::unordered_map<std::string, std::string> m_map;
};

std::string TESTS_ROOT_DIR = "";

static void add_file(LocalFileDB& filedb, std::string const& name)
{
    std::filesystem::path root_dir_path{TESTS_ROOT_DIR};
    std::filesystem::path name_path{name};
    auto final_path = root_dir_path / name_path;
    std::ifstream file(final_path);
    if(!file)  {
        dbgln("Unable to load {}", final_path.c_str());
        throw std::runtime_error("unable to load file");
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    auto file_content = buffer.str();
    filedb.add(name, file_content);
}