#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <variant>

// Lecteur minimal du format PARAM.SFO (métadonnées d'un jeu PS4 : titre, ID, version...).
class Sfo {
public:
    bool Load(const std::filesystem::path& path);

    std::optional<std::string> GetString(const std::string& key) const;
    std::optional<std::uint32_t> GetInt(const std::string& key) const;

private:
    std::map<std::string, std::variant<std::string, std::uint32_t>> entries_;
};
