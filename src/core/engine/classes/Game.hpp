#pragma once

class Game {
public:
    Game() {};

    bool Update();
    bool UpdateMatrix();
    bool UpdateEntityList();

public:
    view_matrix_t view_matrix{};

    uintptr_t entity_list = 0;
    uintptr_t list_entry = 0;
    uintptr_t controllers[65]{};
    int controller_count = 0;
    uintptr_t address;
};