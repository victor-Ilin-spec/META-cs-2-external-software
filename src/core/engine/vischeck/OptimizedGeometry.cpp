#include "OptimizedGeometry.h"
#include "PhysParser.h"

#include <cstring>
#include <fstream>
#include <vector>

void OptimizedGeometry::Clear()
{
    meshes.clear();
    surface_hashes.clear();
}

bool OptimizedGeometry::LoadFromFile(const std::string& optimizedFile)
{
    std::ifstream in(optimizedFile, std::ios::binary | std::ios::ate);
    if (!in)
        return false;

    const auto sz = in.tellg();
    if (sz <= 0)
        return false;
    in.seekg(0, std::ios::beg);

    std::vector<char> data(static_cast<size_t>(sz));
    if (!in.read(data.data(), sz))
        return false;
    in.close();

    Clear();

    const char* p = data.data();
    const char* const end = p + data.size();
    auto take = [&](size_t n) -> const char* {
        if (n > static_cast<size_t>(end - p))
            return nullptr;
        const char* out = p;
        p += n;
        return out;
    };

    const char* nmesh = take(sizeof(size_t));
    if (!nmesh)
        return false;
    size_t numMeshes = 0;
    std::memcpy(&numMeshes, nmesh, sizeof(size_t));
    if (numMeshes == 0 || numMeshes > 4096)
        return false;

    constexpr size_t kLegacyTriBytes = sizeof(Vector3) * 3;
    meshes.reserve(numMeshes);
    for (size_t i = 0; i < numMeshes; ++i)
    {
        const char* ntri = take(sizeof(size_t));
        if (!ntri)
        {
            Clear();
            return false;
        }
        size_t numTris = 0;
        std::memcpy(&numTris, ntri, sizeof(size_t));
        if (numTris > 5'000'000)
        {
            Clear();
            return false;
        }

        const char* packed = take(numTris * kLegacyTriBytes);
        if (!packed)
        {
            Clear();
            return false;
        }

        std::vector<TriangleCombined> mesh(numTris);
        for (size_t t = 0; t < numTris; ++t)
        {
            std::memcpy(&mesh[t].v0, packed + t * kLegacyTriBytes, kLegacyTriBytes);
            mesh[t].materialIndex = -1;
        }
        meshes.push_back(std::move(mesh));
    }
    return true;
}

bool OptimizedGeometry::LoadFromPhys(const std::string& physFile, bool include_hulls)
{
    Clear();
    PhysParser parser(physFile, include_hulls);
    if (parser.GetCombinedList().empty())
        return false;
    meshes.push_back(parser.GetCombinedList());
    surface_hashes = parser.GetSurfaceHashes();
    return true;
}
