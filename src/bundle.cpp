#pragma once

#include "Luau/Ast.h"
#include "fileTree.cpp"
#include "minify.cpp"
#include "utils.cpp"
#include <format>
#include <regex>

const char* bundle_file_template = {
#include "bundle-template.luau"
};


std::string fmtModule(const luaFile& file) {
	auto contents = file.contents;

	// replace require calls
	auto imports = file.imports;
	for (auto importIt = imports.rbegin(); importIt != imports.rend(); importIt++) {
		auto import = *importIt;
		// lua arrays are 1-indexed, so add +1
		auto replace_with = std::format("__BUNDLE.require({})", import.targetFile->importId + 1);
		contents = overwriteLocation(contents, import.callLocation, replace_with);
	}

	// parse file
	Luau::Allocator luauAllocator;
	Luau::AstNameTable nameTable(luauAllocator);
	Luau::ParseOptions parseOptions;
	Luau::ParseResult parseResult = Luau::Parser::parse(contents.data(), contents.size(), nameTable, luauAllocator, parseOptions);
	if (parseResult.errors.size() > 0) {
		write_luau_parse_errors(std::cerr, file.path, parseResult.errors);
		throw std::runtime_error("parse errors");
	}

	MinifyVisitor minifyVisitor{};
	parseResult.root->visit(&minifyVisitor);

	return std::format("function() {} end", minifyVisitor.output);
}

// TODO: take in a config
std::string bundleFileTree(std::vector<luaFile*> files) {
	std::string output = bundle_file_template;
	replace_all_substring(output, "[[NAME]]", "test string");

	std::vector<std::string> module_constructors;
	for (auto file : files) {
		std::cout << "\t- formatting file " << file->path.generic_string() << std::endl;
		module_constructors.push_back("\t" + fmtModule(*file));
	}
	replace_all_substring(output, "[[CONSTRUCTORS]]", implode(module_constructors, ",\n"));

	return output;
}
