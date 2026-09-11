#include "main.h"
#include <climits>
#include <csignal>
#include <libgen.h>

struct Error {
	const std::string message;
	const std::string filePath;
	const std::string function;
	const std::string source;
	const std::string line;
};

static bool isProgressBarActive = false;
static uint32_t filesSkipped = 0;

static struct {
	bool showHelp = false;
	bool silentAssertions = false;
	bool forceOverwrite = false;
	bool ignoreDebugInfo = false;
	bool minimizeDiffs = false;
	bool unrestrictedAscii = false;
	std::string inputPath;
	std::string outputPath;
	std::string extensionFilter;
} arguments;

struct Directory {
	const std::string path;
	std::vector<Directory> folders;
	std::vector<std::string> files;
};

std::string path_find_extension(const std::string& filename) {
	size_t pos = filename.rfind('.');
	if (pos == std::string::npos) return "";
	return filename.substr(pos);
}

std::string path_remove_extension(const std::string& filename) {
	size_t pos = filename.rfind('.');
	if (pos == std::string::npos) return filename;
	return filename.substr(0, pos);
}

std::string path_find_filename(const std::string& path) {
	size_t pos = path.find_last_of("/\\");
	if (pos == std::string::npos) return path;
	return path.substr(pos + 1);
}

bool path_is_directory(const std::string& path) {
	struct stat st;
	if (stat(path.c_str(), &st) != 0) return false;
	return S_ISDIR(st.st_mode);
}

bool path_exists(const std::string& path) {
	struct stat st;
	return stat(path.c_str(), &st) == 0;
}

std::string get_executable_dir() {
	char buf[PATH_MAX];
	ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
	if (len == -1) return "./";
	buf[len] = '\0';
	std::string exePath(buf);
	size_t pos = exePath.find_last_of('/');
	if (pos == std::string::npos) return "./";
	return exePath.substr(0, pos + 1);
}

static std::string string_to_lowercase(const std::string& string) {
	std::string lowercaseString = string;

	for (uint32_t i = lowercaseString.size(); i--;) {
		if (lowercaseString[i] < 'A' || lowercaseString[i] > 'Z') continue;
		lowercaseString[i] += 'a' - 'A';
	}

	return lowercaseString;
}

static void find_files_recursively(Directory& directory) {
	DIR* dir = opendir((arguments.inputPath + directory.path).c_str());
	if (!dir) return;

	struct dirent* entry;
	while ((entry = readdir(dir)) != nullptr) {
		std::string name = entry->d_name;
		if (name == "." || name == "..") continue;

		std::string fullPath = arguments.inputPath + directory.path + name;

		if (path_is_directory(fullPath)) {
			directory.folders.emplace_back(Directory{ .path = directory.path + name + "/" });
			find_files_recursively(directory.folders.back());
			if (!directory.folders.back().files.size() && !directory.folders.back().folders.size()) directory.folders.pop_back();
			continue;
		}

		if (!arguments.extensionFilter.size() || arguments.extensionFilter == string_to_lowercase(path_find_extension(name)))
			directory.files.emplace_back(name);
	}

	closedir(dir);
}

static bool decompile_files_recursively(const Directory& directory) {
	mkdir((arguments.outputPath + directory.path).c_str(), 0755);
	std::string outputFile;

	for (uint32_t i = 0; i < directory.files.size(); i++) {
		outputFile = path_remove_extension(directory.files[i]) + ".lua";

		Bytecode bytecode(arguments.inputPath + directory.path + directory.files[i]);
		Ast ast(bytecode, arguments.ignoreDebugInfo, arguments.minimizeDiffs);
		Lua lua(bytecode, ast, arguments.outputPath + directory.path + outputFile, arguments.forceOverwrite, arguments.minimizeDiffs, arguments.unrestrictedAscii);

		try {
			print("--------------------\nInput file: " + bytecode.filePath + "\nReading bytecode...");
			bytecode();
			print("Building ast...");
			ast();
			print("Writing lua source...");
			lua();
			print("Output file: " + lua.filePath);
		} catch (const Error& error) {
			erase_progress_bar();

			if (arguments.silentAssertions) {
				print("\nError running " + error.function + "\nSource: " + error.source + ":" + error.line + "\n\n" + error.message);
				filesSkipped++;
				continue;
			}

			fprintf(stderr, "\nError running %s\nSource: %s:%s\n\nFile: %s\n\n%s\n",
				error.function.c_str(), error.source.c_str(), error.line.c_str(),
				error.filePath.c_str(), error.message.c_str());

			fprintf(stderr, "\nOptions: [s]kip file, [r]etry, [q]uit: ");
			int ch = getchar();
			if (ch != EOF) { while (getchar() != '\n' && ch != EOF); }

			if (ch == 'q' || ch == 'Q') {
				return false;
			} else if (ch == 'r' || ch == 'R') {
				print("Retrying...");
				i--;
				continue;
			} else {
				print("File skipped.");
				filesSkipped++;
			}
		} catch (...) {
			fprintf(stderr, "Unknown exception\n\nFile: %s\n", bytecode.filePath.c_str());
			throw;
		}
	}

	for (uint32_t i = 0; i < directory.folders.size(); i++) {
		if (!decompile_files_recursively(directory.folders[i])) return false;
	}

	return true;
}

