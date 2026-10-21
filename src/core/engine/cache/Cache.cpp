#include "Cache.hpp"
#include "FrameStore.hpp"

#include "core/engine/Engine.hpp"
#include "core/offsets/Dumper.hpp"
#include "core/engine/vischeck/MapVisibility.hpp"

#include <algorithm>
#include <cstring>

namespace {

void PackPlayer(FramePlayer& dst, const Player& src)
{
    dst.pawn = src.PawnAddress();
    dst.controller = src.ControllerAddress();
    dst.index = src.index;
    dst.team = src.team;
    dst.health = src.health;
    dst.armor = src.armor;
    dst.ammo = src.ammo;
    dst.pawn_handle = src.pawn_controller_addr;
    dst.weapon_id = src.weapon.item_index;
    dst.alive = src.alive;
    dst.localplayer = src.localplayer;
    dst.visible = src.visible;
    dst.spotted = src.spotted;
    dst.can_hit = src.can_hit;
    dst.scoped = src.scoped;
    dst.flashed = src.flashed;
    dst.defusing = src.defusing;
    dst.has_c4 = src.has_c4;
    dst.reloading = src.is_reloading;
    dst.has_helmet = src.has_helmet;
    dst.has_defuser = src.has_defuser;
    dst.immune = src.immune;
    dst.pos = src.pos;
    dst.vel = src.vel;
    dst.eye = src.eye_angles;
    std::memcpy(dst.name, src.name, sizeof(dst.name));
    dst.name[sizeof(dst.name) - 1] = '\0';

    const int n = static_cast<int>((std::min)(src.bone_list.size(), static_cast<size_t>(FramePlayer::kBones)));
    dst.bone_count = n;
    for (int i = 0; i < n; ++i)
    {
        dst.bones[i].pos = src.bone_list[i].pos;
        dst.bones[i].qx = src.bone_list[i].qx;
        dst.bones[i].qy = src.bone_list[i].qy;
        dst.bones[i].qz = src.bone_list[i].qz;
        dst.bones[i].qw = src.bone_list[i].qw;
    }
    for (int i = n; i < FramePlayer::kBones; ++i)
        dst.bones[i].pos = {};
}


void PublishFrame(const Cache& cache, int tick)
{
    auto& slot = FrameStore::Get().BeginWrite();
    slot.seq = FrameStore::Get().Seq() + 1;
    slot.tick = tick;
    slot.matrix = cache.game.view_matrix;
    slot.local_index = cache.local.index;
    slot.local_team = cache.local.team;
    slot.local_health = cache.local.health;
    slot.local_weapon = cache.local.weapon.item_index;
    slot.local_eye = cache.local.EyePos();
    slot.local_alive = cache.local.alive;
    slot.local_scoped = cache.local.scoped;
    slot.local_zoom = cache.local.weapon.zoom_level;
    slot.in_match = cache.globals.in_match;
    slot.is_deathmatch = cache.globals.is_deathmatch;
    std::memcpy(slot.map, cache.globals.map_name, sizeof(slot.map));
    slot.map[sizeof(slot.map) - 1] = '\0';

    slot.player_count = 0;
    for (const auto& pl : cache.players)
    {
        if (slot.player_count >= FrameData::kMaxPlayers)
            break;
        if (!pl.alive && !pl.localplayer)
            continue;
        PackPlayer(slot.players[slot.player_count++], pl);
    }
    FrameStore::Get().Publish(slot);
}




} // namespace

bool Cache::Refresh() {
    return Get().RefreshImpl();
}

Snapshot Cache::CopySnapshot() {
    std::lock_guard<std::mutex> lock(Get().mtx);
    Snapshot out;
    const auto& src = Get().current;
    out.game = src.game;
    out.bomb = src.bomb;
    out.globals = src.globals;
    out.local.CopyStateFrom(src.local);
    out.players.resize(src.players.size());
    for (size_t i = 0; i < src.players.size(); ++i)
        out.players[i].CopyStateFrom(src.players[i]);
    return out;
}

const Snapshot& Cache::Current() {
    return Get().current;
}

