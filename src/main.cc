#include <iostream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <set>
#include <cpp/cppcomprehensionengine.hh>
#include <map>
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

std::optional<std::vector<CodeComprehension::TokenInfo>::size_type> find_token_index(
        int row
        , int column
        , std::vector<CodeComprehension::TokenInfo> const& vec_token_info)
{
    for(int i = 0; i < vec_token_info.size(); ++i) {
        auto const& token_info = vec_token_info[i];
        if(row >= token_info.start_line && row <= token_info.end_line) {
            if(row == token_info.start_line) {
                if(column < token_info.start_column)
                    continue;
            }
            if(row == token_info.end_line) {
                if(column > token_info.end_column)
                    continue;
            }
            return i;
        }
    }
    return std::nullopt;
}

std::string to_string(CodeComprehension::TokenInfo const& token_info) {
    std::string result="[row: ";
    result+= std::to_string(token_info.start_line);
    result+= ", col: ";
    result+= std::to_string(token_info.start_column);
    result+= "] -> [row: ";
    result+= std::to_string(token_info.end_line);
    result+= ", col: ";
    result+= std::to_string(token_info.end_column);
    result+= "]";
    return result;
}

using TokensInfoVec = std::vector<CodeComprehension::TokenInfo>;
class ConvertAkToStd {
protected:
    std::string m_include_path;
    LocalFileDB filedb;
    std::map<std::string, TokensInfoVec> tokens_info_map;
    std::map<std::string, std::string> m_content_as_string;
    std::map<std::string, std::vector<std::string>> content_as_lines;
    std::set<std::string> m_added_files;
public:

    void add_include_filepath_for_output(const char* include_path) {
        m_include_path = include_path;
    }

    void add_file(const char* file_path) {
        ::add_file(filedb, file_path);

        std::ifstream input_file(TESTS_ROOT_DIR + file_path);
        if(!input_file.is_open()) {
            outln("Unable to open {}", file_path);
            exit(0);
        }

        auto contents = load_contents_into_vector(input_file);
        input_file.close();
        outln("{}", contents.size());

        std::string content_as_string;
        for(auto const& line : contents) {
            content_as_string+=line + "\n";
        }

        content_as_lines[file_path] = contents;
        m_content_as_string[file_path] = content_as_string;
        m_added_files.insert(file_path);
    }

    std::string get_token_string(const char* filename, CodeComprehension::TokenInfo const &token_info) {
        auto const& content = content_as_lines[filename];
        std::string result;
        bool first_line = true;

        for (int i = token_info.start_line; i <= token_info.end_line; ++i) {
            if (!first_line)  // The first line should not start with a newline
                result += "\n";
            else
                first_line = false;

            int start_col = 0, end_col = content[i].size() - 1;
            if (i == token_info.start_line) {
                start_col = token_info.start_column;
            }
            if (i == token_info.end_line) {
                end_col = token_info.end_column;
            }

            result += content[i].substr(start_col, end_col - start_col + 1);
        }
        return result;
    }

    std::string token_string (const char* filename, int token_index, TokensInfoVec const &tiv) {
        return get_token_string(filename, tiv[token_index]);
    }

    std::optional<std::string>  object_text(const char* filename, int line, int position,
                                           TokensInfoVec const &tiv)
    {
        // This is the method call
        auto tok_index_opt = find_token_index(line, position, tiv);
        if (!tok_index_opt.has_value()) return std::nullopt;

        // This is the . or the -> character
        auto token_index = tok_index_opt.value();
        auto prev_token = get_token_string(filename, tiv[token_index - 1]);
        if (prev_token != "." && prev_token != "->") return std::nullopt;

        // This is the object text
        auto object_text = get_token_string(filename, tiv[token_index - 2]);
        return object_text;
    }

    std::unique_ptr<CodeComprehension::Cpp::CppComprehensionEngine> engine;

