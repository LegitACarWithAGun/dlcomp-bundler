#pragma once

#include "Luau/Ast.h"
#include "Luau/Parser.h"
#include "utils.cpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>

static std::unordered_set<std::string> fileExtensions = {".lua", ".luau"};

typedef int importId;
typedef std::filesystem::path filePath;

struct importCall;
struct luaFile {
    filePath path;
    // raw file contents
    std::string contents;
    // bundle.cpp/fmtModule expects this to be in ascending order by location; higher locations later
    std::vector<importCall> imports;
    int dependantCount;

    // not filled out by generateFileTree!! call flattenFileTreeDependantFirst
    importId importId;
};
struct importCall {
    Luau::Location callLocation;
    luaFile *targetFile;
};

bool ignoreRequireTarget(std::string target) {
    std::transform(target.begin(), target.end(), target.begin(), [](unsigned char c) { return std::tolower(c); });

    // let the user make network requires, we only care about local files
    return target.starts_with("http://") || target.starts_with("https://");
}

struct requireCall {
    Luau::Location location;
    std::string target;
};
class RequireCallVisitor : public Luau::AstVisitor {
  public:
    std::vector<requireCall> results;

    virtual bool visit(class Luau::AstExprCall *node) {
        Luau::AstExprGlobal *global = node->func->as<Luau::AstExprGlobal>();
        if (!global) {
            return true;
        }

        std::string name = global->name.value;
        if (name != "require") {
            return true;
        }

        if (node->args.size != 1) {
            throw std::runtime_error(std::format("require call has more/less than exactly 1 arg; location: {}", fmtLocation(node->location)));
        }

        auto arg = node->args.data[0]->as<Luau::AstExprConstantString>();
        if (!arg) {
            std::cout << std::format("found require call that doesnt use a constant string; location: {}", fmtLocation(node->location)) << std::endl;
            return true;
            // throw std::runtime_error(std::format("require call's arg isnt a constant string; location: {}",
            // fmtLocation(node->location)));
        }

        results.push_back(requireCall{
            .location = node->location,
            .target = arg->value.data,
        });

        return false;
    };
};
std::vector<requireCall> getRequireCalls(std::string scriptContents) {
    Luau::Allocator luauAllocator;
    Luau::AstNameTable nameTable(luauAllocator);
    Luau::ParseOptions parseOptions;

    Luau::ParseResult parseResult = Luau::Parser::parse(scriptContents.data(), scriptContents.size(), nameTable, luauAllocator, parseOptions);
    if (parseResult.errors.size() > 0) {
        // TODO
        throw std::runtime_error("parse errors");
    }

    RequireCallVisitor visitor;
    parseResult.root->visit(&visitor);
    return visitor.results;
}

std::optional<filePath> attemptExtensions(filePath path) {
    std::optional<filePath> result;

    for (auto extension : fileExtensions) {
        auto newPath = path;
        newPath.concat(extension);
        if (!std::filesystem::is_regular_file(newPath)) {
            continue;
        }
        if (result.has_value()) {
            return std::nullopt;
        }
        result = newPath;
    }

    return result;
}
filePath correctPath(filePath path) {
    // matches directly
    bool hasExpectedExtension = fileExtensions.contains(path.extension().string());
    bool isFile = std::filesystem::is_regular_file(path);
    if (hasExpectedExtension && isFile) {
        return path;
    }

    // if dir, scan for /init.lua and /init.luau in the dir
    if (std::filesystem::is_directory(path)) {
        path.append("init");
    }

    auto result = attemptExtensions(path);
    if (!result.has_value()) {
        throw std::runtime_error(std::format("path has multiple/no valid options: {}", pathAbsGeneric(path)));
    }
    return result.value().lexically_normal();
}

filePath moveFrom(filePath from, std::string require) {
    static std::string selfSelector = "@self/";
    if (require.starts_with(selfSelector)) {
        // doesnt seem to be an issue to use @self from any file
        require = require.erase(0, selfSelector.length());
    } else if (from.stem() == "init") {
        // compatability with https://rfcs.luau.org/abstract-module-paths-and-init-dot-luau.html
        from = from.parent_path();
    }
    auto target = from.parent_path().append(require);
    return correctPath(target);
}

// guarantees about the outputs of this function:
// - index and importId of files are the same
// - the entry file will always be index 0
// - any files that depend on another file will always be before the file they depend on
std::vector<luaFile *> flattenFileTreeDependantFirst(luaFile *entry) {
    std::unordered_map<luaFile *, int> dependenciesVisited;

    std::vector<luaFile *> files;
    std::vector<luaFile *> nextDepth{entry};
    while (!nextDepth.empty()) {
        auto currentDepth = nextDepth;
        nextDepth.clear();
        for (auto file : currentDepth) {
            // assign importId and store
            file->importId = files.size();
            files.push_back(file);

            // visit imports
            for (auto import : file->imports) {
                auto importFile = import.targetFile;
                // have we visited all of the files that depend on this import yet?
                int importVisitCount = ++dependenciesVisited[importFile];
                if (importVisitCount >= importFile->dependantCount) {
                    // we've visited all of the files that depend on this function, visit it at the next depth
                    nextDepth.emplace_back(importFile);
                    dependenciesVisited.erase(importFile);
                }
            }
        }
    }

    assert(dependenciesVisited.empty());

    return files;
}

// builds a dependancy tree from the provided file path.
// the importId fields are not filled, call flattenFileTreeDependantFirst for that.
//
// guarantees about this function:
// - the .imports field of all files will be in ascending order by location; later calls are later in the list
//   - (relevant for overriding the require calls later)
luaFile *generateFileTree(filePath entry) {
    std::unordered_map<filePath, luaFile *> pathToFile;

    // TODO: check for circular dependencies

    std::function<luaFile *(filePath)> recurse = [&](filePath path) -> luaFile * {
        auto file = new luaFile{};
        file->path = path;
        file->contents = readFile(path);

        auto requireCalls = getRequireCalls(file->contents);
        for (auto call : requireCalls) {
            // ignore require calls we dont care about, ie runtime network calls
            if (ignoreRequireTarget(call.target)) {
                continue;
            }

            // calculate the path to the import
            auto targetPath = moveFrom(path, call.target);

            // get the file of the import
            luaFile *targetFile;
            if (pathToFile.contains(targetPath)) {
                // already seen this file
                targetFile = pathToFile.at(targetPath);
            } else {
                // we havent seen this file yet,
                targetFile = recurse(targetPath);
                pathToFile[targetPath] = targetFile;
            }

            // add import data to self
            file->imports.push_back(importCall{
                .callLocation = call.location,
                .targetFile = targetFile,
            });

            // add self to dependency
            targetFile->dependantCount++;
        }

        return file;
    };

    return recurse(correctPath(entry));
}

// wrapper for generateFileTree + flattenFileTreeDependantFirst
std::vector<luaFile *> generateBundleFileList(filePath entry) {
    luaFile *fileTree = generateFileTree(entry);
    return flattenFileTreeDependantFirst(fileTree);
}
