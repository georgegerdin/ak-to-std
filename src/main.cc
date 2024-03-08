#include <iostream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include "cpp_parser/parser.hh"
#include "cpp_parser/traverse_ast.hh"
#include "local_filedb.h"

std::vector<std::string> load_contents_into_vector(std::ifstream& stream) {
    std::vector<std::string> input_file_contents;
    std::string line;

    // read the file line by line
    while (std::getline(stream, line)) {
        // add the line to the vector
        input_file_contents.push_back(line);
    }

    return input_file_contents;
}

bool contains(std::string& str, const char* text) {
    std::size_t pos = str.find(text);
    if(pos == std::string::npos)
        return false;
    return true;
}

void replace(std::string& str, const char* text_to_replace, const char* replacement) {
    auto len = strlen(text_to_replace);
    std::size_t pos = str.find(text_to_replace);
    while(pos != std::string::npos) {
        str.replace(pos, len, replacement);

        //Check if there is more text to replace
        pos = str.find(text_to_replace);
    }
}

std::string to_lower_case(std::string input) {
    std::transform (input.begin (), input.end (), input.begin (), [] (unsigned char c) { return std::tolower (c); });
    return input;
}

std::vector<std::string> convert(std::vector<std::string> content, std::string content_as_string, std::string include_path) {
    std::vector<std::string> converted;

    int line_num = 0;
    std::optional<int> first_include;
    bool include_vector = false
            , include_cassert = false
            , include_intrusive_ptr = false
            , include_util = false
            , include_optional = false
            , include_string_view = false
            , include_string = false;

    for(auto line : content) {
        line_num++;
        if(line.starts_with("#include <AK")) {
            if(!first_include) first_include = line_num - 1;
            continue;
        }
        if(line.starts_with("#include <LibCpp/")) {
            if(!first_include) first_include = line_num - 1;

            auto beginning = line.begin() + strlen("#include <LibCpp/");
            auto end_pos = line.find(">");
            if(end_pos == std::string::npos) {
                outln("Couldn't find '>' character in #include line");
                continue;
            }
            auto end = line.begin() + end_pos;

            std::string filename{beginning, end};
            converted.push_back(fmt::format("#include \"{}h\"", to_lower_case(filename)));
            continue;
        }
        if(line.starts_with("#include \"")) {
            if (!first_include) first_include = line_num - 1;

            auto beginning = line.begin() + strlen("#include \"");
            auto end_pos = line.find_last_of("\"");
            if(end_pos == std::string::npos) {
                outln("Couldn't find '\"' character in #include line");
                continue;
            }
            auto end = line.begin() + end_pos;


            std::string filename{beginning, end};
            converted.push_back(fmt::format("#include \"{}{}h\"", include_path, to_lower_case(filename)));
            continue;
        }

            if(contains(line, "Vector")) {
            replace(line, "Vector", "std::vector");
            include_vector = true;
        }
        if(contains(line, "StringView")) {
            replace(line, "StringView", "std::string_view");
            include_string_view = true;
        }
        if(contains(line, "ByteString::join")) {
            replace(line, "ByteString::join", "join_strings");
            include_string = true;
        }
        if(contains(line, "ByteString::formatted")) {
            replace(line, "ByteString::formatted", "fmt::format");
            include_string = true;
        }
        if(contains(line, "ByteString")) {
            replace(line, "ByteString", "std::string");
            include_string = true;
        }
        if(contains(line, "DeprecatedFlyString")) {
            replace(line, "DeprecatedFlyString", "std::string");
            include_string = true;
        }
        if(contains(line, "String ")) {
            replace(line, "String ", "std::string ");
            include_string = true;
        }
        if(contains(line, "String(")) {
            replace(line, "String( )", "std::string(");
            include_string = true;
        }
        if(contains(line, "VERIFY(")) {
            replace(line, "VERIFY(", "assert(");
            include_cassert = true;
        }
        if(contains(line, "RefCounted")) {
            replace(line, "RefCounted", "intrusive_ref_counter");
            include_intrusive_ptr = true;
        }
        if(contains(line, "NonnullRefPtr")) {
            replace(line, "NonnullRefPtr", "intrusive_ptr");
            include_intrusive_ptr = true;
        }
        if(contains(line, "RefPtr")) {
            replace(line, "RefPtr", "intrusive_ptr");
            include_intrusive_ptr = true;
        }
        if(contains(line, "Optional")) {
            replace(line, "Optional", "std::optional");
            include_optional = true;
        }
        if(contains(line, "\"sv;")) {
            replace(line, "\"sv;", "\";");
        }
        if(contains(line, " move(")) {
            replace(line, " move(", " std::move(");
        }
        if(contains(line, "(move(")) {
            replace(line, "(move(", "(std::move(");
        }
        if(contains(line, "append(")) {
            replace(line, "append(", "push_back(");
        }
        if(contains(line, "ptr()")) {
            replace(line, "ptr()", "get()");
        }
        if(contains(line, "to_byte_string()")) {
            auto position = line.find("to_byte_string");
            if(line.at(position-1) == '.') {

            }
            replace(line, "to_byte_string()", "to_string()");
        }
        if(contains(line, "is_empty()")) {
            replace(line, "is_empty()", "empty()");
        }
        if(contains(line, "verify_cast")) {
            replace(line, "verify_cast", "assert_cast");
            include_util = true;
        }

        converted.push_back(line);
    }

    if(!first_include) {
        outln("finding #pragma once");
        auto pragma_once = std::find(converted.begin(), converted.end(), "#pragma once");
        if(pragma_once == converted.end()) {
            outln("no pragma once");
            return converted;
        }

        first_include = std::distance(converted.begin(), pragma_once);
    }

    if(include_intrusive_ptr) {
        converted.insert(converted.begin() + first_include.value(), fmt::format("#include \"{}intrusive_ptr.hh\"", include_path));
    }
    if(include_util) {
        converted.insert(converted.begin() + first_include.value(), fmt::format("#include \"{}util.hh\"", include_path));
    }
    if(include_vector) {
        converted.insert(converted.begin() + first_include.value(), "#include <vector>");
    }
    if(include_string) {
        converted.insert(converted.begin() + first_include.value(), "#include <string>");
    }
    if(include_string_view) {
        converted.insert(converted.begin() + first_include.value(), "#include <string_view>");
    }
    if(include_optional) {
        converted.insert(converted.begin() + first_include.value(), "#include <optional>");
    }
    if(include_cassert) {
        converted.insert(converted.begin() + first_include.value(), "#include <cassert>");
    }

    return converted;
}

