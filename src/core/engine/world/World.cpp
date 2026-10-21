#include "World.hpp"
#include "Schema.hpp"
#include "core/engine/Engine.hpp"
#include "core/engine/cache/Cache.hpp"
#include "core/offsets/Offsets.hpp"


#include <algorithm>
#include <cstring>

namespace {

uintptr_t EntityByIndex(const std::shared_ptr<pProcess>& p, uintptr_t entity_list, int index)
{
    if (!p || !entity_list || index < 0)
        return 0;
    const auto chunk = p->read<uintptr_t>(entity_list + 0x10 + 0x8 * (static_cast<unsigned>(index) >> 9));
    if (!chunk)
        return 0;
    return p->read<uintptr_t>(chunk + 0x70 * (index & 0x1FF));
}

uint32_t SchemaHash(const std::shared_ptr<pProcess>& p, uintptr_t entity)
{
    const auto identity = p->read<uintptr_t>(entity + 0x10);
    if (!identity)
        return 0;
    const auto class_info = p->read<uintptr_t>(identity + 0x8);
    if (!class_info)
        return 0;
    const auto name_container = p->read<uintptr_t>(class_info + 0x8);
    if (!name_container)
        return 0;
    const auto schema_name = p->read<uintptr_t>(name_container + 0x8);
    if (!schema_name)
        return 0;
    char class_name[64]{};
    p->read_raw(schema_name, class_name, sizeof(class_name) - 1);
    if (!class_name[0])
        return 0;
    return fnv1a::runtime_hash(class_name);
}

short ItemIdFromHash(uint32_t h)
{
    switch (h)
    {
    case "C_AK47"_hash: return weapon_ak47;
    case "C_WeaponM4A1"_hash: return weapon_m4a1;
    case "C_WeaponM4A1Silencer"_hash: return weapon_m4a1_silencer;
    case "C_WeaponAWP"_hash: return weapon_awp;
    case "C_WeaponAug"_hash: return weapon_aug;
    case "C_WeaponFamas"_hash: return weapon_famas;
    case "C_WeaponGalilAR"_hash: return weapon_galilar;
    case "C_WeaponSG556"_hash: return weapon_sg556;
    case "C_WeaponG3SG1"_hash: return weapon_g3sg1;
    case "C_WeaponSCAR20"_hash: return weapon_scar20;
    case "C_WeaponSSG08"_hash: return weapon_ssg08;
    case "C_WeaponMAC10"_hash: return weapon_mac10;
    case "C_WeaponMP5SD"_hash: return weapon_mp5sd;
    case "C_WeaponMP7"_hash: return weapon_mp7;
    case "C_WeaponMP9"_hash: return weapon_mp9;
    case "C_WeaponBizon"_hash: return weapon_bizon;
    case "C_WeaponP90"_hash: return weapon_p90;
    case "C_WeaponUMP45"_hash: return weapon_ump45;
    case "C_WeaponNOVA"_hash: return weapon_nova;
    case "C_WeaponSawedoff"_hash: return weapon_sawedoff;
    case "C_WeaponXM1014"_hash: return weapon_xm1014;
    case "C_WeaponMag7"_hash: return weapon_mag7;
    case "C_WeaponM249"_hash: return weapon_m249;
    case "C_WeaponNegev"_hash: return weapon_negev;
    case "C_DEagle"_hash: return weapon_deagle;
    case "C_WeaponElite"_hash: return weapon_elite;
    case "C_WeaponFiveSeven"_hash: return weapon_fiveseven;
    case "C_WeaponGlock"_hash: return weapon_glock;
    case "C_WeaponHKP2000"_hash: return weapon_hkp2000;
    case "C_WeaponUSPSilencer"_hash: return weapon_usp_silencer;
    case "C_WeaponP250"_hash: return weapon_p250;
    case "C_WeaponCZ75a"_hash: return weapon_cz75a;
    case "C_WeaponTec9"_hash: return weapon_tec9;
    case "C_WeaponRevolver"_hash: return weapon_revolver;
    case "C_WeaponTaser"_hash: return weapon_taser;
    case "C_Knife"_hash: return weapon_knife;
    case "C_C4"_hash: return weapon_c4;
    case "C_Item_Healthshot"_hash: return weapon_healthshot;
    case "C_HEGrenade"_hash: return weapon_hegrenade;
    case "C_Flashbang"_hash: return weapon_flashbang;
    case "C_SmokeGrenade"_hash: return weapon_smokegrenade;
    case "C_MolotovGrenade"_hash: return weapon_molotov;
    case "C_IncendiaryGrenade"_hash: return weapon_incgrenade;
    case "C_DecoyGrenade"_hash: return weapon_decoy;
    default: return -1;
    }
}

WorldProjectile::Type ProjTypeFromHash(uint32_t h)
{
    switch (h)
    {
    case "C_HEGrenadeProjectile"_hash: return WorldProjectile::Type::he;
    case "C_FlashbangProjectile"_hash: return WorldProjectile::Type::flash;
    case "C_SmokeGrenadeProjectile"_hash: return WorldProjectile::Type::smoke;
    case "C_MolotovProjectile"_hash: return WorldProjectile::Type::molotov;
    case "C_Inferno"_hash: return WorldProjectile::Type::inferno;
    case "C_DecoyProjectile"_hash: return WorldProjectile::Type::decoy;
    default: return WorldProjectile::Type::unknown;
    }
}

} // namespace

