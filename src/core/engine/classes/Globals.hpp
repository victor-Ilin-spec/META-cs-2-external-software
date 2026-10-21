#pragma once

class Globals {
public:
    Globals() {};

    bool Update();
public:
    int max_clients = 0;
    long current_time = 0;
	char map_name[32]{};
	bool in_match = false;
	bool is_deathmatch = false;

    uintptr_t localplayer = 0;

private:
    uintptr_t address = 0;
};