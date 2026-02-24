#include "fileTree.cpp"
#include "utils.cpp"
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <unordered_set>

typedef std::filesystem::path path;

typedef std::unordered_set<path> paths;

path luaTreeRoot = "./test/lua-tree/";

// nullopt is error
std::vector<std::tuple<std::string, std::optional<std::string>>> expectedCorrectPath = {
    {"./", std::nullopt},
    {"./empty", "./empty.lua"},
    {"./dep", "./dep/init.lua"},
    {"./dep/init", "./dep/init.lua"},
    {"./depMultiple", std::nullopt},
    {"./depMultiple/init", std::nullopt},
    {"./depSelf", "./depSelf/init.luau"},
    {"./depSelf/init", "./depSelf/init.luau"},
};
std::vector<std::tuple<std::string, std::string, std::optional<std::string>>> expectedMoveFrom = {
    {"./fictional.lua", "./dep", "./dep/init.lua"},
    {"./fictional.lua", "./dep/sub", "./dep/sub.lua"},
    {"./fictional.lua", "./depMultiple", std::nullopt},
    {"./depParent/entry.lua", "../dep/sub", "./dep/sub.lua"},
    {"./fictional.lua", "@self/dep", "./dep/init.lua"},
    {"./depSelf/init.luau", "@self/sub", "./depSelf/sub.lua"},
    {"./depSelf/init.luau", "./depSelf/sub", "./depSelf/sub.lua"},
    {"./depSelf/init.luau", "./sub", std::nullopt},
};

std::string fmtOptional(std::optional<path> optional, std::function<std::string(path)> fmtValue) {
    if (!optional.has_value()) {
        return "nullopt";
    }

    return std::format("{}", fmtValue(optional.value()));
}

bool areOptionalPathsEquivalent(std::optional<path> a, std::optional<path> b) {
    // only one is nullopt
    if (a.has_value() != b.has_value()) {
        return false;
    }
    // both are nullopt
    if (!a.has_value() && !b.has_value()) {
        return true;
    }

    // both have values
    std::error_code error;
    return std::filesystem::equivalent(a.value(), b.value(), error);
}

path pathFrom(path from, std::string relative) {
    return from.append(relative);
}
std::optional<path> pathFrom(path from, std::optional<std::string> relative) {
    if (!relative.has_value()) {
        return std::nullopt;
    }
    return pathFrom(from, relative.value());
}

void testRequireCorrectPath() {
    auto caseList = expectedCorrectPath;
    int caseCount = caseList.size();
    int correct = 0;

    std::cout << "\n" << "correct path:" << std::endl;
    for (auto testCase : caseList) {
        path input = pathFrom(luaTreeRoot, std::get<0>(testCase));
        std::optional<path> wantOutput = pathFrom(luaTreeRoot, std::get<1>(testCase));

        // gather result for case, nullopt is error
        std::optional<path> result;
        try {
            result = correctPath(input);
        } catch (const std::exception &err) {
        }

        if (!areOptionalPathsEquivalent(wantOutput, result)) {
            std::cout << "CASE FAILED:" << std::endl;
            std::cout << std::format("\t- input   : {}", pathAbsGeneric(input)) << std::endl;
            std::cout << std::format("\t- expected: {}", fmtOptional(wantOutput, pathAbsGeneric)) << std::endl;
            std::cout << std::format("\t- got     : {}", fmtOptional(result, pathAbsGeneric)) << std::endl;
        } else {
            correct++;
        }
    }
    std::cout << std::format("{}/{} correct", correct, caseCount) << std::endl;
}

void testRequireMoveFrom() {
    auto caseList = expectedMoveFrom;
    int caseCount = caseList.size();
    int correct = 0;

    std::cout << "\n" << "move from:" << std::endl;
    for (auto testCase : caseList) {
        path from = pathFrom(luaTreeRoot, std::get<0>(testCase));
        std::string require = std::get<1>(testCase);
        std::optional<path> wantOutput = pathFrom(luaTreeRoot, std::get<2>(testCase));

        // gather result for case, nullopt is error
        std::optional<path> result;
        try {
            result = moveFrom(from, require);
        } catch (const std::exception &err) {
        }

        if (!areOptionalPathsEquivalent(wantOutput, result)) {
            std::cout << "CASE FAILED:" << std::endl;
            std::cout << std::format("\t- from    : {}", pathAbsGeneric(from)) << std::endl;
            std::cout << std::format("\t- require : {}", require) << std::endl;
            std::cout << std::format("\t- expected: {}", fmtOptional(wantOutput, pathAbsGeneric)) << std::endl;
            std::cout << std::format("\t- got     : {}", fmtOptional(result, pathAbsGeneric)) << std::endl;
        } else {
            correct++;
        }
    }
    std::cout << std::format("{}/{} correct", correct, caseCount) << std::endl;
}