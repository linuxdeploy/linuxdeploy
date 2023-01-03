// system headers
#include <filesystem>
#include <iostream>

// local headers
#include <linuxdeploy/plugin/plugin.h>

namespace fs = std::filesystem;

int main(const int argc, const char* const* const argv) {
    auto plugins = linuxdeploy::plugin::findPlugins();

    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            const fs::path path = argv[1];
            auto* plugin = linuxdeploy::plugin::createPluginInstance(path);
            plugins[path.filename().string()] = plugin;
        }
    }

    std::vector<std::pair<std::string, linuxdeploy::plugin::IPlugin*>> pluginList;
    for (const auto& plugin : plugins)
        pluginList.push_back(plugin);

    for (const auto& plugin : pluginList) {
        std::cout << "Testing plugin '" << plugin.first << "': " << plugin.second->path().string() << std::endl;

        std::cout << "API level: " << plugin.second->apiLevel() << std::endl;
        std::cout << "Plugin type: " << plugin.second->pluginType() << " (a.k.a. " << plugin.second->pluginTypeString() << ")"
                  << std::endl;

        if (plugin != pluginList.back())
            std::cout << std::endl;
    }

    return 0;
}
