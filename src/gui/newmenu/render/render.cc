#include "../includes.hh"

math::c_vector_2d input::c_input_controller::get_mouse_position()
{
	return math::c_vector_2d(ImGui::GetMousePos().x, ImGui::GetMousePos().y);
}

bool input::c_input_controller::mouse_in_region(const math::c_vector_2d& pos, const math::c_vector_2d& size)
{
	return (ImGui::GetIO().MousePos.x > pos.x && ImGui::GetIO().MousePos.y > pos.y &&
		ImGui::GetIO().MousePos.x < pos.x + size.x && ImGui::GetIO().MousePos.y < pos.y + size.y);
}

bool input::c_input_controller::clicked(mouse_buttons button)
{
	return ImGui::IsMouseClicked(ImGuiMouseButton(button));
}

bool input::c_input_controller::click_down(mouse_buttons button)
{
	return ImGui::IsMouseDown(ImGuiMouseButton(button));
}

bool input::c_input_controller::click_released(mouse_buttons button)
{
	return ImGui::IsMouseReleased(ImGuiMouseButton(button));
}

float input::c_input_controller::get_wheel_value()
{
	return ImGui::GetIO().MouseWheel;
}

/* layering system */
void c_render::begin_layers()
{
}

void c_render::set_layer(engine::render_layer)
{
}

void c_render::end_layers()
{
}

void c_render::rect_shadow(int x, int y, int w, int h, hue::c_color col, float thickness, float rouding)
{
	(void)thickness;
	this->rect(x - 1, y - 1, w + 2, h + 2, col.modulate_normal((std::min)(col.a, 80)), rouding, 1.f);
}

void c_render::rect_filled(int x, int y, int w, int h, hue::c_color col, float rounding, engine::draw_flags flags)
{
	this->m_draw_list->AddRectFilled(ImVec2(static_cast<float>(x), static_cast<float>(y)), ImVec2(static_cast<float>(x + w), static_cast<float>(y + h)),
		col.transform(),
		rounding, flags
	);
}

void c_render::rect(int x, int y, int w, int h, hue::c_color col, float rounding, float thickness)
{
	this->m_draw_list->AddRect(ImVec2(static_cast<float>(x), static_cast<float>(y)), ImVec2(static_cast<float>(x + w), static_cast<float>(y + h)),
		col.transform(), rounding, 0, thickness
	);
}

void c_render::image(int x, int y, int w, int h, ImTextureID texture_id, hue::c_color col, float rounding)
{
	if (rounding > 0.f)
	{
		this->m_draw_list->AddImageRounded(texture_id, ImVec2(static_cast<float>(x), static_cast<float>(y)), ImVec2(static_cast<float>(x + w), static_cast<float>(y + h)), ImVec2(0.f, 0.f), ImVec2(1.f, 1.f),
			col.transform(),
			rounding
		);
	}
	else
	{
		this->m_draw_list->AddImage(texture_id, ImVec2(static_cast<float>(x), static_cast<float>(y)), ImVec2(static_cast<float>(x + w), static_cast<float>(y + h)), ImVec2(0.f, 0.f), ImVec2(1.f, 1.f),
			col.transform()
		);
	}
}