static char* parse_arguments(const int& argc, char** const& argv) {
	if (argc < 2) return nullptr;
	arguments.inputPath = argv[1];
	bool isInputPathSet = true;

	if (arguments.inputPath.size() && arguments.inputPath.front() == '-') {
		arguments.inputPath.clear();
		isInputPathSet = false;
	}

	std::string argument;

	for (uint32_t i = isInputPathSet ? 2 : 1; i < argc; i++) {
		argument = argv[i];

		if (argument.size() >= 2 && argument.front() == '-') {
			if (argument[1] == '-') {
				argument = argument.c_str() + 2;

				if (argument == "extension") {
					if (i <= argc - 2) {
						i++;
						arguments.extensionFilter = argv[i];
						continue;
					}
				} else if (argument == "force_overwrite") {
					arguments.forceOverwrite = true;
					continue;
				} else if (argument == "help") {
					arguments.showHelp = true;
					continue;
				} else if (argument == "ignore_debug_info") {
					arguments.ignoreDebugInfo = true;
					continue;
				} else if (argument == "minimize_diffs") {
					arguments.minimizeDiffs = true;
					continue;
				} else if (argument == "output") {
					if (i <= argc - 2) {
						i++;
						arguments.outputPath = argv[i];
						continue;
					}
				} else if (argument == "silent_assertions") {
					arguments.silentAssertions = true;
					continue;
				} else if (argument == "unrestricted_ascii") {
					arguments.unrestrictedAscii = true;
					continue;
				}
			} else if (argument.size() == 2) {
				switch (argument[1]) {
				case 'e':
					if (i > argc - 2) break;
					i++;
					arguments.extensionFilter = argv[i];
					continue;
				case 'f':
					arguments.forceOverwrite = true;
					continue;
				case '?':
				case 'h':
					arguments.showHelp = true;
					continue;
				case 'i':
					arguments.ignoreDebugInfo = true;
					continue;
				case 'm':
					arguments.minimizeDiffs = true;
					continue;
				case 'o':
					if (i > argc - 2) break;
					i++;
					arguments.outputPath = argv[i];
					continue;
				case 's':
					arguments.silentAssertions = true;
					continue;
				case 'u':
					arguments.unrestrictedAscii = true;
					continue;
				}
			}
		}

		return argv[i];
	}

	return nullptr;
}