bool Cache::RefreshImpl() {
    auto p = Engine::GetProcess();
    auto client = Engine::GetClient();

    if (!p)
        return false;

    auto now = steady_clock::now();

    // Matrix every logic tick. Everything else is gated by cache_ms / tickcount.
    if (!game.Update())
        return false;

    int tick = 0;
    if (p->handle_ && client.base && offsets::globalVars)
    {
        const auto gv = p->read<uintptr_t>(client.base + offsets::globalVars);
        if (gv)
            tick = p->read<int>(gv + offsets::global::tickCount);
    }

    const int interval = (std::max)(cfg::settings::cache_ms, 0);
    const bool same_tick = (tick != 0 && tick == last_tick);
    const bool too_soon = interval > 0 && last.time_since_epoch().count() != 0
        && (now - last < (interval * 1ms));
    if (same_tick || too_soon)
    {
        std::lock_guard<std::mutex> lock(mtx);
        current.game.view_matrix = game.view_matrix;
        return true;
    }

    game.UpdateEntityList();
    if (!globals.Update())
        return false;
    MapVisibility::Get().UpdateMap(globals.map_name);
    bomb.Update();

    uintptr_t local_ctrl = 0;
    if (p->handle_ && client.base && offsets::localPlayerController)
        local_ctrl = p->read<uintptr_t>(client.base + offsets::localPlayerController);

    const int client_count = globals.max_clients > 0
        ? std::clamp(globals.max_clients, 1, 64)
        : 64;

    Player* local_slot = nullptr;
    for (int i = 0; i < client_count; i++) {
        auto& slot = pool[static_cast<size_t>(i)];
        slot.Bind(i, game.entity_list, game.list_entry);

        if (!slot.Update())
        {
            slot.Invalidate();
            continue;
        }

        if (local_ctrl && slot.ControllerAddress() == local_ctrl)
            slot.localplayer = true;

        if (slot.localplayer)
            local_slot = &slot;

        slot.has_c4 = (bomb.carrier != 0 && (uintptr_t)slot.pawn_controller_addr == bomb.carrier);
    }

    for (int i = client_count; i < 64; ++i)
        pool[static_cast<size_t>(i)].Invalidate();

    if (local_slot)
        this->local.CopyStateFrom(*local_slot);

    const int spotted_index = this->local.index;
    const int spotted_handle = this->local.pawn_controller_addr;
    const bool vis_ready = MapVisibility::Get().Loaded();
    RECT rc{};
    const bool have_rc = p->hwnd_ && GetClientRect(p->hwnd_, &rc);
    const Vec2_t screen = (have_rc && rc.right > rc.left)
        ? Vec2_t{ (float)(rc.right - rc.left), (float)(rc.bottom - rc.top) }
        : Vec2_t{ 1920.f, 1080.f };

    for (int i = 0; i < client_count; ++i)
    {
        auto& slot = pool[static_cast<size_t>(i)];
        if (!slot.PawnAddress())
            continue;
        slot.RefreshSpotted(spotted_index, spotted_handle);
        if (!vis_ready)
        {
            slot.RefreshVisible(this->local);
            continue;
        }
        if (!slot.alive || slot.localplayer)
            continue;
        Vec2_t out{};
        if (!game.view_matrix.wts(slot.HeadPos(), screen, out, true))
            continue;
        slot.RefreshVisible(this->local);
    }
    if (local_slot)
        this->local.CopyStateFrom(*local_slot);


    players.clear();
    players.reserve(static_cast<size_t>(client_count));
    for (int i = 0; i < client_count; ++i)
    {
        const auto& slot = pool[static_cast<size_t>(i)];
        if (!slot.PawnAddress() && !slot.alive)
            continue;
        players.emplace_back();
        players.back().CopyStateFrom(slot);
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        duration = duration_cast<std::chrono::milliseconds>(now - last);
        last = now;
        last_tick = tick;
        current.game = game;
        current.bomb = bomb;
        current.globals = globals;
        current.local.CopyStateFrom(local);
        current.players.resize(players.size());
        for (size_t i = 0; i < players.size(); ++i)
            current.players[i].CopyStateFrom(players[i]);
    }

    PublishFrame(*this, tick);
    return true;
}
