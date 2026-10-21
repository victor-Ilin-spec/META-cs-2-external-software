#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct SurfacePen {
    std::string name = "default";
    float pen_mod = 0.5f;
    float dmg_mod = 0.5f;
};

class SurfaceTable {
public:
    static SurfaceTable& Get();

    void EnsureLoaded();
    SurfacePen Resolve(int material_index, const std::vector<uint32_t>& hashes) const;
    SurfacePen ByName(const std::string& name) const;

private:
    bool loaded = false;
    std::unordered_map<uint32_t, std::string> hash_to_name;
    std::unordered_map<std::string, SurfacePen> by_name;

    static std::string FindFile(const char* filename);
    void LoadHashNames(const std::string& path);
    void LoadGameMods(const std::string& path);
};
