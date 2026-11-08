#include "Workload.h"

#include <charconv>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <system_error>

bool nextCommand(std::string_view& text, Command& command) {
    if (text.empty()) {
        return false;
    }

    const auto newline = text.find('\n');
    auto line = text.substr(0, newline);

    if (newline == std::string_view::npos) {
        text = {};
    } else {
        text.remove_prefix(newline + 1);
    }

    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
    }

    if (line == "string") {
        command = {'s', 0, 0};
        return true;
    }

    if (
        line.size() == 6
        && line.substr(0, 5) == "read "
        && (line[5] == '0' || line[5] == '1')
    ) {
        command = {'r', line[5] - '0', 0};
        return true;
    }

    if (
        line.size() >= 9
        && line.substr(0, 6) == "write "
        && (line[6] == '0' || line[6] == '1')
        && line[7] == ' '
    ) {
        int value = 0;

        const char* last = line.data() + line.size();

        const auto parsed = std::from_chars(
            line.data() + 8,
            last,
            value
        );

        if (parsed.ec == std::errc{} && parsed.ptr == last) {
            command = {'w', line[6] - '0', value};
            return true;
        }
    }

    throw std::runtime_error(
        "Invalid command: " + std::string(line)
    );
}

void generateFile(
    const char* filename,
    std::size_t operations,
    Mix mix,
    unsigned int seed
) {
    if (
        operations % 20 != 0
        || mix.read0 < 0
        || mix.write0 < 0
        || mix.read1 < 0
        || mix.write1 < 0
        || mix.asString < 0
        || mix.read0 + mix.write0 + mix.read1
           + mix.write1 + mix.asString != 20
    ) {
        throw std::invalid_argument(
            "Invalid operation count or mix"
        );
    }

    std::ofstream output(filename);

    if (!output) {
        throw std::runtime_error(
            "Cannot create: " + std::string(filename)
        );
    }

    std::mt19937 generator(seed);

    for (
        std::size_t block = 0;
        block < operations / 20;
        ++block
    ) {
        Mix left = mix;

        for (int remaining = 20; remaining > 0; --remaining) {
            int ticket =
                std::uniform_int_distribution<int>(
                    0,
                    remaining - 1
                )(generator);

            auto take = [&ticket](int& count) {
                if (ticket < count) {
                    --count;
                    return true;
                }

                ticket -= count;
                return false;
            };

            if (take(left.read0)) {
                output << "read 0\n";
            } else if (take(left.write0)) {
                output << "write 0 1\n";
            } else if (take(left.read1)) {
                output << "read 1\n";
            } else if (take(left.write1)) {
                output << "write 1 1\n";
            } else {
                output << "string\n";
            }
        }
    }

    output.close();

    if (!output) {
        throw std::runtime_error(
            "Cannot finish: " + std::string(filename)
        );
    }
}

std::string loadFile(const char* filename) {
    std::ifstream input(filename);

    if (!input) {
        throw std::runtime_error(
            "Cannot open: " + std::string(filename)
        );
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    if (input.bad() || !buffer) {
        throw std::runtime_error(
            "Cannot read: " + std::string(filename)
        );
    }

    return buffer.str();
}

std::size_t countCommands(const std::string& text) {
    std::string_view remaining(text);

    Command command;
    std::size_t count = 0;

    while (nextCommand(remaining, command)) {
        ++count;
    }

    return count;
}

void splitCommands(
    const std::string& text,
    std::size_t operations,
    int threads,
    std::string& first,
    std::string& second,
    std::string& third
) {
    if (
        threads < 1
        || threads > 3
        || operations % threads != 0
    ) {
        throw std::invalid_argument("Invalid thread count");
    }

    std::size_t position = 0;

    auto takePart = [&]() {
        const auto start = position;

        for (
            std::size_t i = 0;
            i < operations / threads;
            ++i
        ) {
            const auto newline = text.find('\n', position);

            if (newline == std::string::npos) {
                throw std::runtime_error(
                    "Missing command or newline"
                );
            }

            position = newline + 1;
        }

        return text.substr(start, position - start);
    };

    first = takePart();
    second = threads >= 2 ? takePart() : "";
    third = threads == 3 ? takePart() : "";

    if (position != text.size()) {
        throw std::runtime_error("Unexpected extra commands");
    }
}