/*
Requirements:
  C++20
  POSIX API
  Default char is unsigned (-funsigned-char)
*/

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <unordered_map>
#include <vector>

#define DEBUG_INFO __FUNCTION__, __FILE__, __LINE__

constexpr char PROGRAM_NAME[] = "LuaJIT Decompiler v2";
constexpr uint64_t DOUBLE_SIGN = 0x8000000000000000;
constexpr uint64_t DOUBLE_EXPONENT = 0x7FF0000000000000;
constexpr uint64_t DOUBLE_FRACTION = 0x000FFFFFFFFFFFFF;
constexpr uint64_t DOUBLE_SPECIAL = DOUBLE_EXPONENT;
constexpr uint64_t DOUBLE_NEGATIVE_ZERO = DOUBLE_SIGN;

void print(const std::string& message);
void print_progress_bar(const double& progress = 0, const double& total = 100);
void erase_progress_bar();
void assert(const bool& assertion, const std::string& message, const std::string& filePath, const std::string& function, const std::string& source, const uint32_t& line);
std::string byte_to_string(const uint8_t& byte);

std::string path_find_extension(const std::string& filename);
std::string path_remove_extension(const std::string& filename);
std::string path_find_filename(const std::string& path);
bool path_is_directory(const std::string& path);
bool path_exists(const std::string& path);
std::string get_executable_dir();

class Bytecode;
class Ast;
class Lua;

#include "bytecode/bytecode.h"
#include "ast/ast.h"
#include "lua/lua.h"