    std::optional<std::string> find_parent_token_type(const char* filename, int line, int position,
             TokensInfoVec const &tiv) {
        auto tok_index = find_token_index(line, position, tiv);
        if (tok_index) {
            auto token_index = tok_index.value();
            auto prev_token = get_token_string(filename, tiv[token_index - 1]);
            if (prev_token == "." || prev_token == "->") {
                auto const &prev_prev_token = tiv[token_index - 2];
                auto parent_token = engine->find_declaration_of(filename, {prev_prev_token.start_line,
                                                                          prev_prev_token.start_column});
                if (parent_token.has_value()) {
                    auto tok_index_opt = find_token_index(parent_token.value().line, parent_token.value().column,
                                                          tokens_info_map[parent_token.value().file]);
                    if (tok_index_opt.has_value()) {
                        auto tok_index = tok_index_opt.value();
                        auto const &tiv = tokens_info_map[parent_token.value().file];
                        auto s = get_token_string(parent_token.value().file.c_str(), tiv[tok_index]);
                        return s;
                    }
                }
            }
        } else {
            dbgln("find_token_index({}, {}): std::nullopt", line, position);
        }
        return std::nullopt;
    }

    bool token_is_left_paren(const char* filename, int token_index, TokensInfoVec const &tiv) {
        if (get_token_string(filename, tiv[token_index]) == "(")
            return true;
        return false;
    }


    bool token_is_right_paren (const char* filename, int token_index, TokensInfoVec const &tiv) {
        if (get_token_string(filename, tiv[token_index]) == ")")
            return true;
        return false;
    }

    std::optional<std::string> text_between_matching_parens(
            const char* filename
            , int line
            , int position
            , TokensInfoVec const &tiv)
    {
        auto tok_index_opt = find_token_index(line, position, tiv);
        if (!tok_index_opt) return std::nullopt;

        std::optional<std::string> result;
        auto tok_index = tok_index_opt.value() + 1;

        // Helper lambda to add current token to result string
        auto extend_result = [this, &result, &filename, &tiv, tok_index]() {
            if (result.has_value()) {
                result.value().append(get_whitespace_between_tokens(filename, tok_index - 1, tok_index, tiv));
                result.value().append(get_token_string(filename, tiv[tok_index]));
            } else {
                result = get_token_string(filename, tiv[tok_index]);
            }
        };

        int paren_depth = 0;
        do {
            if (token_is_left_paren(filename, tok_index, tiv)) {
                if (paren_depth > 0) extend_result();
                paren_depth++;
            } else if (token_is_right_paren(filename, tok_index, tiv)) {
                paren_depth--;
                if (paren_depth > 0) extend_result();
            } else
                extend_result();

            tok_index++;
        } while (paren_depth);

        return result;
    }

    std::string get_whitespace_between_tokens(const char* filename, int prev_token, int token, TokensInfoVec const &tiv) {
        auto prev_token_info = tiv[prev_token];
        auto token_info = tiv[token];
        CodeComprehension::TokenInfo info;
        info.start_line = prev_token_info.end_line;
        info.start_column = prev_token_info.end_column + 1;
        info.end_line = token_info.start_line;
        info.end_column = (token_info.start_column == 0) ? 0 : token_info.start_column - 1;

        return get_token_string(filename, info);
    }

    std::optional<int> position_of_last_matching_paren(const char* filename, int line, int position, TokensInfoVec const &tiv)
    {
        auto tok_index_opt = find_token_index(line, position, tiv);
        if (!tok_index_opt) return std::nullopt;

        auto tok_index = tok_index_opt.value() + 1;
        int paren_depth = 0;
        do {
            if (token_is_left_paren(filename, tok_index, tiv)) {
                paren_depth++;
            } else if (token_is_right_paren(filename, tok_index, tiv)) {
                paren_depth--;
            }
            tok_index++;
        } while (paren_depth && tok_index < tiv.size());

        if (token_is_right_paren(filename, tok_index - 1, tiv))
            return tiv[tok_index].start_column;
        return std::nullopt;
    };

