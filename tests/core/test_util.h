#pragma once

#include <filesystem>
#include <optional>

std::filesystem::path make_temporary_directory(std::optional<std::string> pattern = std::nullopt);
