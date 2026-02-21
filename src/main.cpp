#include "bundle.cpp"
#include "config.cpp"
#include "fileTree.cpp"
#include "utils.cpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <istream>
#include <vector>

std::filesystem::path defaultBundleConfigPath = "./deadline-bundler.json";

int main() {
    std::filesystem::path configPath = defaultBundleConfigPath;
    std::ifstream configFile(configPath);
    if (!configFile) {
        std::cerr << "no config file found at " << pathAbsGeneric(configPath) << std::endl;
        std::cerr << "example config file contents:" << std::endl;
        std::cerr << generateBundleConfigFileContents(generateDefaultConfig()) << std::endl;
        return 1;
    }
    bundleConfig config = parseConfig(configFile);

    std::cout << "reading from " << pathAbsGeneric(config.entryPath) << std::endl;
    std::vector<luaFile> files = generateFileTree(config.entryPath);

    std::string bundle = bundleFileTree(files);
    std::filesystem::path outputPath = config.outputPath;
    writeFile(outputPath, bundle);
    std::cout << "\n";
    if (config.printResult) {
        std::cout << "results: " << std::endl;
        std::cout << bundle << "\n" << std::endl;
    }
    std::cout << "wrote results to " << pathAbsGeneric(outputPath) << std::endl;

    return 0;
}
