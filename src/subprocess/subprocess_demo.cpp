#include <iostream>

#include "linuxdeploy/subprocess/subprocess.h"

using namespace linuxdeploy::subprocess;

int main(int argc, char** argv) {
    subprocess proc({"cat", "/proc/cpuinfo"});

    auto result = proc.run();

    std::cout << "result: " << result.exit_code() << std::endl;

    std::cout << "stdout contents: " << result.stdout_string() << std::endl;
    std::cout << "stderr contents: " << result.stderr_string() << std::endl;
}
