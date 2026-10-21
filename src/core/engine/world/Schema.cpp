#include "Schema.hpp"
#include "core/engine/Engine.hpp"
#include "core/memory/Memory.hpp"
#include <Windows.h>
#include <algorithm>


#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr size_t kChunk = 0x10000;

struct Section {
    uintptr_t base{};
    size_t size{};
    uint32_t characteristics{};
    char name[9]{};
};

std::vector<Section> ReadSections(const std::shared_ptr<pProcess>& p, uintptr_t module_base)
{
    std::vector<Section> out;
    if (!p || !module_base)
        return out;

    IMAGE_DOS_HEADER dos{};
    if (!p->read_raw(module_base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE)
        return out;

    IMAGE_NT_HEADERS64 nt{};
    if (!p->read_raw(module_base + dos.e_lfanew, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE)
        return out;

    const uintptr_t first = module_base + dos.e_lfanew + offsetof(IMAGE_NT_HEADERS64, OptionalHeader)
        + nt.FileHeader.SizeOfOptionalHeader;
    out.reserve(nt.FileHeader.NumberOfSections);
    for (uint16_t i = 0; i < nt.FileHeader.NumberOfSections; ++i)
    {
        IMAGE_SECTION_HEADER hdr{};
        if (!p->read_raw(first + i * sizeof(IMAGE_SECTION_HEADER), &hdr, sizeof(hdr)))
            continue;
        Section sec{};
        sec.base = module_base + hdr.VirtualAddress;
        sec.size = hdr.Misc.VirtualSize;
        sec.characteristics = hdr.Characteristics;
        std::memcpy(sec.name, hdr.Name, 8);
        sec.name[8] = '\0';
        out.push_back(sec);
    }
    return out;
}

template<typename Fn>
uintptr_t ScanChunked(const std::shared_ptr<pProcess>& p, uintptr_t base, size_t size, size_t overlap, Fn&& match)
{
    std::vector<uint8_t> buf(kChunk + overlap);
    for (size_t off = 0; off < size; off += kChunk)
    {
        const size_t n = (std::min)(kChunk + overlap, size - off);
        if (!p->read_raw(base + off, buf.data(), n))
            continue;
        const size_t hit = match(buf.data(), n);
        if (hit != static_cast<size_t>(-1))
            return base + off + hit;
    }
    return 0;
}

uintptr_t FindQword(const std::shared_ptr<pProcess>& p, uintptr_t module_base, uintptr_t value, uint32_t filter)
{
    for (const auto& sec : ReadSections(p, module_base))
    {
        if (!(sec.characteristics & filter) || sec.size < 8)
            continue;
        const uintptr_t hit = ScanChunked(p, sec.base, sec.size, 8, [&](const uint8_t* data, size_t n) -> size_t {
            for (size_t i = 0; i + 8 <= n; i += 8)
            {
                if (*reinterpret_cast<const uintptr_t*>(data + i) == value)
                    return i;
            }
            return static_cast<size_t>(-1);
        });
        if (hit)
            return hit;
    }
    return 0;
}

uintptr_t FindVtable(const std::shared_ptr<pProcess>& p, uintptr_t module_base, const char* class_name)
{
    const std::string descriptor = std::string(".?AV") + class_name + "@@";
    uintptr_t type_desc = 0;
    for (const auto& sec : ReadSections(p, module_base))
    {
        constexpr uint32_t required = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;
        if ((sec.characteristics & required) != required || sec.size <= descriptor.size())
            continue;
        const uintptr_t hit = ScanChunked(p, sec.base, sec.size, descriptor.size() + 1,
            [&](const uint8_t* data, size_t n) -> size_t {
                for (size_t i = 0; i + descriptor.size() < n; ++i)
                {
                    if (std::memcmp(data + i, descriptor.data(), descriptor.size() + 1) == 0)
                        return i;
                }
                return static_cast<size_t>(-1);
            });
        if (hit)
        {
            type_desc = hit - 0x10;
            break;
        }
    }
    if (!type_desc)
        return 0;

    const uint32_t rva = static_cast<uint32_t>(type_desc - module_base);
    uintptr_t col = 0;
    for (const auto& sec : ReadSections(p, module_base))
    {
        if (std::string_view{ sec.name }.find(".rdata") == std::string_view::npos || sec.size < 0x30)
            continue;
        const uintptr_t hit = ScanChunked(p, sec.base, sec.size, 0x30, [&](const uint8_t* data, size_t n) -> size_t {
            for (size_t i = 0; i + 0x30 <= n; i += 8)
            {
                if (reinterpret_cast<const uint32_t*>(data + i)[3] == rva)
                    return i;
            }
            return static_cast<size_t>(-1);
        });
        if (hit)
        {
            col = hit;
            break;
        }
    }
    if (!col)
        return 0;
    const uintptr_t ref = FindQword(p, module_base, col, IMAGE_SCN_MEM_READ);
    return ref ? ref + 8 : 0;
}

uintptr_t FindVtableInstance(const std::shared_ptr<pProcess>& p, uintptr_t module_base, const char* class_name)
{
    const uintptr_t vt = FindVtable(p, module_base, class_name);
    if (!vt)
        return 0;
    return FindQword(p, module_base, vt, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE);
}

} // namespace

Schema& Schema::Get()
{
    static Schema i;
    return i;
}

int Schema::Field(const char* class_name, uint32_t field_hash)
{
    return Get().Lookup(class_name, field_hash);
}

int Schema::Lookup(const char* class_name, uint32_t field_hash)
{
    auto p = Engine::GetProcess();
    if (!p)
        return 0;

    const uintptr_t info = FindClassBinding(class_name);
    if (!info)
        return 0;

    const auto field_count = p->read<int16_t>(info + 0x24);
    const auto fields_ptr = p->read<uintptr_t>(info + 0x30);
    if (field_count <= 0 || !fields_ptr)
        return 0;

    for (int16_t i = 0; i < field_count; ++i)
    {
        const uintptr_t field_addr = fields_ptr + static_cast<size_t>(i) * 0x20;
        const auto name_ptr = p->read<uintptr_t>(field_addr);
        if (!name_ptr)
            continue;
        char name[256]{};
        p->read_raw(name_ptr, name, sizeof(name) - 1);
        if (fnv1a::runtime_hash(name) == field_hash)
            return p->read<int>(field_addr + 0x10);
    }
    return 0;
}

uintptr_t Schema::FindClassBinding(const char* class_name)
{
    auto p = Engine::GetProcess();
    if (!p)
        return 0;

    if (!client_scope)
    {
        const auto schemasystem = p->GetModule("schemasystem.dll");
        if (!schemasystem.base)
            return 0;
        const uintptr_t schema_system = FindVtableInstance(p, schemasystem.base, "CSchemaSystem");
        if (!schema_system)
            return 0;

        const int scope_count = p->read<int>(schema_system + 0x190);
        const uintptr_t scope_data = p->read<uintptr_t>(schema_system + 0x198);
        if (scope_count <= 0 || scope_count > 64 || !scope_data)
            return 0;

        for (int i = 0; i < scope_count; ++i)
        {
            const uintptr_t scope_ptr = p->read<uintptr_t>(scope_data + i * sizeof(uintptr_t));
            if (!scope_ptr)
                continue;
            char scope_name[32]{};
            p->read_raw(scope_ptr + 0x8, scope_name, sizeof(scope_name) - 1);
            if (std::strcmp(scope_name, "client.dll") == 0)
            {
                client_scope = scope_ptr;
                break;
            }
        }
        if (!client_scope)
            return 0;
    }

    const uint32_t name_hash = Murmur2(class_name);
    const uint8_t idx = BucketIndex(name_hash);
    const uintptr_t bucket_addr = client_scope + 0x5c0 + static_cast<size_t>(idx) * 24;
    const uintptr_t first = p->read<uintptr_t>(bucket_addr);
    uintptr_t element = first;
    while (element)
    {
        if (p->read<uint32_t>(element) == name_hash)
        {
            const uintptr_t data = p->read<uintptr_t>(element + 16);
            if (data)
                return data;
        }
        element = p->read<uintptr_t>(element + 8);
    }

    element = p->read<uintptr_t>(bucket_addr + 16);
    while (element && element != first)
    {
        if (p->read<uint32_t>(element) == name_hash)
        {
            const uintptr_t data = p->read<uintptr_t>(element + 16);
            if (data)
                return data;
        }
        element = p->read<uintptr_t>(element + 8);
    }
    return 0;
}

uint32_t Schema::Murmur2(const char* str)
{
    unsigned len = 0;
    const auto* data = reinterpret_cast<const uint8_t*>(str);
    while (data[len])
        ++len;

    uint32_t h = len ^ 0xBAADFEED;
    unsigned remaining = len;
    const uint8_t* p = data;
    while (remaining >= 4)
    {
        uint32_t k = *reinterpret_cast<const uint32_t*>(p);
        k *= 0x5BD1E995;
        k ^= k >> 24;
        k *= 0x5BD1E995;
        h *= 0x5BD1E995;
        h ^= k;
        p += 4;
        remaining -= 4;
    }
    switch (remaining)
    {
    case 3: h ^= p[2] << 16; [[fallthrough]];
    case 2: h ^= p[1] << 8; [[fallthrough]];
    case 1: h ^= p[0];
        h *= 0x5BD1E995;
        break;
    }
    h ^= h >> 13;
    h *= 0x5BD1E995;
    h ^= h >> 15;
    return h;
}

uint8_t Schema::BucketIndex(uint32_t hash)
{
    auto v4 = (4097 * hash + 2127912214) ^ ((4097 * hash + 2127912214) >> 19) ^ 0xC761C23C;
    auto v5 = (33 * v4 + 374761393) << 9;
    auto a = 9 * (v5 ^ (33 * v4 - 369570787)) - 42973499;
    auto b = (a >> 16) ^ a ^ 0xB55A4F09;
    auto byte0 = static_cast<uint8_t>((a >> 16) ^ (9 * (v5 ^ (33 * v4 + 29)) - 59));
    auto byte1 = static_cast<uint8_t>(b >> 16);
    auto byte2 = static_cast<uint8_t>((static_cast<uint16_t>((a >> 16) ^ (9 * (v5 ^ (33 * v4 - 13283)) + 18117) ^ 0x4F09 ^ (b >> 16))) >> 8);
    return byte0 ^ 9 ^ byte1 ^ byte2;
}
