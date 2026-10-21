#include "PhysParser.h"
#include "SurfaceTable.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

std::string PhysParser::readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file)
        return {};
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::vector<std::string> PhysParser::extractStructs(const std::string& block)
{
    std::vector<std::string> structs;
    int depth = 0;
    size_t start = std::string::npos;
    for (size_t i = 0; i < block.size(); ++i)
    {
        if (block[i] == '{')
        {
            if (depth == 0)
                start = i;
            ++depth;
        }
        else if (block[i] == '}')
        {
            --depth;
            if (depth == 0 && start != std::string::npos)
            {
                structs.push_back(block.substr(start, i - start + 1));
                start = std::string::npos;
            }
        }
    }
    return structs;
}

int PhysParser::findParam(const std::string& block, const std::string& key)
{
    size_t pos = block.find(key);
    if (pos == std::string::npos)
        return -1;
    pos = block.find('=', pos);
    if (pos == std::string::npos)
        return -1;
    ++pos;
    while (pos < block.size() && std::isspace(static_cast<unsigned char>(block[pos])))
        ++pos;
    std::istringstream iss(block.substr(pos));
    int value = -1;
    iss >> value;
    return value;
}

std::string PhysParser::extractArrayContent(const std::string& block, const std::string& key)
{
    size_t pos = block.find(key);
    if (pos == std::string::npos)
        return {};
    pos = block.find('[', pos);
    if (pos == std::string::npos)
        return {};
    size_t end = block.find(']', pos);
    if (end == std::string::npos)
        return {};
    return block.substr(pos + 1, end - pos - 1);
}

std::string PhysParser::findNamedArray(const std::string& text, const char* key)
{
    size_t pos = text.find(key);
    if (pos == std::string::npos)
        return {};
    pos = text.find('[', pos);
    if (pos == std::string::npos)
        return {};
    int depth = 1;
    size_t start = pos + 1;
    size_t i = start;
    while (i < text.size() && depth > 0)
    {
        if (text[i] == '[')
            ++depth;
        else if (text[i] == ']')
            --depth;
        ++i;
    }
    return text.substr(start, i - start - 1);
}

std::string PhysParser::extractInnerBlock(const std::string& block, const std::string& key)
{
    size_t pos = block.find(key);
    if (pos == std::string::npos)
        return {};
    pos = block.find('{', pos);
    if (pos == std::string::npos)
        return {};
    int depth = 0;
    size_t start = pos;
    size_t i = pos;
    for (; i < block.size(); ++i)
    {
        if (block[i] == '{')
            ++depth;
        else if (block[i] == '}')
            --depth;
        if (depth == 0)
            break;
    }
    return block.substr(start, i - start + 1);
}

std::string PhysParser::extractHexBlob(const std::string& block, const std::string& key)
{
    size_t pos = block.find(key);
    if (pos == std::string::npos)
        return {};
    pos = block.find("#[", pos);
    if (pos == std::string::npos)
        return {};
    pos += 2;
    size_t end = block.find(']', pos);
    if (end == std::string::npos)
        return {};
    std::string hexStr = block.substr(pos, end - pos);
    hexStr.erase(std::remove_if(hexStr.begin(), hexStr.end(),
        [](unsigned char c) { return std::isspace(c) != 0; }), hexStr.end());
    return hexStr;
}

std::vector<unsigned char> PhysParser::hexStringToBytes(const std::string& hex)
{
    std::vector<unsigned char> bytes;
    if (hex.size() % 2 != 0)
        return bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2)
    {
        unsigned int byte = 0;
        std::istringstream iss(hex.substr(i, 2));
        iss >> std::hex >> byte;
        bytes.push_back(static_cast<unsigned char>(byte));
    }
    return bytes;
}

void PhysParser::parseVertices(const std::string& hexBlob, std::vector<Vector3>& vertices)
{
    auto bytes = hexStringToBytes(hexBlob);
    const size_t numVerts = bytes.size() / 12;
    vertices.resize(numVerts);
    for (size_t i = 0; i < numVerts; ++i)
    {
        float v[3]{};
        std::memcpy(v, &bytes[i * 12], 12);
        vertices[i] = { v[0], v[1], v[2] };
    }
}

void PhysParser::parseTriangles(const std::string& hexBlob, std::vector<Triangle>& triangles)
{
    auto bytes = hexStringToBytes(hexBlob);
    const size_t numTris = bytes.size() / 12;
    triangles.resize(numTris);
    for (size_t i = 0; i < numTris; ++i)
    {
        int idx[3]{};
        std::memcpy(idx, &bytes[i * 12], 12);
        triangles[i] = { idx[0], idx[1], idx[2] };
    }
}

