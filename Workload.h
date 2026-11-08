#pragma once

#include <cstddef>
#include <string>
#include <string_view>

struct Mix {
    int read0;
    int write0;
    int read1;
    int write1;
    int asString;
};

struct Command {
    char type = 's';
    int field = 0;
    int value = 0;
};

bool nextCommand(std::string_view& text, Command& command);

void generateFile(
    const char* filename,
    std::size_t operations,
    Mix mix,
    unsigned int seed
);

std::string loadFile(const char* filename);

std::size_t countCommands(const std::string& text);

void splitCommands(
    const std::string& text,
    std::size_t operations,
    int threads,
    std::string& first,
    std::string& second,
    std::string& third
);