void c_render::set_linear_alpha(int vert_start_idx, int vert_end_idx, ImVec2 gradient_p0, ImVec2 gradient_p1, ImU32 col0, ImU32 col1)
{
	ImVec2 gradient_extent = gradient_p1 - gradient_p0;
	float gradient_inv_length2 = 1.0f / ImLengthSqr(gradient_extent);
	ImDrawVert* vert_start = this->m_draw_list->VtxBuffer.Data + vert_start_idx;
	ImDrawVert* vert_end = this->m_draw_list->VtxBuffer.Data + vert_end_idx;
	const int col0_r = (int)(col0 >> IM_COL32_R_SHIFT) & 0xFF;
	const int col0_g = (int)(col0 >> IM_COL32_G_SHIFT) & 0xFF;
	const int col0_b = (int)(col0 >> IM_COL32_B_SHIFT) & 0xFF;
	const int col0_a = (int)(col0 >> IM_COL32_A_SHIFT) & 0xFF;
	const int col_delta_r = ((int)(col1 >> IM_COL32_R_SHIFT) & 0xFF) - col0_r;
	const int col_delta_g = ((int)(col1 >> IM_COL32_G_SHIFT) & 0xFF) - col0_g;
	const int col_delta_b = ((int)(col1 >> IM_COL32_B_SHIFT) & 0xFF) - col0_b;
	const int col_delta_a = ((int)(col1 >> IM_COL32_A_SHIFT) & 0xFF) - col0_a;
	for (ImDrawVert* vert = vert_start; vert < vert_end; vert++)
	{
		float d = ImDot(vert->pos - gradient_p0, gradient_extent);
		float t = ImClamp(d * gradient_inv_length2, 0.0f, 1.0f);
		int r = (int)(col0_r + col_delta_r * t);
		int g = (int)(col0_g + col_delta_g * t);
		int b = (int)(col0_b + col_delta_b * t);
		int a = (int)(col0_a + col_delta_a * t);
		vert->col = (r << IM_COL32_R_SHIFT) | (g << IM_COL32_G_SHIFT) | (b << IM_COL32_B_SHIFT) | (a << IM_COL32_A_SHIFT);
	}
}


void c_render::fade_rect(int x, int y, int w, int h, hue::c_color col, hue::c_color col2, engine::fade_direction direction, float rounding, float thickness)
{
	int vtx_buffer_start = this->m_draw_list->VtxBuffer.Size;
	this->m_draw_list->AddRect(ImVec2(static_cast<float>(x), static_cast<float>(y)), ImVec2(static_cast<float>(x + w), static_cast<float>(y + h)),
		col.transform(), rounding, 0, thickness
	);
	int vtx_buffer_end = this->m_draw_list->VtxBuffer.Size;

	const ImVec2 fade_pos_in = (direction == engine::fade_direction::diagonally_reversed) ? ImVec2(x + w, y) : ImVec2(x, y);
	const ImVec2 fade_pos_out = (direction == engine::fade_direction::vertically) ? ImVec2(x, y + h) :
		(direction == engine::fade_direction::horizontally) ? ImVec2(x + w, y) :
		(direction == engine::fade_direction::diagonally) ? ImVec2(x + w, y + h) :
		(direction == engine::fade_direction::diagonally_reversed) ? ImVec2(x, y + h) : ImVec2(0, 0);

	this->set_linear_alpha(
		vtx_buffer_start,
		vtx_buffer_end,
		fade_pos_in, fade_pos_out,
		col.transform(),
		col2.transform()
	);
}

void c_render::fade_rect_filled(int x, int y, int w, int h, hue::c_color col1, hue::c_color col2, engine::fade_direction direction, float rounding, engine::draw_flags flags)
{
	switch (direction)
	{
	case engine::vertically:
		this->rect_filled_multi_color(x,y,w,h, { col1, col1, col2, col2 }, rounding, flags);
		break;
	case engine::horizontally:
		this->rect_filled_multi_color(x, y, w, h, { col1, col2, col1, col2 }, rounding, flags);
		break;
	case engine::diagonally:
		this->rect_filled_multi_color(x, y, w, h, { col1, col2, col2, col1 }, rounding, flags);
		break;
	case engine::diagonally_reversed:
		this->rect_filled_multi_color(x, y, w, h, { col2, col1, col1, col2 }, rounding, flags);
		break;
	default:
		break;
	}
}

void c_render::radial_gradient(int x, int y, int w, int h, float radius, hue::c_color col1, hue::c_color col2)
{
	this->m_draw_list->_PathArcToFastEx(ImVec2(x, y), radius, 0, IM_DRAWLIST_ARCFAST_SAMPLE_MAX, 0);
	const int count = this->m_draw_list->_Path.Size - 1;

	unsigned int vtx_base = this->m_draw_list->_VtxCurrentIdx;
	this->m_draw_list->PrimReserve(count * 3, count + 1);

	const ImVec2 uv = this->m_draw_list->_Data->TexUvWhitePixel;
	this->m_draw_list->PrimWriteVtx(ImVec2(x, y), uv, col1.transform());
	for (int n = 0; n < count; n++)
		this->m_draw_list->PrimWriteVtx(this->m_draw_list->_Path[n], uv, col2.transform());

	for (int n = 0; n < count; n++)
	{
		this->m_draw_list->PrimWriteIdx((ImDrawIdx)(vtx_base));
		this->m_draw_list->PrimWriteIdx((ImDrawIdx)(vtx_base + 1 + n));
		this->m_draw_list->PrimWriteIdx((ImDrawIdx)(vtx_base + 1 + ((n + 1) % count)));
	}
	this->m_draw_list->_Path.Size = 0;
}


