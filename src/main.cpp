#include "bundle.cpp"
#include "fileTree.cpp"
#include "utils.cpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

int main() {
    std::vector<luaFile> files = generateFileTree("./sample2.lua");

    std::string bundle = bundleFileTree(files);
    std::filesystem::path outputPath = "./output.lua";
    writeFile(outputPath, bundle);
    std::cout << "wrote results to " << pathAbsGeneric(outputPath) << std::endl;

    return 0;
}
