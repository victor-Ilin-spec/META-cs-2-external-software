#include "MapVisibility.hpp"
#include "common.hpp"

#include <Windows.h>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <algorithm>

MapVisibility& MapVisibility::Get()
{
    static MapVisibility instance;
    return instance;
}

std::filesystem::path MapVisibility::MapsDir()
{
    char exe[MAX_PATH]{};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    return std::filesystem::path(exe).parent_path() / "maps";
}

void MapVisibility::EnsureMapsDir()
{
    std::error_code ec;
    std::filesystem::create_directories(MapsDir(), ec);
    if (ec)
        LOGF(WARNING, "VisCheck: could not create {}", MapsDir().string());
    else
        LOGF(INFO, "VisCheck: maps folder {}", MapsDir().string());
}


MapVisibility::~MapVisibility()
{
    running.store(false, std::memory_order_release);
    if (worker.joinable())
        worker.join();
}

std::string MapVisibility::NormalizeMapName(const char* raw)
{
    if (!raw || !raw[0])
        return {};

    std::string name = raw;
    const auto slash = name.find_last_of("\\/");
    if (slash != std::string::npos)
        name = name.substr(slash + 1);

    const auto dot = name.find('.');
    if (dot != std::string::npos)
        name.resize(dot);

    for (char& c : name)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (name.empty() || name == "<empty>" || name == "empty" ||
        name == "lobby_mapveto" || name == "<uninitialized>" ||
        name == "error_global_client" || name.find("error_") == 0)
        return {};

    return name;
}

std::string MapVisibility::FindNamed(const std::string& map, const char* const* names, size_t count)
{
    const std::filesystem::path exe_maps = MapsDir();
    const std::filesystem::path cwd = std::filesystem::current_path();
    const std::filesystem::path roots[] = {
        exe_maps,
        cwd / "maps",
    };


    std::error_code ec;
    char buf[MAX_PATH]{};
    for (const auto& root : roots)
    {
        for (size_t i = 0; i < count; ++i)
        {
            std::snprintf(buf, sizeof(buf), names[i], map.c_str());
            const auto path = root / buf;
            if (std::filesystem::exists(path, ec) && !ec)
                return path.string();
        }
    }
    return {};
}

std::string MapVisibility::FindOptPath(const std::string& map)
{
    static const char* names[] = { "%s.opt" };
    return FindNamed(map, names, 1);
}

std::string MapVisibility::FindPhysPath(const std::string& map)
{
    static const char* names[] = { "phys/%s.vphys", "phys/%s.phys", "%s.vphys", "%s.phys" };
    return FindNamed(map, names, 4);
}

void MapVisibility::RequestLoad(const std::string& map, const std::string& vis_path, const std::string& pen_path)
{
    std::lock_guard<std::mutex> lock(mtx);
    requested_map = map;
    requested_vis = vis_path;
    requested_pen = pen_path;
    work_pending = true;
    if (!worker_started.exchange(true))
        worker = std::thread(&MapVisibility::WorkerLoop, this);
}

void MapVisibility::WorkerLoop()
{
    while (running.load(std::memory_order_acquire))
    {
        std::string map;
        std::string vis_path;
        std::string pen_path;
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (work_pending)
            {
                map = requested_map;
                vis_path = requested_vis;
                pen_path = requested_pen;
                work_pending = false;
            }
        }

        if (map.empty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
            continue;
        }
        auto vis_loaded = std::make_unique<VisCheck>();
        bool vis_ok = false;
        if (!vis_path.empty())
            vis_ok = vis_loaded->Load(vis_path);
        if (!vis_ok && !pen_path.empty())
            vis_ok = vis_loaded->LoadPhys(pen_path, false);
        if (!vis_ok)
            vis_loaded.reset();

        {
            std::lock_guard<std::mutex> lock(mtx);
            if (map == current_map && vis_loaded && vis_loaded->Loaded())
            {
                cache[map].vis = std::move(vis_loaded);
                vis_ptr.store(cache[map].vis.get(), std::memory_order_release);
                vis_ready.store(true, std::memory_order_release);
                LOGF(INFO, "VisCheck: vis ready {}", vis_path.empty() ? pen_path : vis_path);
            }
            else if (map == current_map && !vis_ptr.load(std::memory_order_relaxed))
            {
                vis_ptr.store(nullptr, std::memory_order_release);
                vis_ready.store(false, std::memory_order_release);
                LOGF(FATAL, "VisCheck: failed vis={} phys={}", vis_path, pen_path);
            }
        }
    }
}

