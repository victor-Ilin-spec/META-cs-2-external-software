#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <tuple>
#include <vector>

#include "imgui.h"

#include "core/engine/types/Vec2.hpp"
#include "core/engine/types/Vec3.hpp"

class Base_Radar
{
public:
	void SetSize(const float& Size);
	void SetPos(const Vec2_t& Pos);
	void SetRange(const float& Range);
	void SetCrossColor(const ImColor& Color);
	void SetProportion(const float& Proportion);
	void SetDrawList(ImDrawList* DrawList);

	float GetSize() const;
	Vec2_t GetPos() const;

	void AddPoint(const Vec3_t& LocalPos, const float& LocalYaw,
		const Vec3_t& Pos, ImColor Color, int Type = 0, float Yaw = 0.0f);
	void Render();

public:
	ImDrawList* DrawList = nullptr;
	bool ShowCrossLine = true;
	ImColor CrossColor = ImColor(255, 255, 255, 255);
	float Proportion = 2680.f;
	float CircleSize = 4.f;
	float ArrowSize = 11.f;
	float ArcArrowSize = 7.f;
	float RenderRange = 250.f;
	float LocalYaw = 0.0f;
	bool Opened = true;
	int PointType = 0;

private:
	Vec2_t Pos{ 0.f, 0.f };
	float Width = 200.f;
	std::vector<std::tuple<Vec2_t, ImColor, int, float>> Points;
};
