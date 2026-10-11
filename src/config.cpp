#pragma once

#include "json.hpp"
#include <filesystem>
#include <format>
#include <fstream>
#include <istream>
using json = nlohmann::json;
typedef std::filesystem::path path;

namespace dlcomp::bundler::config {

// bundle options that can be defined from anywhere (CLI flags, project options, per-bundle options)
struct genericBundleOptions {
	std::optional<path> entryPrefix;
	std::optional<path> outputPrefix;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(genericBundleOptions, entryPrefix, outputPrefix);

// options that need to be specified for each bundle + generic options (see above)
// would keep BundleOptions and specificBundleOptions seperate, but NLOHMANN_DEFINE_DERIVED_TYPE_NON_INTRUSIVE can only take a single parent class
struct BundleOptions : genericBundleOptions {
	std::string name;
	path entry;
	path output;
};
NLOHMANN_DEFINE_DERIVED_TYPE_NON_INTRUSIVE(BundleOptions, genericBundleOptions, name, entry, output);

// config file
struct ProjectConfig {
	std::optional<genericBundleOptions> defaults;
	std::vector<BundleOptions> bundles;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ProjectConfig, defaults, bundles);

ProjectConfig ParseConfig(std::istream& stream) {
	json data = json::parse(stream, nullptr, true, true, true);
	return data.get<ProjectConfig>();
}

template <class T>
T pickEarliestOptional(std::initializer_list<T> list) {
	for (T v : list) {
		if (v)
			return v;
	}
	return std::nullopt;
}

BundleOptions CalcBundleOptions(ProjectConfig project, int bundleIndex) {
	auto bundle = project.bundles[bundleIndex];
	auto defaults = project.defaults.value_or(genericBundleOptions{});

	BundleOptions result = bundle;
	result.entryPrefix = pickEarliestOptional({bundle.entryPrefix, defaults.entryPrefix});
	result.outputPrefix = pickEarliestOptional({bundle.outputPrefix, defaults.outputPrefix});

	return result;
}

path ConcatPath(std::optional<path> pathPrefix, path path) {
	if (pathPrefix)
		return pathPrefix.value() / path;
	return path;
}
path GetEntryPath(BundleOptions bundle) {
	return ConcatPath(bundle.entryPrefix, bundle.entry);
}
path GetOutputPath(BundleOptions bundle) {
	return ConcatPath(bundle.outputPrefix, bundle.output);
}

// removes all null values from a json object and its descendants
//
// I could not figure out how to make this take any version of basic_json
// NLOHMANN_BASIC_JSON_TPL_DECLARATION seems like the solution but its intentionally cleaned up
void jsonRemoveAllNull(nlohmann::ordered_json& obj) {
	if (!obj.is_object() && !obj.is_array()) {
		return;
	}

	for (auto itr = obj.begin(); itr != obj.end();) {
		auto& val = itr.value();
		if (val.is_null()) {
			itr = obj.erase(itr);
		} else {
			jsonRemoveAllNull(val);
			itr++;
		}
	}
}

nlohmann::ordered_json GenerateDefaultConfigJson() {
	nlohmann::ordered_json project;

	BundleOptions exampleBundle{};
	exampleBundle.name = "example-bundle";
	exampleBundle.entry = "src/init";
	exampleBundle.output = "example.luau";

	project["$schema"] = "TODO";
	project["defaults"] = genericBundleOptions{
		.outputPrefix = "output",
	};
	project["bundles"] = {exampleBundle};

	jsonRemoveAllNull(project);
	return project;
}

} // namespace dlcomp::bundler::config