void MapVisibility::UpdateMap(const char* map_name)
{
    const std::string map = NormalizeMapName(map_name);
    if (map == current_map)
        return;

    current_map = map;
    vis_ready.store(false, std::memory_order_release);

    if (map.empty())
    {
        vis_ptr.store(nullptr, std::memory_order_release);
        LOGF(INFO, "VisCheck: no active map, collision unloaded");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = cache.find(map);
        if (it != cache.end() && it->second.vis && it->second.vis->Loaded())
        {
            vis_ptr.store(it->second.vis.get(), std::memory_order_release);
            vis_ready.store(true, std::memory_order_release);
            LOGF(INFO, "VisCheck: reused cached collision for {}", map);
            return;
        }
        vis_ptr.store(nullptr, std::memory_order_release);
    }

    const auto vis_path = FindOptPath(map);
    const auto pen_path = FindPhysPath(map);
    if (vis_path.empty() && pen_path.empty())
    {
        LOGF(WARNING, "VisCheck: no .opt/.vphys for {} (maps/ and maps/phys/)", map);
        return;
    }

    LOGF(INFO, "VisCheck: loading {} vis={} phys={}", map,
        vis_path.empty() ? "-" : vis_path,
        pen_path.empty() ? "-" : pen_path);
    RequestLoad(map, vis_path, pen_path);
}

bool MapVisibility::IsVisible(const Vec3_t& start, const Vec3_t& end)
{
    if (!vis_ready.load(std::memory_order_acquire))
        return false;
    auto* v = vis_ptr.load(std::memory_order_acquire);
    if (!v || !v->Loaded())
        return false;
    return v->IsVisible(
        Vector3{ start.x, start.y, start.z },
        Vector3{ end.x, end.y, end.z });
}

bool MapVisibility::PointClear(const Vec3_t& eye, const Vec3_t& point, float skin)
{
    if (IsVisible(eye, point))
        return true;

    Vec3_t to_pt = point - eye;
    const float len = to_pt.length();
    if (len < 10.f)
        return true;

    const float skip = (std::min)(28.f, len * 0.35f);
    if (skip > 4.f)
    {
        const Vec3_t eye_out = eye + to_pt * (skip / len);
        if (IsVisible(eye_out, point))
            return true;
    }
    (void)skin;
    return false;
}

bool MapVisibility::GroundDistance(const Vec3_t& origin, float max_down, float& hit_distance)
{
    if (!vis_ready.load(std::memory_order_acquire))
        return false;
    auto* v = vis_ptr.load(std::memory_order_acquire);
    if (!v || !v->Loaded())
        return false;
    return v->TraceRay(
        Vector3{ origin.x, origin.y, origin.z + 4.f },
        Vector3{ 0.f, 0.f, -1.f },
        max_down,
        hit_distance);
}

bool MapVisibility::TraceSegment(const Vec3_t& start, const Vec3_t& end, float hull, TraceHit& out)
{
    out = {};
    out.end = end;
    out.fraction = 1.f;
    if (!vis_ready.load(std::memory_order_acquire))
        return false;
    auto* v = vis_ptr.load(std::memory_order_acquire);
    if (!v || !v->Loaded())
        return false;

    const Vec3_t delta = end - start;
    const float len = delta.length();
    if (len <= 1e-4f)
        return false;

    VisCheck::Hit hit{};
    if (!v->TraceClosest(
        Vector3{ start.x, start.y, start.z },
        Vector3{ delta.x, delta.y, delta.z },
        len, hit))
        return false;

    float stop = hit.distance - hull;
    if (stop < 0.f)
        stop = 0.f;
    const Vec3_t dir = delta * (1.f / len);
    out.hit = true;
    out.distance = stop;
    out.fraction = stop / len;
    out.end = start + dir * stop;
    out.normal = Vec3_t{ hit.normal.x, hit.normal.y, hit.normal.z };
    return true;
}

