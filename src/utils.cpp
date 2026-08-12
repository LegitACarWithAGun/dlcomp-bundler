#pragma once

#include "Luau/Ast.h"
#include <filesystem>
#include <format>
#include <fstream>

// reads the entire file into a string
std::string readFile(std::filesystem::path path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error(std::format("opening file for reading {}", path.string()));
    }

    std::string output;
    char buffer[5];

    file.seekg(0);
    while (!file.eof()) {
        file.read(buffer, sizeof(buffer));
        output.append(buffer, file.gcount());
        // output.insert(output.end(), buffer, buffer + file.gcount());
    }
    file.close();

    return output;
}

void writeFile(std::filesystem::path path, std::string contents) {
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    file << contents;
    file.close();
}

std::string fmtPosition(Luau::Position pos) {
    return std::format("{}x{}", pos.line, pos.column);
}
std::string fmtLocation(Luau::Location location) {
    return std::format("{}-{}", fmtPosition(location.begin), fmtPosition(location.end));
}

size_t positionToIndex(std::string text, Luau::Position position) {
    size_t lineStart = 0;
    for (int line = 0; line < position.line; line++) {
        lineStart = text.find("\n", lineStart) + 1;
    }
    return lineStart + position.column;
}
std::string overwriteLocation(std::string fileContents, Luau::Location location, std::string replaceWith) {
    auto startIndex = positionToIndex(fileContents, location.begin);
    auto length = positionToIndex(fileContents, location.end) - startIndex;
    return fileContents.replace(startIndex, length, replaceWith);
}

std::string pathAbsGeneric(std::filesystem::path path) {
    return std::filesystem::absolute(path).generic_string();
}

void replace_all_substring(std::string& text, std::string find, std::string replace) {
    while (true) {
        std::size_t pos = text.find(find);
        if (pos == std::string::npos) {
            break;
        }

        text = text.replace(pos, find.size(), replace);
    }
}
