#pragma once

#include "Luau/Ast.h"
#include "Luau/Parser.h"
#include "utils.cpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>

static std::unordered_set<std::string> fileExtensions = {".lua", ".luau"};

typedef int importId;
struct importCall {
    Luau::Location location;
    importId targetId;
    std::filesystem::path targetPath;
};
struct luaFile {
    std::filesystem::path path;
    importId importId;
    std::string contents;
    // bundle.cpp -> fmtModule expects this to be in ascending order by location; higher locations later
    std::vector<importCall> imports;
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

std::optional<std::filesystem::path> attemptExtensions(std::filesystem::path path) {
    std::optional<std::filesystem::path> result;

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
std::filesystem::path correctPath(std::filesystem::path path) {
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
    return result.value();
}

std::filesystem::path moveFrom(std::filesystem::path from, std::string require) {
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

// guarantees about this function:
// - the entry file will always be index and importId 0, no other file has any specific requirements on location or id
// - the .imports field of all files will be in ascending order by location; later calls are later in the list
std::vector<luaFile> generateFileTree(std::filesystem::path entry) {
    std::vector<luaFile> files;
    std::unordered_map<std::filesystem::path, importId> pathToFile;

    // TODO: check for circular dependencies

    std::function<importId(std::filesystem::path)> recurse = [&](std::filesystem::path path) -> importId {
        importId selfId = files.size();

        luaFile file;
        file.path = path;
        file.contents = readFile(path);
        file.importId = selfId;
        files.push_back(file);

        pathToFile.insert_or_assign(path, selfId);

        auto requireCalls = getRequireCalls(file.contents);
        for (auto call : requireCalls) {
            // ignore require calls we dont care about, ie runtime network calls
            if (ignoreRequireTarget(call.target)) {
                continue;
            }

            // calculate the path to the import
            auto targetPath = moveFrom(path, call.target);

            // get id of the import
            importId targetId;
            if (pathToFile.contains(targetPath)) {
                targetId = pathToFile.at(targetPath);
            } else {
                // we havent seen this file yet
                targetId = recurse(targetPath);
            }

            // add import data to self
            files[selfId].imports.push_back(importCall{
                .location = call.location,
                .targetId = targetId,
                .targetPath = targetPath,
            });
        }

        return selfId;
    };

    recurse(correctPath(entry));

    return files;
}
