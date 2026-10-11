#include "bundle.cpp"
#include "config.cpp"
#include "fileTree.cpp"
#include "minify.cpp"
#include "utils.cpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <istream>
#include <vector>
namespace config = dlcomp::bundler::config;

std::filesystem::path defaultBundleConfigPath = "./bundler.json";

void handleBundle(const config::BundleOptions& bundleOptions) {
	std::cout << "BUNDLING " << bundleOptions.name << std::endl;

	// generate file/dependancy tree
	auto entryPath = config::GetEntryPath(bundleOptions);
	std::cout << "- entering from " << pathAbsGeneric(entryPath) << std::endl;
	std::vector<luaFile*> files = generateBundleFileList(entryPath);

	// bundle all the files together
	std::string bundle = bundleFileTree(files, bundleOptions);

	// write to file
	auto outputPath = config::GetOutputPath(bundleOptions);
	std::cout << "- writing bundle to " << pathAbsGeneric(outputPath) << std::endl;
	writeFile(outputPath, bundle);
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
		std::cerr << config::GenerateDefaultConfigJson().dump(1, '\t') << std::endl;
		return 1;
	}
	config::ProjectConfig projectConfig = config::ParseConfig(configFile);
	std::cout << std::endl;

	auto bundles = projectConfig.bundles;
	for (int i = 0; i < bundles.size(); i++) {
		auto bundleOptions = config::CalcBundleOptions(projectConfig, i);
		handleBundle(bundleOptions);
	}

	std::cout << "success" << std::endl;
	return 0;
}
