#pragma once
#include "VisCheck.h"
#include "core/engine/types/Vec3.hpp"
#include <atomic>
#include <filesystem>

#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

class MapVisibility {
public:
    static MapVisibility& Get();

    void UpdateMap(const char* map_name);
    static void EnsureMapsDir();
    static std::filesystem::path MapsDir();

    const std::string& CurrentMap() const { return current_map; }
    bool Loaded() const { return vis_ready.load(std::memory_order_acquire); }
    bool Ready() const { return vis_ready.load(std::memory_order_acquire); }
    bool IsVisible(const Vec3_t& start, const Vec3_t& end);
    bool PointClear(const Vec3_t& eye, const Vec3_t& point, float skin = 8.f);
    bool GroundDistance(const Vec3_t& origin, float max_down, float& hit_distance);
    struct TraceHit {
        bool hit = false;
        float fraction = 1.f;
        float distance = 0.f;
        Vec3_t end{};
        Vec3_t normal{};
    };
    bool TraceSegment(const Vec3_t& start, const Vec3_t& end, float hull, TraceHit& out);


private:
    MapVisibility() = default;
    ~MapVisibility();

    MapVisibility(const MapVisibility&) = delete;
    MapVisibility& operator=(const MapVisibility&) = delete;

    static std::string NormalizeMapName(const char* raw);
    static std::string FindNamed(const std::string& map, const char* const* names, size_t count);
    static std::string FindOptPath(const std::string& map);
    static std::string FindPhysPath(const std::string& map);

    void RequestLoad(const std::string& map, const std::string& vis_path, const std::string& pen_path);
    void WorkerLoop();

    std::atomic<VisCheck*> vis_ptr{ nullptr };
    std::string current_map;
    std::string requested_map;
    std::string requested_vis;
    std::string requested_pen;

    struct LoadedMap {
        std::unique_ptr<VisCheck> vis;
    };
    std::unordered_map<std::string, LoadedMap> cache;

    std::mutex mtx;
    std::thread worker;
    std::atomic<bool> vis_ready{ false };
    std::atomic<bool> running{ true };
    std::atomic<bool> worker_started{ false };
    bool work_pending = false;
};
