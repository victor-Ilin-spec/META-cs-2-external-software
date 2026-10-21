#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "Math.hpp"

class OptimizedGeometry {
public:
    std::vector<std::vector<TriangleCombined>> meshes;
    std::vector<uint32_t> surface_hashes;
    bool LoadFromFile(const std::string& optimizedFile);
    bool LoadFromPhys(const std::string& physFile, bool include_hulls = false);
    void Clear();
};
