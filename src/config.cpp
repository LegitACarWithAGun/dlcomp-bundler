#include "json.hpp"
#include "utils.cpp"
#include <filesystem>
#include <format>
#include <fstream>
#include <istream>
using json = nlohmann::json;

typedef std::filesystem::path path;

struct bundleConfig {
	std::string name;
	path entryPath;
	path outputPath;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(bundleConfig, name, entryPath, outputPath);

struct projectConfig {
	std::vector<bundleConfig> bundles;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(projectConfig, bundles);

projectConfig parseConfig(std::istream& stream) {
	json data = json::parse(stream);
	return data.get<projectConfig>();
}

projectConfig generateDefaultConfig() {
	std::vector<bundleConfig> bundles;
	bundles.push_back(bundleConfig{
		.name = "example",
		.entryPath = "./init",
		.outputPath = "./output.luau",
	});

	return projectConfig{
		.bundles = bundles,
	};
}
