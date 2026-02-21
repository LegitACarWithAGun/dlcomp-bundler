#include "json.hpp"
#include "utils.cpp"
#include <filesystem>
#include <format>
#include <fstream>
#include <istream>
using json = nlohmann::json;

typedef std::filesystem::path path;
struct bundleConfig {
    path entryPath;
    path outputPath;
    bool printResult;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(bundleConfig, entryPath, outputPath, printResult);

bundleConfig parseConfig(std::istream &stream) {
    json data = json::parse(stream);
    return data.get<bundleConfig>();
}

std::string generateBundleConfigFileContents(bundleConfig config) {
    json jsonObject = config;
    return jsonObject.dump(1, '\t');
}
bundleConfig generateDefaultConfig() {
    return bundleConfig{
        .entryPath = "./init",
        .outputPath = "./output.luau",
        .printResult = false,
    };
}
