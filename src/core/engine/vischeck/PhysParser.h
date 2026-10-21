#pragma once
#include "Math.hpp"
#include <cstdint>
#include <string>
#include <vector>

class PhysParser {
public:
    explicit PhysParser(const std::string& path, bool include_hulls = false);
    const std::vector<TriangleCombined>& GetCombinedList() const { return combined; }
    const std::vector<uint32_t>& GetSurfaceHashes() const { return hashes; }

private:
    std::string DataPath;
    std::string FileText;
    std::vector<TriangleCombined> combined;
    std::vector<uint32_t> hashes;

    std::string readFile(const std::string& filename);
    std::vector<std::string> extractStructs(const std::string& block);
    int findParam(const std::string& block, const std::string& key);
    std::string extractArrayContent(const std::string& block, const std::string& key);
    std::string findNamedArray(const std::string& text, const char* key);
    std::string extractInnerBlock(const std::string& block, const std::string& key);
    std::string extractHexBlob(const std::string& block, const std::string& key);
    std::vector<unsigned char> hexStringToBytes(const std::string& hex);
    void parseVertices(const std::string& hexBlob, std::vector<Vector3>& vertices);
    void parseTriangles(const std::string& hexBlob, std::vector<Triangle>& triangles);
    void parseMaterials(const std::string& arrayContent, std::vector<int>& materials);
    std::vector<float> parseFloatArray(const std::string& arrayContent);
    std::vector<int> parseCollisionAttributes();
    void parseSurfaceHashes();
    void processMeshes();
    void processHulls();
    std::vector<TriangleCombined> trianglesFromHull(const std::string& innerHull, int materialIndex);
};
