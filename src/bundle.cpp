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

// scopes every declared global function to the root (turns them into a local)
// to ensure they cant override any functions declared in other modules/files
class ScopeGlobalFunctionsVisitor : public Luau::AstVisitor {
  public:
	Luau::AstStatBlock* treeRoot;
	Luau::Allocator& allocator;

	ScopeGlobalFunctionsVisitor(Luau::AstStatBlock* treeRoot, Luau::Allocator& allocator) : treeRoot(treeRoot), allocator(allocator) {};

	virtual bool visit(class Luau::AstStatFunction* node) {
		auto nameInfo = node->name;

		if (nameInfo->is<Luau::AstExprLocal>()) {
			return false;
		}
		if (nameInfo->is<Luau::AstExprIndexName>()) {
			return false;
		}

		auto nameAsGlobal = nameInfo->as<Luau::AstExprGlobal>();
		if (!nameAsGlobal) {
			std::cout << "unkown node used for function name @ " << fmtLocation(nameAsGlobal->location) << " (" << nameAsGlobal->classIndex
			          << "): " << typeid(*nameAsGlobal).name() << std::endl;
			return false;
		}

		auto createdLocalName = allocator.alloc<Luau::AstLocal>(nameAsGlobal->name, nameAsGlobal->location, nullptr, 0, 0, nullptr);
		auto createdNode = allocator.alloc<Luau::AstStatLocal>(nameAsGlobal->location, CopyToAstArray(allocator, &createdLocalName, 1),
		                                                       Luau::AstArray<Luau::AstExpr*>{}, std::nullopt);

		// TODO: only add 1 node per module instead of 1 node per variable
		// as in: local a, b, c
		treeRoot->body = AstArrayPushFront(allocator, treeRoot->body, static_cast<Luau::AstStat*>(createdNode));

		return false;
	}
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

	ScopeGlobalFunctionsVisitor functionVisitor{parseResult.root, luauAllocator};
	parseResult.root->visit(&functionVisitor);

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
