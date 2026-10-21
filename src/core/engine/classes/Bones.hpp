#pragma once

enum bone_index : DWORD {
    origin = 0,
    pelvis = 1,
    spine_0 = 2,
    spine_1 = 3,
    spine_2 = 4,
    neck = 6,
    head = 7,
    shoulder_L = 9,
    elbow_L = 10,
    hand_L = 11,
    shoulder_R = 13,
    elbow_R = 14,
    hand_R = 15,
    hip_L = 17,
    knee_L = 18,
    foot_heel_L = 19,
    hip_R = 20,
    knee_R = 21,
    foot_heel_R = 22,
    chest = 23,
};

struct BoneLink { int a; int b; };

static const BoneLink kSkeleton[] = {
    { bone_index::head,       bone_index::neck },
    { bone_index::neck,       bone_index::spine_2 },
    { bone_index::spine_2,    bone_index::spine_1 },
    { bone_index::spine_1,    bone_index::spine_0 },
    { bone_index::spine_0,    bone_index::pelvis },

    { bone_index::neck,       bone_index::shoulder_L },
    { bone_index::shoulder_L, bone_index::elbow_L },
    { bone_index::elbow_L,    bone_index::hand_L },

    { bone_index::neck,       bone_index::shoulder_R },
    { bone_index::shoulder_R, bone_index::elbow_R },
    { bone_index::elbow_R,    bone_index::hand_R },

    { bone_index::pelvis,     bone_index::hip_L },
    { bone_index::hip_L,      bone_index::knee_L },
    { bone_index::knee_L,     bone_index::foot_heel_L },

    { bone_index::pelvis,     bone_index::hip_R },
    { bone_index::hip_R,      bone_index::knee_R },
    { bone_index::knee_R,     bone_index::foot_heel_R },
};

struct bone_data {
    Vec3_t pos;
    float pad = 0.f;
    float qx = 0.f;
    float qy = 0.f;
    float qz = 0.f;
    float qw = 1.f;
};

struct bone_pos {
    Vec3_t pos;
    float qx = 0.f;
    float qy = 0.f;
    float qz = 0.f;
    float qw = 1.f;
};