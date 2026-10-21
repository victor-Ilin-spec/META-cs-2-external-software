#include "Game.hpp"

#include "core/engine/Engine.hpp"
#include "core/offsets/Dumper.hpp"
#include <cstring>

bool Game::Update() {
	if (!Engine::GetProcess())
		return false;	
	
	if (!UpdateMatrix()) {
		LOGF(FATAL, "Failed to update view matrix");
		return false;
	}

	// No need to be updated along with the view matrix
	//if (!UpdateEntityList()) {
	//	LOGF(FATAL, "Failed to update entity list");
	//	return false;
	//}

	return true;
}

bool Game::UpdateMatrix() {
	auto p = Engine::GetProcess();
	auto client = Engine::GetClient();

	this->view_matrix = p->read<view_matrix_t>(client.base + offsets::viewMatrix);

	return true;
}

bool Game::UpdateEntityList() {
	auto p = Engine::GetProcess();
	auto client = Engine::GetClient();

	this->entity_list = p->read<DWORD64>(client.base + offsets::entityList);
	this->list_entry = p->read<DWORD64>(this->entity_list + 0x10);
	this->controller_count = 0;
	if (!this->list_entry)
		return false;

	// Первый бакет: контроллеры 1..64 лежат подряд с шагом 0x70.
	uint8_t raw[65 * 0x70]{};
	if (!p->read_raw(this->list_entry, raw, sizeof(raw)))
		return this->list_entry != 0;

	for (int i = 1; i <= 64; ++i)
	{
		uintptr_t ctrl = 0;
		std::memcpy(&ctrl, raw + i * 0x70, sizeof(ctrl));
		this->controllers[i] = ctrl;
	}
	this->controller_count = 64;
	return true;
}