void PhysParser::parseMaterials(const std::string& arrayContent, std::vector<int>& materials)
{
    materials.clear();
    std::istringstream iss(arrayContent);
    int num = 0;
    while (iss >> num)
    {
        materials.push_back(num);
        if (iss.peek() == ',')
            iss.ignore();
    }
}

std::vector<float> PhysParser::parseFloatArray(const std::string& arrayContent)
{
    std::vector<float> values;
    std::istringstream iss(arrayContent);
    float num = 0.f;
    while (iss >> num)
    {
        values.push_back(num);
        if (iss.peek() == ',' || std::isspace(static_cast<unsigned char>(iss.peek())))
            iss.ignore();
    }
    return values;
}

void PhysParser::parseSurfaceHashes()
{
    hashes.clear();
    const std::string arr = extractArrayContent(FileText, "m_surfacePropertyHashes");
    if (arr.empty())
        return;
    std::istringstream iss(arr);
    unsigned long long n = 0;
    while (iss >> n)
    {
        hashes.push_back(static_cast<uint32_t>(n));
        if (iss.peek() == ',')
            iss.ignore();
    }
}

std::vector<int> PhysParser::parseCollisionAttributes()
{
    std::vector<int> keep;
    if (FileText.empty())
        return keep;

    const std::string collisionBlock = findNamedArray(FileText, "m_collisionAttributes");
    if (collisionBlock.empty())
        return keep;

    auto collisionStructs = extractStructs(collisionBlock);
    for (size_t idx = 0; idx < collisionStructs.size(); ++idx)
    {
        const auto& attr = collisionStructs[idx];
        std::string interact = extractArrayContent(attr, "m_InteractAsStrings");
        for (char& c : interact)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (interact.find("passbullets") != std::string::npos ||
            interact.find("playerclip") != std::string::npos ||
            interact.find("npcclip") != std::string::npos ||
            interact.find("grenadeclip") != std::string::npos ||
            interact.find("sky") != std::string::npos)
            continue;

        size_t strPos = attr.find("m_CollisionGroupString");
        if (strPos == std::string::npos)
            continue;
        size_t eqPos = attr.find('=', strPos);
        if (eqPos == std::string::npos)
            continue;
        ++eqPos;
        while (eqPos < attr.size() &&
            (std::isspace(static_cast<unsigned char>(attr[eqPos])) || attr[eqPos] == '"'))
            ++eqPos;
        size_t endPos = attr.find('"', eqPos);
        if (endPos == std::string::npos)
            continue;
        std::string groupStr = attr.substr(eqPos, endPos - eqPos);
        std::transform(groupStr.begin(), groupStr.end(), groupStr.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (groupStr == "default")
            keep.push_back(static_cast<int>(idx));
    }
    return keep;
}

void PhysParser::processMeshes()
{
    if (FileText.empty())
        return;
    const std::string meshesBlock = findNamedArray(FileText, "m_meshes");
    if (meshesBlock.empty())
        return;

    auto meshStructs = extractStructs(meshesBlock);
    const std::vector<int> defaultCollisionIndices = parseCollisionAttributes();
    SurfaceTable::Get().EnsureLoaded();

    auto skip_surface = [](const SurfacePen& s) {
        std::string n = s.name;
        for (char& c : n)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return n == "player_control_clip" || n == "default_silent" ||
            n == "no_decal" || n == "audioblocker" || n.find("clip") != std::string::npos;
    };

    for (const auto& meshStruct : meshStructs)
    {
        const int collisionAttribute = findParam(meshStruct, "m_nCollisionAttributeIndex");
        if (!defaultCollisionIndices.empty() &&
            std::find(defaultCollisionIndices.begin(), defaultCollisionIndices.end(), collisionAttribute)
                == defaultCollisionIndices.end())
            continue;

        const int surfaceProperty = findParam(meshStruct, "m_nSurfacePropertyIndex");
        const std::string innerMesh = extractInnerBlock(meshStruct, "m_Mesh");
        if (innerMesh.empty())
            continue;

        const std::string verticesHex = extractHexBlob(innerMesh, "m_Vertices");
        const std::string trianglesHex = extractHexBlob(innerMesh, "m_Triangles");
        const std::string materialsContent = extractArrayContent(innerMesh, "m_Materials");
        if (verticesHex.empty() || trianglesHex.empty())
            continue;

        std::vector<Vector3> vertices;
        std::vector<Triangle> triangles;
        std::vector<int> materials;
        parseVertices(verticesHex, vertices);
        parseTriangles(trianglesHex, triangles);
        const bool hasMaterials = !materialsContent.empty();
        if (hasMaterials)
            parseMaterials(materialsContent, materials);

        for (size_t i = 0; i < triangles.size(); ++i)
        {
            const Triangle& t = triangles[i];
            if (static_cast<size_t>(t.a) >= vertices.size() ||
                static_cast<size_t>(t.b) >= vertices.size() ||
                static_cast<size_t>(t.c) >= vertices.size())
                continue;
            const int mat = hasMaterials
                ? (i < materials.size() ? materials[i] : 15)
                : surfaceProperty;
            if (skip_surface(SurfaceTable::Get().Resolve(mat, hashes)))
                continue;
            TriangleCombined tc;
            tc.v0 = vertices[t.a];
            tc.v1 = vertices[t.b];
            tc.v2 = vertices[t.c];
            tc.materialIndex = mat;
            combined.push_back(tc);
        }
    }
}

std::vector<TriangleCombined> PhysParser::trianglesFromHull(
    const std::string& innerHull, int materialIndex)
{
    std::vector<TriangleCombined> tris;
    const std::string posHex = extractHexBlob(innerHull, "m_VertexPositions");
    const std::string edgeHex = extractHexBlob(innerHull, "m_Edges");
    const std::string faceHex = extractHexBlob(innerHull, "m_Faces");
    if (posHex.empty() || edgeHex.empty() || faceHex.empty())
        return tris;

    std::vector<Vector3> verts;
    parseVertices(posHex, verts);
    if (verts.empty())
        return tris;

    const auto edgeBytes = hexStringToBytes(edgeHex);
    const auto faceBytes = hexStringToBytes(faceHex);
    if (edgeBytes.size() < 4 || faceBytes.empty())
        return tris;

    const size_t nedges = edgeBytes.size() / 4;
    for (size_t f = 0; f < faceBytes.size(); ++f)
    {
        size_t e = faceBytes[f];
        if (e >= nedges)
            continue;

        int origin[16]{};
        int n = 0;
        const size_t start = e;
        for (int guard = 0; guard < 16; ++guard)
        {
            if (e >= nedges)
                break;
            origin[n++] = edgeBytes[e * 4 + 2];
            e = edgeBytes[e * 4];
            if (e == start)
                break;
        }
        if (n < 3)
            continue;
        for (int i = 1; i + 1 < n; ++i)
        {
            const int a = origin[0];
            const int b = origin[i];
            const int c = origin[i + 1];
            if (a < 0 || b < 0 || c < 0)
                continue;
            if (static_cast<size_t>(a) >= verts.size() ||
                static_cast<size_t>(b) >= verts.size() ||
                static_cast<size_t>(c) >= verts.size())
                continue;
            TriangleCombined tc;
            tc.v0 = verts[a];
            tc.v1 = verts[b];
            tc.v2 = verts[c];
            tc.materialIndex = materialIndex;
            tris.push_back(tc);
        }
    }
    return tris;
}

void PhysParser::processHulls()
{
    if (FileText.empty())
        return;
    const std::string hullsBlock = findNamedArray(FileText, "m_hulls");
    if (hullsBlock.empty())
        return;

    const std::vector<int> keepCollision = parseCollisionAttributes();
    SurfaceTable::Get().EnsureLoaded();

    auto skip_surface = [](const SurfacePen& s) {
        std::string n = s.name;
        for (char& c : n)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return n == "player_control_clip" || n == "default_silent" ||
            n == "no_decal" || n == "audioblocker" || n.find("clip") != std::string::npos;
    };

    for (const auto& hullStruct : extractStructs(hullsBlock))
    {
        const int collisionAttribute = findParam(hullStruct, "m_nCollisionAttributeIndex");
        if (!keepCollision.empty() &&
            std::find(keepCollision.begin(), keepCollision.end(), collisionAttribute)
                == keepCollision.end())
            continue;

        const int surfacePropertyIndex = findParam(hullStruct, "m_nSurfacePropertyIndex");
        if (skip_surface(SurfaceTable::Get().Resolve(surfacePropertyIndex, hashes)))
            continue;

        const std::string innerHull = extractInnerBlock(hullStruct, "m_Hull");
        if (innerHull.empty())
            continue;
        auto hullTriangles = trianglesFromHull(innerHull, surfacePropertyIndex);
        combined.insert(combined.end(), hullTriangles.begin(), hullTriangles.end());
    }
}

PhysParser::PhysParser(const std::string& path, bool include_hulls) : DataPath(path)
{
    FileText = readFile(DataPath);
    parseSurfaceHashes();
    processMeshes();
    if (include_hulls)
        processHulls();
    FileText.clear();
}
