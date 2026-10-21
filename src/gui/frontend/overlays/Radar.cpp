#include "Radar.hpp"

#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

Vec2_t RevolveCoordinatesSystem(float RevolveAngle, const Vec2_t& OriginPos, const Vec2_t& DestPos)
{
	if (RevolveAngle == 0.f)
		return DestPos;

	const float rad = RevolveAngle * (float)M_PI / 180.f;
	const float c = std::cos(rad);
	const float s = std::sin(rad);
	Vec2_t ResultPos;
	ResultPos.x = OriginPos.x + (DestPos.x - OriginPos.x) * c + (DestPos.y - OriginPos.y) * s;
	ResultPos.y = OriginPos.y - (DestPos.x - OriginPos.x) * s + (DestPos.y - OriginPos.y) * c;
	return ResultPos;
}

ImVec2 ToIm(const Vec2_t& v)
{
	return ImVec2(v.x, v.y);
}

void DrawArrow(ImDrawList* dl, const Vec2_t& pt, float yaw, float local_yaw, float size, ImColor col)
{
	const float revolve = local_yaw - yaw;
	const Vec2_t a = RevolveCoordinatesSystem(revolve, pt, Vec2_t(pt.x, pt.y - size));
	const Vec2_t b = RevolveCoordinatesSystem(revolve, pt, Vec2_t(pt.x - size / 1.5f, pt.y + size / 2.f));
	const Vec2_t c = RevolveCoordinatesSystem(revolve, pt, Vec2_t(pt.x + size / 1.5f, pt.y + size / 2.f));
	dl->AddTriangleFilled(ToIm(a), ToIm(b), ToIm(c), col);
	dl->AddTriangle(ToIm(a), ToIm(b), ToIm(c), IM_COL32(0, 0, 0, col.Value.w * 255.f), 1.f);
}

void DrawCircleDot(ImDrawList* dl, const Vec2_t& pt, float radius, ImColor col)
{
	dl->AddCircleFilled(ToIm(pt), radius, IM_COL32(0, 0, 0, (int)(col.Value.w * 255.f)));
	dl->AddCircle(ToIm(pt), radius, col, 0, 1.4f);
	dl->AddCircleFilled(ToIm(pt), radius * 0.55f, col);
}

void DrawArcArrow(ImDrawList* dl, const Vec2_t& pt, float yaw, float local_yaw, float size, ImColor col)
{
	DrawCircleDot(dl, pt, size * 0.45f, col);
	const float revolve = local_yaw - yaw;
	const Vec2_t tip = RevolveCoordinatesSystem(revolve, pt, Vec2_t(pt.x, pt.y - size));
	const Vec2_t l = RevolveCoordinatesSystem(revolve, pt, Vec2_t(pt.x - size * 0.35f, pt.y - size * 0.15f));
	const Vec2_t r = RevolveCoordinatesSystem(revolve, pt, Vec2_t(pt.x + size * 0.35f, pt.y - size * 0.15f));
	dl->AddTriangleFilled(ToIm(tip), ToIm(l), ToIm(r), col);
}

} // namespace

void Base_Radar::SetSize(const float& Size) { Width = Size; }
void Base_Radar::SetPos(const Vec2_t& NewPos) { Pos = NewPos; }
void Base_Radar::SetRange(const float& Range) { RenderRange = Range; }
void Base_Radar::SetCrossColor(const ImColor& Color) { CrossColor = Color; }
void Base_Radar::SetProportion(const float& NewProportion) { Proportion = NewProportion; }
void Base_Radar::SetDrawList(ImDrawList* List) { DrawList = List; }
float Base_Radar::GetSize() const { return Width; }
Vec2_t Base_Radar::GetPos() const { return Pos; }

void Base_Radar::AddPoint(const Vec3_t& LocalPos, const float& LocalYawIn,
	const Vec3_t& WorldPos, ImColor Color, int Type, float Yaw)
{
	this->LocalYaw = LocalYawIn;

	const float dx = WorldPos.x - LocalPos.x;
	const float dy = WorldPos.y - LocalPos.y;
	float Distance = std::sqrt(dx * dx + dy * dy);
	float Angle = std::atan2(dy, dx) * 180.f / (float)M_PI;
	Angle = (this->LocalYaw - Angle) * (float)M_PI / 180.f;

	Distance = Distance / this->Proportion * this->RenderRange * 2.f;

	Vec2_t PointPos;
	PointPos.x = this->Pos.x + Distance * std::sin(Angle);
	PointPos.y = this->Pos.y - Distance * std::cos(Angle);

	if (PointPos.x < this->Pos.x - RenderRange || PointPos.x > this->Pos.x + RenderRange ||
		PointPos.y > this->Pos.y + RenderRange || PointPos.y < this->Pos.y - RenderRange)
		return;

	this->Points.emplace_back(PointPos, Color, Type, Yaw);
}

void Base_Radar::Render()
{
	if (!DrawList || Width <= 0.f)
		return;

	const float half = Width * 0.5f;
	const ImVec2 min(Pos.x - half, Pos.y - half);
	const ImVec2 max(Pos.x + half, Pos.y + half);
	DrawList->PushClipRect(min, max, true);

	if (ShowCrossLine)
	{
		DrawList->AddLine(ImVec2(Pos.x - half, Pos.y), ImVec2(Pos.x + half, Pos.y), CrossColor, 1.f);
		DrawList->AddLine(ImVec2(Pos.x, Pos.y - half), ImVec2(Pos.x, Pos.y + half), CrossColor, 1.f);
	}

	for (auto& p : Points)
	{
		const Vec2_t pt = std::get<0>(p);
		const ImColor col = std::get<1>(p);
		const int type = std::get<2>(p);
		const float yaw = std::get<3>(p);

		if (type == 1)
			DrawArrow(DrawList, pt, yaw, LocalYaw, ArrowSize, col);
		else if (type == 2)
			DrawArcArrow(DrawList, pt, yaw, LocalYaw, ArcArrowSize, col);
		else
			DrawCircleDot(DrawList, pt, CircleSize, col);
	}

	DrawList->PopClipRect();
	Points.clear();
}