void c_render::enlarged_arrow(math::c_vector_2d pos, hue::c_color col, int dir, float scale)
{
	float thickness = ImMax(scale / 15.0f, 1.0f);
	scale -= thickness * 0.5f;
	pos += math::c_vector_2d(ImVec2(thickness * 0.25f, thickness * 0.25f).x, ImVec2(thickness * 0.25f, thickness * 0.25f).y);

	float third = scale / 3.0f;
	float bx = pos.x + third;
	float by = pos.y + scale - third * 0.5f;

	switch (dir) {
	case ImGuiDir_Down:
		this->m_draw_list->PathLineTo(ImVec2(bx - third, by - third));
		this->m_draw_list->PathLineTo(ImVec2(bx, by));
		this->m_draw_list->PathLineTo(ImVec2(bx + third, by - third));
		break;
	case ImGuiDir_Up:
		this->m_draw_list->PathLineTo(ImVec2(bx - third, by + third));
		this->m_draw_list->PathLineTo(ImVec2(bx, by));
		this->m_draw_list->PathLineTo(ImVec2(bx + third, by + third));
		break;
	case ImGuiDir_Left:
		this->m_draw_list->PathLineTo(ImVec2(bx + third, by - third));
		this->m_draw_list->PathLineTo(ImVec2(bx, by));
		this->m_draw_list->PathLineTo(ImVec2(bx + third, by + third));
		break;
	case ImGuiDir_Right:
		this->m_draw_list->PathLineTo(ImVec2(bx - third, by - third));
		this->m_draw_list->PathLineTo(ImVec2(bx, by));
		this->m_draw_list->PathLineTo(ImVec2(bx - third + third / 30, by + third));
		break;
	}

	this->m_draw_list->PathStroke(col.transform(), 0, thickness);
}

void c_render::radial_gradient_rect_filled(int x, int y, int w, int h, ImVec2 size, float rounding, hue::c_color col1, hue::c_color col2)
{
	ImDrawList* dl = this->m_draw_list;
	ImVec2 pos = ImVec2(x, y);
	ImVec2 max_pos = ImVec2(pos.x + size.x, pos.y + size.y);
	const ImU32 c1 = col1.transform();
	const ImU32 c2 = col2.transform();

	rounding = ImMin(rounding, ImMin(size.x, size.y) * 0.5f);

	auto lerp_color = [](ImU32 a, ImU32 b, float t) -> ImU32 {
		int ra = (a >> 0) & 0xFF, ga = (a >> 8) & 0xFF, ba = (a >> 16) & 0xFF, aa = (a >> 24) & 0xFF;
		int rb = (b >> 0) & 0xFF, gb = (b >> 8) & 0xFF, bb = (b >> 16) & 0xFF, ab = (b >> 24) & 0xFF;
		return IM_COL32(
			(int)(ra + (rb - ra) * t),
			(int)(ga + (gb - ga) * t),
			(int)(ba + (bb - ba) * t),
			(int)(aa + (ab - aa) * t)
		);
		};

	const int layers = 12;

	for (int i = 0; i < layers; ++i)
	{
		float t = (float)i / (float)(layers - 1);

		float offset_x = (size.x * 0.5f) * t;
		float offset_y = (size.y * 0.5f) * t;

		ImVec2 layer_pos = ImVec2(pos.x + offset_x, pos.y + offset_y);
		ImVec2 layer_max = ImVec2(max_pos.x - offset_x, max_pos.y - offset_y);
		ImVec2 layer_size = ImVec2(layer_max.x - layer_pos.x, layer_max.y - layer_pos.y);

		if (layer_size.x <= 0.0f || layer_size.y <= 0.0f)
			break;

		float layer_rounding = ImMin(rounding * (1.0f - t), ImMin(layer_size.x, layer_size.y) * 0.5f);

		dl->AddRectFilled(layer_pos, layer_max, lerp_color(c2, c1, t), layer_rounding);
	}
}