int main(int argc, char* argv[]) {
	print(std::string(PROGRAM_NAME) + "\nCompiled on " + __DATE__);
	
	if (parse_arguments(argc, argv)) {
		print("Invalid argument: " + std::string(parse_arguments(argc, argv)) + "\nUse -? to show usage and options.");
		return EXIT_FAILURE;
	}
	
	if (arguments.showHelp) {
		print(
			"Usage: luajit-decompiler-v2 INPUT_PATH [options]\n"
			"\n"
			"Available options:\n"
			"  -h, -?, --help\t\tShow this message\n"
			"  -o, --output OUTPUT_PATH\tOverride default output directory\n"
			"  -e, --extension EXTENSION\tOnly decompile files with the specified extension\n"
			"  -s, --silent_assertions\tDisable assertion error pop-up window\n"
			"\t\t\t\t  and auto skip files that fail to decompile\n"
			"  -f, --force_overwrite\t\tAlways overwrite existing files\n"
			"  -i, --ignore_debug_info\tIgnore bytecode debug info\n"
			"  -m, --minimize_diffs\t\tOptimize output formatting to help minimize diffs\n"
			"  -u, --unrestricted_ascii\tDisable default UTF-8 encoding and string restrictions"
		);
		return EXIT_SUCCESS;
	}
	
	if (!arguments.inputPath.size()) {
		print("No input path specified!");
		return EXIT_FAILURE;
	}

	if (!arguments.outputPath.size()) {
		arguments.outputPath = get_executable_dir() + "output/";
	} else {
		if (!path_exists(arguments.outputPath)) {
			print("Failed to open output path: " + arguments.outputPath);
			return EXIT_FAILURE;
		}

		if (!path_is_directory(arguments.outputPath)) {
			print("Output path is not a folder!");
			return EXIT_FAILURE;
		}

		switch (arguments.outputPath.back()) {
		case '/':
			break;
		default:
			arguments.outputPath += '/';
			break;
		}
	}

	if (arguments.extensionFilter.size()) {
		if (arguments.extensionFilter.front() != '.') arguments.extensionFilter.insert(arguments.extensionFilter.begin(), '.');
		arguments.extensionFilter = string_to_lowercase(arguments.extensionFilter);
	}

	if (!path_exists(arguments.inputPath)) {
		print("Failed to open input path: " + arguments.inputPath);
		return EXIT_FAILURE;
	}

	Directory root;

	if (path_is_directory(arguments.inputPath)) {
		switch (arguments.inputPath.back()) {
		case '/':
			break;
		default:
			arguments.inputPath += '/';
			break;
		}

		find_files_recursively(root);

		if (!root.files.size() && !root.folders.size()) {
			print("No files " + (arguments.extensionFilter.size() ? "with extension " + arguments.extensionFilter + " " : "") + "found in path: " + arguments.inputPath);
			return EXIT_FAILURE;
		}
	} else {
		root.files.emplace_back(path_find_filename(arguments.inputPath));
		arguments.inputPath = path_remove_extension(arguments.inputPath);
		arguments.inputPath = path_find_filename(arguments.inputPath);
		arguments.inputPath = arguments.inputPath.substr(0, arguments.inputPath.size() - path_find_filename(arguments.inputPath).size());
	}

	try {
		if (!decompile_files_recursively(root)) {
			print("--------------------\nAborted!");
			return EXIT_FAILURE;
		}
	} catch (...) {
		throw;
	}

	print("--------------------\n" + (filesSkipped ? "Failed to decompile " + std::to_string(filesSkipped) + " file" + (filesSkipped > 1 ? "s" : "") + ".\n" : "") + "Done!");
	return EXIT_SUCCESS;
}

void print(const std::string& message) {
	printf("%s\n", message.c_str());
}

void print_progress_bar(const double& progress, const double& total) {
	static char PROGRESS_BAR[] = "\r[====================]";

	const uint8_t threshold = std::round(20 / total * progress);

	for (uint8_t i = 20; i--;) {
		PROGRESS_BAR[i + 2] = i < threshold ? '=' : ' ';
	}

	printf("%s", PROGRESS_BAR);
	fflush(stdout);
	isProgressBarActive = true;
}

void erase_progress_bar() {
	static constexpr char PROGRESS_BAR_ERASER[] = "\r                      \r";

	if (!isProgressBarActive) return;
	printf("%s", PROGRESS_BAR_ERASER);
	fflush(stdout);
	isProgressBarActive = false;
}

void assert(const bool& assertion, const std::string& message, const std::string& filePath, const std::string& function, const std::string& source, const uint32_t& line) {
	if (!assertion) throw Error{
		.message = message,
		.filePath = filePath,
		.function = function,
		.source = source,
		.line = std::to_string(line)
	};
}

std::string byte_to_string(const uint8_t& byte) {
	char string[] = "0x00";
	uint8_t digit;
	
	for (uint8_t i = 2; i--;) {
		digit = (byte >> i * 4) & 0xF;
		string[3 - i] = digit >= 0xA ? 'A' + digit - 0xA : '0' + digit;
	}

	return string;
}