std::string read_first_line(const char* filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error(fmt::format("Error opening file: {}", filePath));
    }

    std::string line;
    std::getline(file, line);

    return line;
}

int main(int argc, char* argv[]) {
    std::vector<std::string> arguments;
    for(std::size_t i = 0; i < argc; ++i) {
        arguments.push_back(argv[i]);
    }

    if(arguments.size() < 3) {
        outln("Usage: ast_to_std <dst-file> <src-file1> [src-file2] ...");
        return -1;
    }

    auto input_file_path = arguments[2];
    auto output_file_path = arguments[1];

    outln("arguments: {}\ninput: {}\noutput: {}", arguments.size(), input_file_path, output_file_path);

    TESTS_ROOT_DIR = read_first_line("project_source_dir.txt") + "/test";

    std::ifstream input_file(input_file_path);
    if(!input_file.is_open()) {
        outln("Unable to open {}", input_file_path);
        return -1;
    }

    auto contents = load_contents_into_vector(input_file);
    input_file.close();
    outln("{}", contents.size());

    std::string content_as_string;
    for(auto const& line : contents) {
        content_as_string+=line + "\n";
    }

    auto output_content = convert(contents, content_as_string, "cpp_parser/");

    std::ofstream output(output_file_path);
    if(output.is_open()) {
        for(auto const& line : output_content) {
            output << line << std::endl;
        }
    }

    return 0;
}