#include "SurfaceTable.hpp"
#include "common.hpp"

#include <Windows.h>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
    std::string Lower(std::string s)
    {
        for (char& c : s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    std::vector<std::string> ExtractListEntries(const std::string& text, const char* list_key)
    {
        const size_t key = text.find(list_key);
        if (key == std::string::npos)
            return {};
        size_t pos = text.find('[', key);
        if (pos == std::string::npos)
            return {};
        int depth = 1;
        const size_t start = pos + 1;
        size_t i = start;
        while (i < text.size() && depth > 0)
        {
            if (text[i] == '[')
                ++depth;
            else if (text[i] == ']')
                --depth;
            ++i;
        }
        const std::string list = text.substr(start, i - start - 1);

        std::vector<std::string> out;
        int d = 0;
        size_t s = std::string::npos;
        for (size_t k = 0; k < list.size(); ++k)
        {
            if (list[k] == '{')
            {
                if (d == 0)
                    s = k;
                ++d;
            }
            else if (list[k] == '}')
            {
                --d;
                if (d == 0 && s != std::string::npos)
                {
                    out.push_back(list.substr(s, k - s + 1));
                    s = std::string::npos;
                }
            }
        }
        return out;
    }

    std::string Quoted(const std::string& block, const char* key)
    {
        size_t pos = block.find(key);
        if (pos == std::string::npos)
            return {};
        pos = block.find('"', pos + std::strlen(key));
        if (pos == std::string::npos)
            return {};
        const size_t end = block.find('"', pos + 1);
        if (end == std::string::npos)
            return {};
        return block.substr(pos + 1, end - pos - 1);
    }

    bool NumberAfter(const std::string& block, const char* key, double& out)
    {
        size_t pos = block.find(key);
        if (pos == std::string::npos)
            return false;
        pos = block.find('=', pos);
        if (pos == std::string::npos)
            return false;
        ++pos;
        while (pos < block.size() && std::isspace(static_cast<unsigned char>(block[pos])))
            ++pos;
        try
        {
            out = std::stod(block.substr(pos));
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}

SurfaceTable& SurfaceTable::Get()
{
    static SurfaceTable t;
    return t;
}

std::string SurfaceTable::FindFile(const char* filename)
{
    char exe[MAX_PATH]{};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    const std::filesystem::path exe_dir = std::filesystem::path(exe).parent_path();
    const std::filesystem::path cwd = std::filesystem::current_path();
    const std::filesystem::path candidates[] = {
        exe_dir / "maps" / "surfaces" / filename,
        cwd / "maps" / "surfaces" / filename,
        exe_dir.parent_path().parent_path() / "maps" / "surfaces" / filename,
        cwd.parent_path() / "maps" / "surfaces" / filename,
        exe_dir / filename,
        cwd / filename,
    };
    std::error_code ec;
    for (const auto& p : candidates)
    {
        if (std::filesystem::exists(p, ec) && !ec)
            return p.string();
    }
    return {};
}

void SurfaceTable::LoadHashNames(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return;
    std::stringstream ss;
    ss << in.rdbuf();
    for (const auto& block : ExtractListEntries(ss.str(), "SurfacePropertiesList"))
    {
        const std::string name = Quoted(block, "surfacePropertyName");
        double hash = 0;
        if (name.empty() || !NumberAfter(block, "m_nameHash", hash))
            continue;
        hash_to_name[static_cast<uint32_t>(hash)] = name;
    }
}

void SurfaceTable::LoadGameMods(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return;
    std::stringstream ss;
    ss << in.rdbuf();
    std::unordered_map<std::string, std::string> base_of;
    for (const auto& block : ExtractListEntries(ss.str(), "SurfacePropertiesList"))
    {
        const std::string name = Quoted(block, "surfacePropertyName");
        if (name.empty())
            continue;
        SurfacePen pen;
        pen.name = name;
        double d = 0;
        if (NumberAfter(block, "bulletPenetrationDistanceModifier", d))
            pen.pen_mod = static_cast<float>(d);
        else
            pen.pen_mod = -1.f;
        if (NumberAfter(block, "bulletPenetrationDamageModifier", d))
            pen.dmg_mod = static_cast<float>(d);
        else
            pen.dmg_mod = -1.f;
        const std::string base = Quoted(block, "base");
        if (!base.empty())
            base_of[Lower(name)] = base;
        by_name[Lower(name)] = pen;
    }
    for (auto& [key, pen] : by_name)
    {
        std::string walk = key;
        int guard = 0;
        while (guard++ < 8 && (pen.pen_mod < 0.f || pen.dmg_mod < 0.f))
        {
            auto bit = base_of.find(walk);
            if (bit == base_of.end())
                break;
            auto pit = by_name.find(Lower(bit->second));
            if (pit == by_name.end())
                break;
            if (pen.pen_mod < 0.f)
                pen.pen_mod = pit->second.pen_mod;
            if (pen.dmg_mod < 0.f)
                pen.dmg_mod = pit->second.dmg_mod;
            walk = Lower(bit->second);
        }
        if (pen.pen_mod < 0.f)
            pen.pen_mod = 0.5f;
        if (pen.dmg_mod < 0.f)
            pen.dmg_mod = 0.5f;
    }
}

void SurfaceTable::EnsureLoaded()
{
    if (loaded)
        return;
    loaded = true;
    const auto hashes = FindFile("surfaceproperties.txt");
    const auto game = FindFile("surfaceproperties_game.txt");
    if (!hashes.empty())
        LoadHashNames(hashes);
    if (!game.empty())
        LoadGameMods(game);
    if (by_name.find("default") == by_name.end())
        by_name["default"] = { "default", 0.5f, 0.5f };
    LOGF(INFO, "SurfaceTable: {} hashes, {} game surfaces ({})",
        hash_to_name.size(), by_name.size(),
        game.empty() ? "fallback default 0.5" : game);
}

SurfacePen SurfaceTable::ByName(const std::string& name) const
{
    auto it = by_name.find(Lower(name));
    if (it != by_name.end())
        return it->second;
    it = by_name.find("default");
    if (it != by_name.end())
        return it->second;
    return {};
}

SurfacePen SurfaceTable::Resolve(int material_index, const std::vector<uint32_t>& hashes) const
{
    if (material_index >= 0 && static_cast<size_t>(material_index) < hashes.size())
    {
        auto hit = hash_to_name.find(hashes[static_cast<size_t>(material_index)]);
        if (hit != hash_to_name.end())
            return ByName(hit->second);
    }
    return ByName("default");
}
