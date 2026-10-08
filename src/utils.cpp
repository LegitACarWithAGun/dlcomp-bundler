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
	// luau treats the first character as 0,0; while vscode uses 1,1
	// seperated by colon to allow jumping to the position when appended to path
	return std::format("{}:{}", pos.line + 1, pos.column + 1);
}
std::string fmtLocation(Luau::Location location) {
	return std::format("{} - {}", fmtPosition(location.begin), fmtPosition(location.end));
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

void write_luau_parse_errors(std::ostream& stream, std::optional<std::filesystem::path> path, std::vector<Luau::ParseError> errors) {
	stream << "PARSE ERRORS:" << std::endl;
	for (auto err : errors) {

		auto location = err.getLocation();
		if (path.has_value()) {
			stream << std::format("\t@ {}:{}", pathAbsGeneric(path.value()), fmtPosition(location.begin)) << std::endl;
		}

		stream << "\t- area: " << fmtLocation(location) << std::endl;
		stream << "\t- what: " << err.getMessage() << std::endl;
	}
}

template <typename T>
Luau::AstArray<T> InitAstArrayOfSize(Luau::Allocator& allocator, size_t size) {
	Luau::AstArray<T> result;
	result.size = size;
	result.data = static_cast<T*>(allocator.allocate(sizeof(T) * size));
	return result;
}

// this is directly copying AstArray<T> Parser::copy
// please reffer to that
template <typename T>
Luau::AstArray<T> CopyToAstArray(Luau::Allocator& allocator, T* data, size_t size) {
	Luau::AstArray<T> result = InitAstArrayOfSize<T>(allocator, size);

	// This is equivalent to std::uninitialized_copy, but without the exception guarantee
	// since our types don't have destructors
	for (size_t i = 0; i < size; ++i)
		new (result.data + i) T(data[i]);

	return result;
}

template <typename T>
Luau::AstArray<T> AstArrayPushFront(Luau::Allocator& allocator, Luau::AstArray<T> arr, T add) {
	size_t size = 1 + arr.size;
	Luau::AstArray<T> result = InitAstArrayOfSize<T>(allocator, size);

	new (result.data) T(add);
	for (auto i = 0; i < arr.size; i++) {
		new (result.data + 1 + i) T(arr.data[i]);
	}

	return result;
}
