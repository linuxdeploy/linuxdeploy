#include <iostream>

#include <linuxdeploy/plugin/plugin.h>

namespace bf = boost::filesystem;

int main(const int argc, const char* const* const argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <path to plugin>" << std::endl;
        return 1;
    }

    const std::string path = argv[1];

    std::cout << "Testing plugin: " << path << std::endl;

    auto* plugin = linuxdeploy::plugin::createPluginInstance(path);

    std::cout << "API level: " << plugin->apiLevel() << std::endl;
    std::cout << "Plugin type: " << plugin->pluginType() << " (a.k.a. " << plugin->pluginTypeString() << ")" << std::endl;

    return 0;
}
