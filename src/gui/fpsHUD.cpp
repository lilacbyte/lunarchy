#include "gui/fpsHUD.h"

#include "gui/moduleColor.h"
#include "util/string.h"

#include <algorithm>
#include <cmath>

FpsHUD::FpsHUD(const core::rect<s32> &rect) : CheatUIElement(rect) {}

void FpsHUD::draw(video::IVideoDriver *driver, gui::IGUIFont *font, float dtime,
	ClientEnvironment &env, bool editing)
{
	(void)env;
	if (!hudShouldRender(editing))
		return;

	if (!g_settings->getBool("fps") && !editing)
		return;
	if (!font)
		return;

	m_sample_timer += dtime;
	++m_sample_frames;
	if (editing || m_cached_text == L"fps: --" || m_sample_timer >= 0.5f) {
		const float fps = m_sample_timer > 0.0001f ? m_sample_frames / m_sample_timer : 0.0f;
		m_cached_text = L"fps: " + utf8_to_wide(itos(static_cast<int>(std::round(fps))));
		m_sample_timer = 0.0f;
		m_sample_frames = 0;
	}
	const std::wstring &text = m_cached_text;
	const core::dimension2d<u32> dim_u32 = font->getDimension(text.c_str());
	const core::dimension2d<s32> dim(dim_u32.Width, dim_u32.Height);

	const core::rect<s32> draw_bounds = fitModuleHudBounds(*this, dim.Width, dim.Height);

	const bool draw_background = editing || g_settings->getBool("fps.background");
	if (draw_background) {
		driver->draw2DRectangle(readModuleBackgroundColor(), draw_bounds);
		driver->draw2DRectangleOutline(draw_bounds, readModuleBorderColor(), 2);
	}

	const s32 padding = moduleHudPadding();
	const s32 x = draw_bounds.UpperLeftCorner.X + padding;
	const s32 y = draw_bounds.UpperLeftCorner.Y + padding;
	font->draw(text.c_str(), core::rect<s32>(x, y, x + dim.Width, y + dim.Height), readModuleTextColor());
}