    std::vector<std::string> convert(const char *filename) {
        std::vector<std::string> converted;
        engine = std::make_unique<CodeComprehension::Cpp::CppComprehensionEngine>(filedb);

        for(auto const file_iterator : m_added_files) {
            tokens_info_map[file_iterator] = engine->get_tokens_info(file_iterator);
        }

        int line_num = 0;
        std::optional<int> first_include;
        bool include_vector = false
        , include_cassert = false
        , include_intrusive_ptr = false
        , include_util = false
        , include_optional = false
        , include_string_view = false
        , include_string = false
        , add_todo_entry = false;

        std::set<std::string> debug_constants;



        for (auto line : content_as_lines[filename]) {
            line_num++;
            if (line.starts_with("#include <AK")) {
                if (!first_include) first_include = line_num - 1;
                continue;
            }
            if (line.starts_with("#include <LibCpp/")) {
                if (!first_include) first_include = line_num - 1;

                auto beginning = line.begin() + strlen("#include <LibCpp/");
                auto end_pos = line.find(">");
                if (end_pos == std::string::npos) {
                    outln("Couldn't find '>' character in #include line");
                    continue;
                }
                auto end = line.begin() + end_pos;

                std::string filename{beginning, end};
                converted.push_back(fmt::format("#include \"{}{}h\"", m_include_path, to_lower_case(filename)));
                continue;
            }
            if (line.starts_with("#include \"")) {
                if (!first_include) first_include = line_num - 1;

                auto beginning = line.begin() + strlen("#include \"");
                auto end_pos = line.find_last_of("\"");
                if (end_pos == std::string::npos) {
                    outln("Couldn't find '\"' character in #include line");
                    continue;
                }
                auto end = line.begin() + end_pos;


                std::string filename{beginning, end};
                dbgln("{}", filename);
                converted.push_back(fmt::format("#include \"{}{}h\"", m_include_path, to_lower_case(filename)));
                continue;
            }

            if (contains(line, "CPP_DEBUG")) debug_constants.insert("CPP_DEBUG");

            if (contains(line, "Vector")) {
                replace(line, "Vector", "std::vector");
                include_vector = true;
            }
            if (contains(line, "StringView")) {
                replace(line, "StringView", "std::string_view");
                include_string_view = true;
            }
            if (contains(line, "ByteString::empty()")) {
                replace(line, "ByteString::empty()", "\"\"");
            }
            if (contains(line, "ByteString::join")) {
                replace(line, "ByteString::join", "join_strings");
                include_string = true;
            }

            if (contains(line, "DeprecatedFlyString")) {
                replace(line, "DeprecatedFlyString", "std::string");
                include_string = true;
            }
            if (contains(line, "StringBuilder ")) {
                replace(line, "StringBuilder ", "std::string ");
                include_string = true;
            }
            if (contains(line, "String(")) {
                replace(line, "String( )", "std::string(");
                include_string = true;
            }
            if (contains(line, "VERIFY(")) {
                replace(line, "VERIFY(", "assert(");
                include_cassert = true;
            }
            if (contains(line, "RefCounted")) {
                replace(line, "RefCounted", "intrusive_ref_counter");
                include_intrusive_ptr = true;
            }
            if (contains(line, "NonnullRefPtr")) {
                replace(line, "NonnullRefPtr", "intrusive_ptr");
                include_intrusive_ptr = true;
            }
            if (contains(line, "RefPtr")) {
                replace(line, "RefPtr", "intrusive_ptr");
                include_intrusive_ptr = true;
            }
            if (contains(line, "Optional")) {
                replace(line, "Optional", "std::optional");
                include_optional = true;
            }
            if (contains(line, " move(")) {
                replace(line, " move(", " std::move(");
            }
            if (contains(line, "(move(")) {
                replace(line, "(move(", "(std::move(");
            }
            if (contains(line, "append(")) {
                auto position = line.find("append(");
                auto parent_token_type = find_parent_token_type(filename, line_num - 1, position,
                                                                tokens_info_map[filename]);
                if (parent_token_type.has_value() && parent_token_type.value() == "StringBuilder") {
                    // StringBuilders are converted to std::string which have an append function!
                    // But this function does not work with chars and push_back needs to be used...
                    auto text_between = text_between_matching_parens(filename, line_num - 1, position, tokens_info_map[filename]);
                    if (text_between) {
                        if (text_between.value().at(0) == '\'')
                            replace(line, "append(", "push_back(");
                    }
                } else
                    replace(line, "append(", "push_back(");
            }
            if (contains(line, "ptr()")) {
                replace(line, "ptr()", "get()");
            }

            if (contains(line, "to_byte_string()")) {
                auto position = line.find("to_byte_string");
                auto parent_token_type = find_parent_token_type(filename, line_num - 1, position,
                                                                tokens_info_map[filename]);
                if (parent_token_type.has_value() && parent_token_type.value() ==
                                                     "StringBuilder") { // StringBuilders are converted to std::string and no to_string is needed
                    replace(line, ".to_byte_string()", "");
                    replace(line, "->to_byte_string()", "");
                } else
                    replace(line, "to_byte_string()", "to_string()");
            }
            if (contains(line, "is_empty()")) {
                replace(line, "is_empty()", "empty()");
            }
            if (contains(line, "verify_cast")) {
                replace(line, "verify_cast", "assert_cast");
                include_util = true;
            }
            if (contains(line, "ScopeLogger")) {
                include_util = true;
            }
            if (contains(line, "extend")) {
                auto position = line.find("extend");
                auto object = object_text(filename, line_num - 1, position, tokens_info_map[filename]);
                if (object.has_value()) {
                    auto text_between = text_between_matching_parens(filename, line_num - 1, position, tokens_info_map[filename]);

                    if (text_between.has_value()) {
                        // Construct a reasonable temporary variable name
                        auto temp_variable_name = text_between.value();
                        replace(temp_variable_name, "m_", "");
                        replace(temp_variable_name, ".", "_");
                        replace(temp_variable_name, "->", "_");
                        replace(temp_variable_name, "(", "");
                        replace(temp_variable_name, ")", "");

                        std::string only_whitespace;
                        auto whitespace = std::copy_if(
                                line.begin(),
                                line.end(),
                                std::back_inserter(only_whitespace),
                                [](auto c) { return std::isspace(c); }
                        );

                        auto format_parameters = fmt::format("{}.begin(), {}.end()", temp_variable_name.c_str(),
                                                             temp_variable_name.c_str()
                        );
                        replace(line, text_between.value().c_str(), format_parameters.c_str());
                        replace(line, "extend(", fmt::format("insert({}.end(), ", object.value()).c_str());
                        line = fmt::format("{}{{\nauto {}    {} = {};\n    {}\n{}}}", only_whitespace, only_whitespace,
                                           temp_variable_name, text_between.value(), line, only_whitespace);
                    }
                }

            }
            if (contains(line, "appendff")) {
                auto position = line.find("appendff");
                auto last_matching_paren = position_of_last_matching_paren(filename, line_num - 1, position,
                                                                           tokens_info_map[filename]);
                if (last_matching_paren) {
                    line.insert(line.begin() + last_matching_paren.value(), ')');
                    replace(line, "appendff", "append(fmt::format");
                }
            }
            if (contains(line, "empend")) {
                replace(line, "empend", "emplace_back");
            }
            if (contains(line, "ByteString::formatted")) {
                replace(line, "ByteString::formatted", "fmt::format");
                include_string = true;
            }
            if (contains(line, "ByteString")) {
                replace(line, "ByteString", "std::string");
                include_string = true;
            }

            if (contains(line, "type_as_byte_string")) {
                replace(line, "type_as_byte_string", "type_as_string");
            }

            if (contains(line, "String ")) {
                auto position = line.find("String ");
                auto tok_index = find_token_index(line_num - 1, position, tokens_info_map[filename]);
                if (tok_index) {
                    if (token_string(filename, tok_index.value(), tokens_info_map[filename]) == "String") {
                        replace(line, "String ", "std::string ");
                        include_string = true;
                    }
                }
            }

            if (contains(line, "AK_MAKE_NONCOPYABLE")) {
                auto position = line.find("AK_MAKE_NONCOPYABLE");
                auto class_name = text_between_matching_parens(filename, line_num - 1, position, tokens_info_map[filename]);
                if (class_name) {
                    std::string whitespace;
                    std::copy_if(
                            line.begin(),
                            line.end(),
                            std::back_inserter(whitespace),
                            [](auto c) { return std::isspace(c); }
                    );
                    line = fmt::format("{}{}({} const&) = delete;", whitespace, class_name.value(), class_name.value());
                }
            }

            if (line == "#include <LibCodeComprehension/Types.h>") {
                add_todo_entry = true;
                continue;
            }

            if (contains(line, "adopt_ref")) {
                auto position = line.find("adopt_ref");
                auto new_statement_text_opt = text_between_matching_parens(filename, line_num - 1, position,
                                                                           tokens_info_map[filename]);
                if (new_statement_text_opt) {
                    auto new_statement_text = new_statement_text_opt.value();
                    auto full_text = fmt::format("adopt_ref({})", new_statement_text);
                    new_statement_text.erase(0, 1); // Remove the '*' character in adopt_ref(* <--
                    replace(line, full_text.c_str(), new_statement_text.c_str());
                }
            }

            if (contains(line, " forward<")) {
                replace(line, " forward<", " std::forward<");
            }

            if (contains(line, "first()")) {
                auto position = line.find("first()");
                auto parent_token_type = find_parent_token_type(filename, line_num - 1, position,
                                                                tokens_info_map[filename]);
                if (parent_token_type.has_value()) {
                    if(parent_token_type.value() == "Vector")
                        replace(line, "first()", "front()");
                }
            }

            converted.push_back(line);
        }

        if (!first_include) {
            outln("finding #pragma once");
            auto pragma_once = std::find(converted.begin(), converted.end(), "#pragma once");
            if (pragma_once == converted.end()) {
                outln("no pragma once");
                return converted;
            }

            first_include = std::distance(converted.begin(), pragma_once);
        }

        int last_include = 0;
        for (int i = 0; i < converted.size(); ++i) {
            if (converted[i].starts_with("#include"))
                last_include = i;
        }

        const char *todo_entry = "          \n"
                                 "namespace CodeComprehension {      \n"
                                 "    struct TodoEntry {             \n"
                                 "        std::string content;       \n"
                                 "        std::string filename;      \n"
                                 "        size_t line { 0 };         \n"
                                 "        size_t column { 0 };       \n"
                                 "    };                             \n"
                                 "}                                  \n";
        if (add_todo_entry)
            converted.insert(converted.begin() + last_include + 1, todo_entry);

        // Add any used debug constants
        for (auto c : debug_constants) {
            converted.insert(converted.begin() + last_include + 1, fmt::format("constexpr bool {} = false;", c));
        }

        // If we are using string view literals we need a using namespace directive
        if (contains(m_content_as_string[filename], "\"sv")) {
            dbgln("Last include: {}", last_include);
            converted.insert(converted.begin() + last_include + 1, "\nusing namespace std::literals;");
        }

        if (include_intrusive_ptr) {
            converted.insert(converted.begin() + first_include.value(),
                             fmt::format("#include \"{}intrusive_ptr.hh\"", m_include_path));
        }
        if (include_util) {
            converted.insert(converted.begin() + first_include.value(),
                             fmt::format("#include \"{}util.hh\"", m_include_path));
        }
        if (include_vector) {
            converted.insert(converted.begin() + first_include.value(), "#include <vector>");
        }
        if (include_string) {
            converted.insert(converted.begin() + first_include.value(), "#include <string>");
        }
        if (include_string_view) {
            converted.insert(converted.begin() + first_include.value(), "#include <string_view>");
        }
        if (include_optional) {
            converted.insert(converted.begin() + first_include.value(), "#include <optional>");
        }
        if (include_cassert) {
            converted.insert(converted.begin() + first_include.value(), "#include <cassert>");
        }

        return converted;
    }
};

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
        outln("Usage: ast_to_std <dst-file> <src-file> ");
        return -1;
    }

    auto input_file_path = arguments[2];
    auto output_file_path = arguments[1];

    outln("arguments: {}\ninput: {}\noutput: {}", arguments.size(), input_file_path, output_file_path);

    TESTS_ROOT_DIR = read_first_line("project_source_dir.txt") + "/test/";



    ConvertAkToStd convert_object;
    convert_object.add_include_filepath_for_output("cpp_parser/");
    convert_object.add_file("Parser.cpp");
    convert_object.add_file("Parser.h");
    auto output_content = convert_object.convert(input_file_path.c_str());

    std::ofstream output(output_file_path);
    if(output.is_open()) {
        for(auto const& line : output_content) {
            output << line << std::endl;
        }
    }

    return 0;
}