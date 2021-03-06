// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "ast.hh"
#include "compiler.hh"
#include "context.hh"
#include "json.hh"
#include "string_util.hh"
#include "log.hh"
#include "schema.hh"
#include "validate.hh"

#include <string>
#include <string_view>
#include <filesystem>
#include <iostream>
#include <vector>
#include <fstream>

namespace fs = std::filesystem;

namespace {
    struct Config {
        fs::path input;
        fs::path output;
        fs::path deps;
        std::vector<fs::path> search;

        enum class Mode {
            Compile,
            Help,
        } mode = Mode::Compile;
    };

    bool parse_arguments(int argc, char* argv[], Config& config) {
        using namespace sapc;

        enum class Arg {
            None,
            InputFile,
            OutputFile,
            DepsFile,
            IncludePath,
        } mode = Arg::None;
        std::string_view mode_argument;

        bool allow_options = true;

        for (int arg_index = 1; arg_index != argc; ++arg_index) {
            auto arg = std::string_view{ argv[arg_index] };
            auto const original_arg = arg;

            switch (mode) {
            case Arg::OutputFile:
                config.output = arg;
                mode = Arg::None;
                break;
            case Arg::DepsFile:
                config.deps = arg;
                mode = Arg::None;
                break;
            case Arg::IncludePath:
                config.search.push_back(fs::path{ arg }.make_preferred());
                mode = Arg::None;
                break;
            default:
                if (arg == "--") {
                    allow_options = false;
                    break;
                }
                else if (allow_options && starts_with(arg, "--"))
                    arg = arg.substr(2);
                else if (allow_options && (starts_with(arg, "/") || starts_with(arg, "-")))
                    arg = arg.substr(1);
                else if (config.input.empty()) {
                    config.input = fs::path{ arg }.make_preferred();
                    break;
                }
                else {
                    std::cerr << "Unexpected command parameter '" << arg << "'\n";
                    return false;
                }

                mode_argument = original_arg;
                if (arg == "i" || arg == "input")
                    mode = Arg::InputFile;
                else if (arg == "o" || arg == "output")
                    mode = Arg::OutputFile;
                else if (arg == "d" || arg == "deps")
                    mode = Arg::DepsFile;
                else if (arg == "h" || arg == "help")
                    config.mode = Config::Mode::Help;
                else if (starts_with(arg, "I") && arg.size() > 1)
                    config.search.push_back(fs::path{ arg.substr(1) }.make_preferred());
                else if (starts_with(arg, "I") && arg.size() == 1)
                    mode = Arg::IncludePath;
                else if (starts_with(original_arg, "/") && config.input.empty())
                    config.input = fs::path{ original_arg }.make_preferred();
                else {
                    std::cerr << "error: Unknown command argument '" << original_arg << "'\n";
                    return false;
                }

                break;
            }
        }

        if (mode != Arg::None) {
            std::cerr << "error: Expected parameter after '" << mode_argument << "'\n";
            return false;
        }

        return true;
    }
}

static int compile(Config& config) {
    if (config.input.empty()) {
        std::cerr << "error: No input file provided; use --help to see options\n";
        return 1;
    }

    sapc::Log log;

    sapc::Context ctx;
    ctx.targetFile = config.input;
    ctx.searchPaths = config.search;

    auto const compiled = compile(ctx, log);
    if (!compiled && log.lines.empty())
        log.error({ ctx.targetFile }, "Failed to compile input");

    auto const valid = compiled && validate(*ctx.rootModule, log);

    for (auto const& line : log.lines)
        std::cerr << line << '\n';

    if (!compiled)
        return 2;
    if (!valid)
        return 4;

    auto const doc = sapc::serializeToJson(*ctx.rootModule);
    auto const json = doc.dump(4);

    if (!config.output.empty()) {
        std::ofstream output_stream(config.output);
        if (!output_stream) {
            std::cerr << "error: Failed to open '" << config.output.string() << "' for writing\n";
            return 3;
        }
        output_stream << json << '\n';
    }
    else
        std::cout << json << '\n';

    if (!config.deps.empty() && !config.output.empty()) {
        std::ofstream deps_stream(config.deps);
        if (!deps_stream) {
            std::cerr << "error: Failed to open '" << config.deps.string() << "' for writing\n";
            return 3;
        }

        deps_stream << fs::relative(config.output).string() << ": ";

        auto const num_deps = ctx.dependencies.size();
        for (size_t i = 0; i != num_deps; ++i) {
            if (i != 0)
                deps_stream << "  ";

            deps_stream << fs::relative(ctx.dependencies[i]).string() << ' ';

            if (i != num_deps - 1)
                deps_stream << '\\';

            deps_stream << '\n';
        }
    }

    return 0;
}

static int help(std::filesystem::path program) {
    std::cout <<
        "usage: " << program.filename().string() << " [-I<path>]... [-o <input>] [-d <depfile>] [-h] [--] <input>\n" <<
        "  -I<path>              Add a path to the search list for importsand includes\n" <<
        "  -o|--output <ouput>   Output file path, otherwise prints to stdout\n"
        "  -d|--deps <depfile>   Specify the path that a Make-style deps file will be written to for build system integration\n" <<
        "  -h|--help             Print this help information\n" <<
        "  <input>               The input sap IDL file\n";
    return 0;
}

int main(int argc, char* argv[]) {
    fs::path inputPath;

    Config config;
    if (!parse_arguments(argc, argv, config)) {
        return 1;
    }

    switch (config.mode) {
    case Config::Mode::Compile: return compile(config);
    case Config::Mode::Help: return help(argv[0]);
    default: return 1;
    }
}
