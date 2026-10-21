#pragma once
#include "core/engine/classes/Game.hpp"
#include "core/engine/classes/Bomb.hpp"
#include "core/engine/classes/Player.hpp"
#include "core/engine/classes/Globals.hpp"
#include "core/engine/classes/Weapon.hpp"
#include <array>
#include <mutex>
#include <vector>
#include <chrono>


using namespace std::chrono;

struct Snapshot {
	Game game;
	Bomb bomb;
	Player local;
	Globals globals;
	std::vector<Player> players;
};

class Cache {
public:
	Game game;
	Bomb bomb;
	Player local;
	Globals globals;
	std::vector<Player> players;
public:
	static Cache& Get()
	{
		static Cache instance{};
		return instance;
	}

	static Snapshot CopySnapshot();
	static const Snapshot& Current();
	static bool Refresh();
private:
	std::mutex mtx;
	Snapshot current{};
	std::array<Player, 64> pool{};
	milliseconds duration{1};
	steady_clock::time_point last{};
	int last_tick = 0;
private:
	bool RefreshImpl();
};