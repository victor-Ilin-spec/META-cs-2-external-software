#pragma once
#include "Fnv1a.hpp"
#include <cstdint>

class Schema {
public:
    static int Field(const char* class_name, uint32_t field_hash);

private:
    static Schema& Get();
    int Lookup(const char* class_name, uint32_t field_hash);
    uintptr_t FindClassBinding(const char* class_name);
    static uint32_t Murmur2(const char* str);
    static uint8_t BucketIndex(uint32_t hash);

    uintptr_t client_scope = 0;
};

#define SCHEMA(class_name, field_hash) \
    []() -> int { \
        static const int off = Schema::Field(class_name, field_hash); \
        return off; \
    }()
