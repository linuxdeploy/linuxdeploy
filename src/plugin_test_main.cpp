#include <iostream>

#include <linuxdeploy/plugin/plugin.h>

namespace bf = boost::filesystem;

int main(const int argc, const char* const* const argv) {
    auto plugins = linuxdeploy::plugin::findPlugins();

    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            const std::string path = argv[1];
            auto* plugin = linuxdeploy::plugin::createPluginInstance(path);
            plugins.push_back(plugin);
        }
    }

    for (const auto& plugin : plugins) {
        std::cout << "Testing plugin: " << plugin->path().string() << std::endl;

        std::cout << "API level: " << plugin->apiLevel() << std::endl;
        std::cout << "Plugin type: " << plugin->pluginType() << " (a.k.a. " << plugin->pluginTypeString() << ")"
                  << std::endl;

        if (plugin != plugins.back())
            std::cout << std::endl;
    }

    return 0;
}