World& World::Get()
{
    static World i;
    return i;
}

void World::Tick()
{
    Get().TickImpl();
}

WorldSnapshot World::Copy()
{
    return Get().CopyImpl();
}

WorldSnapshot World::CopyImpl() const
{
    std::lock_guard<std::mutex> lock(mtx);
    return current;
}
void World::TickImpl()
{
    if (!cfg::world::items && !cfg::world::projectiles && !cfg::world::grenade_pred)
        return;

    auto p = Engine::GetProcess();
    auto client = Engine::GetClient();
    if (!p || !client.base || !offsets::entityList || !offsets::globalVars)
        return;

    const auto entity_list = p->read<uintptr_t>(client.base + offsets::entityList);
    if (!entity_list)
        return;

    const auto gv = p->read<uintptr_t>(client.base + offsets::globalVars);
    const float curtime = gv ? p->read<float>(gv + 0x30) : 0.f;

    const int off_owner = SCHEMA("C_BaseEntity", "m_hOwnerEntity"_hash);
    const int off_gsn = SCHEMA("C_BaseEntity", "m_pGameSceneNode"_hash);
    const int off_origin = SCHEMA("CGameSceneNode", "m_vecAbsOrigin"_hash);
    const int off_clip = SCHEMA("C_BasePlayerWeapon", "m_iClip1"_hash);
    const int off_subclass = SCHEMA("C_BaseEntity", "m_nSubclassID"_hash);
    const int off_maxclip = SCHEMA("CBasePlayerWeaponVData", "m_iMaxClip1"_hash);
    const int off_vel = SCHEMA("C_BaseEntity", "m_vecAbsVelocity"_hash);
    const int off_thrower = SCHEMA("C_BaseGrenade", "m_hThrower"_hash);
    const int off_bounces = SCHEMA("C_BaseCSGrenadeProjectile", "m_nBounces"_hash);
    const int off_explode = SCHEMA("C_BaseCSGrenadeProjectile", "m_nExplodeEffectTickBegin"_hash);
    const int off_init_pos = SCHEMA("C_BaseCSGrenadeProjectile", "m_vInitialPosition"_hash);
    const int off_init_vel = SCHEMA("C_BaseCSGrenadeProjectile", "m_vInitialVelocity"_hash);
    const int off_smoke_tick = SCHEMA("C_SmokeGrenadeProjectile", "m_nSmokeEffectTickBegin"_hash);
    const int off_smoke_on = SCHEMA("C_SmokeGrenadeProjectile", "m_bDidSmokeEffect"_hash);
    const int off_decoy_tick = SCHEMA("C_DecoyProjectile", "m_nDecoyShotTick"_hash);
    const int off_fire_count = SCHEMA("C_Inferno", "m_fireCount"_hash);
    const int off_fire_pos = SCHEMA("C_Inferno", "m_firePositions"_hash);
    const int off_fire_on = SCHEMA("C_Inferno", "m_bFireIsBurning"_hash);
    const int off_fire_tick = SCHEMA("C_Inferno", "m_nFireEffectTickBegin"_hash);

    WorldSnapshot next{};
    next.curtime = curtime;
    next.items.reserve(48);
    next.projectiles.reserve(24);

    const auto& snap = Cache::Current();
    if (snap.local.ControllerAddress())
        next.local_pawn_handle = static_cast<uintptr_t>(snap.local.pawn_controller_addr);

    for (int i = 64; i < 2048; ++i)
    {
        const uintptr_t ent = EntityByIndex(p, entity_list, i);
        if (!ent)
            continue;

        const uint32_t hash = SchemaHash(p, ent);
        if (!hash)
            continue;

        const short item_id = ItemIdFromHash(hash);
        if (item_id > 0)
        {
            if (!off_owner || !off_gsn || !off_origin)
                continue;
            const auto owner = p->read<uint32_t>(ent + off_owner);
            if (owner && owner != 0xffffffffu)
                continue;
            const auto gsn = p->read<uintptr_t>(ent + off_gsn);
            if (!gsn)
                continue;
            WorldItem item{};
            item.entity = ent;
            item.origin = p->read<Vec3_t>(gsn + off_origin);
            item.weapon_id = item_id;
            if (off_clip)
                item.ammo = p->read<int>(ent + off_clip);
            if (off_subclass)
            {
                const auto vdata = p->read<uintptr_t>(ent + off_subclass + 0x8);
                if (vdata && off_maxclip)
                    item.max_ammo = p->read<int>(vdata + off_maxclip);
            }
            next.items.push_back(item);
            continue;
        }

        const auto ptype = ProjTypeFromHash(hash);
        if (ptype == WorldProjectile::Type::unknown)
            continue;

        WorldProjectile proj{};
        proj.entity = ent;
        proj.type = ptype;
        proj.curtime = curtime;

        if (ptype == WorldProjectile::Type::inferno)
        {
            if (!off_fire_count || !off_fire_pos || !off_fire_on)
                continue;
            const int fire_count = p->read<int>(ent + off_fire_count);
            if (fire_count <= 0)
                continue;
            const int n = (std::min)(fire_count, 64);
            Vec3_t center{};
            int used = 0;
            for (int f = 0; f < n; ++f)
            {
                if (!p->read<bool>(ent + off_fire_on + f))
                    continue;
                const auto pt = p->read<Vec3_t>(ent + off_fire_pos + f * sizeof(Vec3_t));
                proj.fire_points[static_cast<size_t>(used)] = pt;
                center += pt;
                ++used;
            }
            if (used <= 0)
                continue;
            proj.fire_count = used;
            proj.origin = center * (1.f / static_cast<float>(used));
            if (off_fire_tick)
            {
                const int tick = p->read<int>(ent + off_fire_tick);
                proj.effect_tick_begin = tick;
                proj.expire_time = static_cast<float>(tick) * (1.f / 64.f) + 7.f;
            }
            next.projectiles.push_back(proj);
            continue;
        }

        if (!off_gsn || !off_origin)
            continue;
        const auto gsn = p->read<uintptr_t>(ent + off_gsn);
        if (!gsn)
            continue;
        proj.origin = p->read<Vec3_t>(gsn + off_origin);
        if (off_vel)
            proj.velocity = p->read<Vec3_t>(ent + off_vel);
        if (off_thrower)
            proj.thrower_handle = p->read<uint32_t>(ent + off_thrower);
        if (off_bounces)
            proj.bounces = p->read<int>(ent + off_bounces);
        if (off_init_pos)
            proj.initial_pos = p->read<Vec3_t>(ent + off_init_pos);
        if (off_init_vel)
            proj.initial_vel = p->read<Vec3_t>(ent + off_init_vel);

        if ((ptype == WorldProjectile::Type::he || ptype == WorldProjectile::Type::flash) && off_explode)
            proj.detonated = p->read<int>(ent + off_explode) > 0;
        else if (ptype == WorldProjectile::Type::smoke)
        {
            if (off_smoke_tick)
                proj.effect_tick_begin = p->read<int>(ent + off_smoke_tick);
            if (off_smoke_on)
                proj.smoke_active = p->read<bool>(ent + off_smoke_on);
        }
        else if (ptype == WorldProjectile::Type::decoy && off_decoy_tick)
            proj.effect_tick_begin = p->read<int>(ent + off_decoy_tick);

        next.projectiles.push_back(proj);
    }

    std::lock_guard<std::mutex> lock(mtx);
    current = std::move(next);
}