// hue::c_color top_r, hue::c_color top_l, hue::c_color bottom_r, hue::c_color bottom_l
void c_render::rect_filled_multi_color(int x, int y, int w, int h, engine::c_gradient_data gradient_data, float rounding, engine::draw_flags flags)
{
	auto fix_rect_corner_flags = [](engine::draw_flags rflags)
		{
			if ((rflags & engine::draw_flags_round_corners_mask) == 0)
				rflags |= engine::draw_flags_round_corners_all;
			return rflags;
		};

	auto p_min = ImVec2(x, y);
	auto p_max = (ImVec2(x, y) + ImVec2(w, h));

	flags = fix_rect_corner_flags(flags);
	rounding = ImMin(rounding, ImFabs(p_max.x - p_min.x) * (((flags & engine::draw_flags_round_corners_top) == engine::draw_flags_round_corners_top) || ((flags & engine::draw_flags_round_corners_bottom) == engine::draw_flags_round_corners_bottom) ? 0.5f : 1.0f) - 1.0f);
	rounding = ImMin(rounding, ImFabs(p_max.y - p_min.y) * (((flags & engine::draw_flags_round_corners_left) == engine::draw_flags_round_corners_left) || ((flags & engine::draw_flags_round_corners_right) == engine::draw_flags_round_corners_right) ? 0.5f : 1.0f) - 1.0f);

	if (rounding > 0.0f)
	{
		const int size_before = this->m_draw_list->VtxBuffer.Size;
		this->m_draw_list->AddRectFilled(p_min, p_max, IM_COL32_WHITE, rounding, flags);
		const int size_after = this->m_draw_list->VtxBuffer.Size;

		for (int i = size_before; i < size_after; i++)
		{
			ImDrawVert* vert = this->m_draw_list->VtxBuffer.Data + i;

			ImVec4 upr_left = ImGui::ColorConvertU32ToFloat4(gradient_data.m_top_l.transform());
			ImVec4 bot_left = ImGui::ColorConvertU32ToFloat4(gradient_data.m_bot_l.transform());
			ImVec4 up_right = ImGui::ColorConvertU32ToFloat4(gradient_data.m_top_r.transform());
			ImVec4 bot_right = ImGui::ColorConvertU32ToFloat4(gradient_data.m_bot_r.transform());

			float X = ImClamp((vert->pos.x - p_min.x) / (p_max.x - p_min.x), 0.0f, 1.0f);

			// 4 colors - 8 deltas

			float r1 = upr_left.x + (up_right.x - upr_left.x) * X;
			float r2 = bot_left.x + (bot_right.x - bot_left.x) * X;

			float g1 = upr_left.y + (up_right.y - upr_left.y) * X;
			float g2 = bot_left.y + (bot_right.y - bot_left.y) * X;

			float b1 = upr_left.z + (up_right.z - upr_left.z) * X;
			float b2 = bot_left.z + (bot_right.z - bot_left.z) * X;

			float a1 = upr_left.w + (up_right.w - upr_left.w) * X;
			float a2 = bot_left.w + (bot_right.w - bot_left.w) * X;


			float Y = ImClamp((vert->pos.y - p_min.y) / (p_max.y - p_min.y), 0.0f, 1.0f);
			float r = r1 + (r2 - r1) * Y;
			float g = g1 + (g2 - g1) * Y;
			float b = b1 + (b2 - b1) * Y;
			float a = a1 + (a2 - a1) * Y;
			ImVec4 RGBA(r, g, b, a);

			RGBA = RGBA * ImGui::ColorConvertU32ToFloat4(vert->col);

			vert->col = ImColor(RGBA);
		}
		return;
	}

	const ImVec2 uv = this->m_draw_list->_Data->TexUvWhitePixel;
	this->m_draw_list->PrimReserve(6, 4);
	this->m_draw_list->PrimWriteIdx((ImDrawIdx)(this->m_draw_list->_VtxCurrentIdx)); this->m_draw_list->PrimWriteIdx((ImDrawIdx)(this->m_draw_list->_VtxCurrentIdx + 1)); this->m_draw_list->PrimWriteIdx((ImDrawIdx)(this->m_draw_list->_VtxCurrentIdx + 2));
	this->m_draw_list->PrimWriteIdx((ImDrawIdx)(this->m_draw_list->_VtxCurrentIdx)); this->m_draw_list->PrimWriteIdx((ImDrawIdx)(this->m_draw_list->_VtxCurrentIdx + 2)); this->m_draw_list->PrimWriteIdx((ImDrawIdx)(this->m_draw_list->_VtxCurrentIdx + 3));


	this->m_draw_list->PrimWriteVtx(p_min, uv, gradient_data.m_top_l.transform());
	this->m_draw_list->PrimWriteVtx(ImVec2(p_max.x, p_min.y), uv, gradient_data.m_top_r.transform());
	this->m_draw_list->PrimWriteVtx(p_max, uv, gradient_data.m_bot_r.transform());
	this->m_draw_list->PrimWriteVtx(ImVec2(p_min.x, p_max.y), uv, gradient_data.m_bot_l.transform());
}


