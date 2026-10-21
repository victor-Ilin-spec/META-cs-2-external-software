#pragma once
#include <cstdint>
#include <cstddef>

namespace fnv1a {

constexpr uint32_t hash(const char* str, size_t length) noexcept
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < length; ++i)
    {
        h ^= static_cast<uint32_t>(str[i]);
        h *= 16777619u;
    }
    return h;
}

inline uint32_t runtime_hash(const char* str) noexcept
{
    uint32_t h = 2166136261u;
    while (*str)
    {
        h ^= static_cast<uint32_t>(*str++);
        h *= 16777619u;
    }
    return h;
}

} // namespace fnv1a

constexpr uint32_t operator""_hash(const char* str, size_t length) noexcept
{
    return fnv1a::hash(str, length);
}
