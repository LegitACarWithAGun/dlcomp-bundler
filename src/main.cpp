#include "bundle.cpp"
#include "config.cpp"
#include "fileTree.cpp"
#include "json.hpp"
#include "minify.cpp"
#include "utils.cpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <istream>
#include <vector>
using json = nlohmann::json;

std::filesystem::path defaultBundleConfigPath = "./bundler.json";

void handleBundle(projectConfig& projectConfig, bundleConfig& bundleConfig) {
	std::cout << "BUNDLING " << bundleConfig.name << std::endl;

	// generate file/dependancy tree
	std::cout << "- reading from " << pathAbsGeneric(bundleConfig.entryPath) << std::endl;
	std::vector<luaFile*> files = generateBundleFileList(bundleConfig.entryPath);

	// format and output file tree
	std::string bundle = bundleFileTree(files);
	bundle = minify(bundle);
	std::filesystem::path outputPath = bundleConfig.outputPath;
	writeFile(outputPath, bundle);
	std::cout << "- wrote results to " << pathAbsGeneric(outputPath) << std::endl;
	std::cout << std::endl;
}

int main() {
	// read config
	std::cout << "READING CONFIG" << std::endl;
	std::filesystem::path configPath = defaultBundleConfigPath;
	std::ifstream configFile(configPath);
	if (!configFile) {
		std::cerr << "!! no config file found at " << pathAbsGeneric(configPath) << std::endl;
		std::cerr << "example config file contents:" << std::endl;

		json defaultConfigJSON;
		to_json(defaultConfigJSON, generateDefaultConfig());
		std::cerr << defaultConfigJSON.dump(1, '\t') << std::endl;
		return 1;
	}
	projectConfig projectConfig = parseConfig(configFile);
	std::cout << std::endl;

	for (auto bundleConfig : projectConfig.bundles) {
		handleBundle(projectConfig, bundleConfig);
	}

	std::cout << "success" << std::endl;
	return 0;
}