void c_render::check_mark(int x, int y, float size, hue::c_color col)
{
	float thickness = ImMax(size / 5.0f, 1.0f);
	size -= thickness * 0.5f;

	x += thickness * 0.25f;
	y += thickness * 0.25f;

	float third = size / 3.0f;
	float bx = x + third;
	float by = y + size - third * 0.5f;

	ImVec2 p0 = { bx - third,        by - third };
	ImVec2 p1 = { bx,                by };
	ImVec2 p2 = { bx + third * 2.f,  by - third * 2.f };

	float r = thickness * 0.5f;

	ImVec2 cp0 = ImVec2((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
	ImVec2 cp1 = ImVec2((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);

	this->m_draw_list->AddBezierCubic(p0, cp0, cp0, p1, col.transform(), thickness, 0);
	this->m_draw_list->AddBezierCubic(p1, cp1, cp1, p2, col.transform(), thickness, 0);

	this->m_draw_list->AddCircleFilled(p1, thickness * 0.5f, col.transform());

	this->m_draw_list->AddCircleFilled(p0, thickness * 0.5f, col.transform());
	this->m_draw_list->AddCircleFilled(p2, thickness * 0.5f, col.transform());
}

void c_render::push_clip(int x, int y, int w, int h)
{
	this->m_draw_list->PushClipRect(ImVec2(static_cast<float>(x), static_cast<float>(y)), ImVec2(static_cast<float>(x + w), static_cast<float>(y + h)), true);
}

void c_render::restore_clip()
{
	this->m_draw_list->PopClipRect();
}

void c_render::setup()
{
	slog::log::info("render.setup: skip drawlist until first frame");

	ImGuiIO& io = ImGui::GetIO();
	if (!io.Fonts)
	{
		slog::log::error("render.setup: io.Fonts is null");
		return;
	}

	ImFontConfig cfg{};
	cfg.FontDataOwnedByAtlas = false;

	slog::log::info("render.setup: add default font");
	g_font->f_default.create((void*)roboto_regular, (int)roboto_regular_size, 18.f, &cfg, NULL, "roboto_regular");
	slog::log::info("render.setup: add child font");
	g_font->f_childs.create((void*)roboto_regular, (int)roboto_regular_size, 17.f, &cfg, NULL, "f_childs");
	slog::log::info("render.setup: add bold font");
	g_font->f_bold.create((void*)roboto_bold, (int)roboto_bold_size, 17.f, &cfg, NULL, "roboto_bold");
	slog::log::info("render.setup: add icon fallback font");
	g_font->f_icons.create((void*)roboto_regular, (int)roboto_regular_size, 14.f, &cfg, NULL, "f_icons_fallback");

	slog::log::success("render engine has been initialized.");
}

bool c_font::create(const std::string& font_location, float size)
{
	ImGuiIO& io = ImGui::GetIO();
	this->m_handle = io.Fonts->AddFontFromFileTTF(font_location.c_str(), size);
	this->m_size = math::c_vector_2d(size * 0.55f, size);
	return this->m_handle != nullptr;
}

bool c_font::create(const std::string& font_location, float size, const ImFontConfig* font_template, const ImWchar* glyph)
{
	ImGuiIO& io = ImGui::GetIO();
	this->m_handle = io.Fonts->AddFontFromFileTTF(font_location.c_str(), size, font_template, glyph);
	this->m_size = math::c_vector_2d(size * 0.55f, size);
	return this->m_handle != nullptr;
}

bool c_font::create(void* data, int sizess, float size, std::string id, bool compresed)
{
	ImGuiIO& io = ImGui::GetIO();
	this->m_handle = compresed
		? io.Fonts->AddFontFromMemoryCompressedTTF(data, sizess, size)
		: io.Fonts->AddFontFromMemoryTTF(data, sizess, size);
	this->m_size = math::c_vector_2d(size * 0.55f, size);
	slog::log::debug("created font: {} ok={}", id.c_str(), this->m_handle != nullptr);
	return this->m_handle != nullptr;
}

bool c_font::create(void* data, int font_size, float size, const ImFontConfig* font_template, const ImWchar* glyph, std::string id, bool compresed)
{
	ImGuiIO& io = ImGui::GetIO();
	this->m_handle = compresed
		? io.Fonts->AddFontFromMemoryCompressedTTF(data, font_size, size, font_template, glyph)
		: io.Fonts->AddFontFromMemoryTTF(data, font_size, size, font_template, glyph);
	this->m_size = math::c_vector_2d(size * 0.55f, size);
	slog::log::debug("created font: {} ok={}", id.c_str(), this->m_handle != nullptr);
	return this->m_handle != nullptr;
}

math::c_vector_2d c_font::measure(const std::string& text)
{
	if (!this->m_handle)
		return math::c_vector_2d(8.f * static_cast<float>(text.size()), 16.f);

	const float size = this->m_handle->LegacySize > 0.f ? this->m_handle->LegacySize : 17.f;
	if (ImGui::GetCurrentContext() && ImGui::GetFrameCount() > 0)
	{
		auto wraper = this->m_handle->CalcTextSizeA(size, FLT_MAX, -1.0f, text.c_str());
		return math::c_vector_2d(wraper.x, wraper.y);
	}

	return math::c_vector_2d(size * 0.55f * static_cast<float>(text.size()), size);
}

void c_font::string(int x, int y, std::string text, hue::c_color color)
{
	if (!g_render->draw_list() || !this->m_handle)
		return;
	g_render->draw_list()->AddText(this->m_handle, this->m_handle->LegacySize > 0.f ? this->m_handle->LegacySize : 17.f,
		ImVec2(static_cast<float>(x), static_cast<float>(y)), color.transform(), text.c_str());
}

void c_render::gradient(math::c_vector_2d pos, math::c_vector_2d size, hue::c_color color, hue::c_color color2, engine::fade_direction flags, int rounding, hue::c_color, ImDrawFlags)
{
	gradient((int)pos.x, (int)pos.y, (int)size.x, (int)size.y, color, color2, flags, rounding);
}

void c_render::gradient(int x, int y, int w, int h, hue::c_color color, hue::c_color color2, engine::fade_direction flags, int rounding, hue::c_color, ImDrawFlags)
{
	(void)rounding;
	const ImVec2 start((float)x, (float)y);
	const ImVec2 end((float)(x + w), (float)(y + h));
	const ImU32 c0 = color.transform();
	const ImU32 c1 = color2.transform();
	if (flags == engine::fade_direction::vertically)
		this->m_draw_list->AddRectFilledMultiColor(start, end, c0, c0, c1, c1);
	else
		this->m_draw_list->AddRectFilledMultiColor(start, end, c0, c1, c1, c0);
}
