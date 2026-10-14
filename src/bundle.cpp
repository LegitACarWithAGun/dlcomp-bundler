#pragma once

#include "Luau/Ast.h"
#include "astEdit.cpp"
#include "config.cpp"
#include "fileTree.cpp"
#include "minify.cpp"
#include "utils.cpp"
#include <format>
#include <regex>
namespace config = dlcomp::bundler::config;
namespace astEdit = dlcomp::bundler::astEdit;

const char* bundle_file_template = {
#include "bundle-template.luau"
};

// gathers every declared global function
// used to scope them to them to the module, so they cant override any functions declared in other modules/files
class ScopeDeclaredGlobalFunctions : public Luau::AstVisitor {
  public:
	Luau::Allocator& allocator;
	std::vector<Luau::AstLocal*> result;

	ScopeDeclaredGlobalFunctions(Luau::Allocator& allocator) : allocator(allocator) {};

	virtual bool visit(class Luau::AstStatFunction* node) {
		auto nameInfo = node->name;

		// already not a global
		if (nameInfo->is<Luau::AstExprLocal>())
			return true;
		if (nameInfo->is<Luau::AstExprIndexName>())
			return true;

		Luau::AstLocal* createdLocal;

		// turn the name into a local
		auto nameAsGlobal = nameInfo->as<Luau::AstExprGlobal>();
		if (nameAsGlobal) {
			createdLocal = allocator.alloc<Luau::AstLocal>(nameAsGlobal->name, nameAsGlobal->location, nullptr, 0, 0, nullptr);
		}

		// didnt know what to do with the node, so throw it
		if (!createdLocal) {
			std::cout << "unkown node used for function name @ " << fmtLocation(node->location) << " (" << nameInfo->classIndex
			          << "): " << typeid(*nameAsGlobal).name() << std::endl;
			return true;
		}

		result.push_back(createdLocal);
		// override the node used to store the function name
		// I dont know if upvalue is correct here tho
		node->name = allocator.alloc<Luau::AstExprLocal>(astEdit::unknownLocation, createdLocal, node->func->functionDepth > 0);

		return true;
	}

	void prependToBlock(Luau::AstStatBlock* node) {
		if (result.size() == 0)
			return;

		auto scopedNames = astEdit::NodeAstStatLocal(allocator, result);
		node->body = astEdit::AstArrayPushFrontNoCopy(allocator, node->body, static_cast<Luau::AstStat*>(scopedNames));
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
	Luau::Allocator allocator;
	Luau::AstNameTable nameTable(allocator);
	Luau::ParseOptions parseOptions;
	Luau::ParseResult parseResult = Luau::Parser::parse(contents.data(), contents.size(), nameTable, allocator, parseOptions);
	if (parseResult.errors.size() > 0) {
		write_luau_parse_errors(std::cerr, file.path, parseResult.errors);
		throw std::runtime_error("parse errors");
	}
	auto root = parseResult.root;

	// scopes every declared global function to the root (turns them into a local)
	// to ensure they cant override any functions declared in other modules/files
	ScopeDeclaredGlobalFunctions functionVisitor{allocator};
	root->visit(&functionVisitor);
	functionVisitor.prependToBlock(root);

	// minify module
	MinifyVisitor minifyVisitor{};
	root->visit(&minifyVisitor);

	return std::format("function() {} end", minifyVisitor.output);
}

std::string bundleFileTree(const std::vector<luaFile*>& files, const config::BundleOptions& bundleOptions) {
	std::string output = bundle_file_template;
	replace_all_substring(output, "[[NAME]]", bundleOptions.name);

	std::vector<std::string> module_constructors;
	for (auto file : files) {
		std::cout << "\t- formatting file " << pathAbsGeneric(file->path) << std::endl;
		module_constructors.push_back("\t" + fmtModule(*file));
	}
	replace_all_substring(output, "[[CONSTRUCTORS]]", implode(module_constructors, ",\n"));

	return output;
}
