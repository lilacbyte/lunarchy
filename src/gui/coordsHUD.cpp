#include "gui/coordsHUD.h"

#include "gui/moduleColor.h"

#include <algorithm>
#include <vector>

coordsHUD::coordsHUD(const core::rect<s32>& rect) : CheatUIElement(rect) {}

namespace {
static std::vector<std::wstring> buildCoordsLines(const v3f &coords)
{
	std::vector<std::wstring> lines;
	std::wstringstream wss;
	wss << std::fixed << std::setprecision(1);
	wss << L"X: " << coords.X / 10 << L" Y: " << coords.Y / 10 << L" Z: " << coords.Z / 10;
	lines.push_back(wss.str());

	if (g_settings->getBool("coords.nether_coords")) {
		std::wstringstream nss;
		nss << std::fixed << std::setprecision(1);
		nss << L"Nether X: " << (coords.X / 10) / 8.0f
			<< L" Y: " << coords.Y / 10
			<< L" Z: " << (coords.Z / 10) / 8.0f;
		lines.push_back(nss.str());
	}

	return lines;
}
}

void coordsHUD::draw(video::IVideoDriver* driver, gui::IGUIFont* font, float dtime, ClientEnvironment &env, bool editing) {
    if (!hudShouldRender(editing))
        return;

    if (g_settings->getBool("coords") || editing) {
		if (!font)
			return;
        const video::SColor outline_color = readModuleBorderColor();
        const video::SColor background_color = readModuleBackgroundColor();
        const video::SColor text_color = readModuleTextColor();

		LocalPlayer *player = env.getLocalPlayer();
		if (!player)
			return;
		m_cache_timer += dtime;
		if (editing || !m_cache_valid || m_cache_timer >= 0.1f) {
			m_cached_lines = buildCoordsLines(player->getPosition());
			m_cache_valid = true;
			m_cache_timer = 0.0f;
		}
		const std::vector<std::wstring> &lines = m_cached_lines;
		s32 total_height = 0;
		s32 max_width = 0;
		for (const auto &line : lines) {
			const core::dimension2d<u32> dim_u32 = font->getDimension(line.c_str());
			max_width = std::max(max_width, static_cast<s32>(dim_u32.Width));
			total_height += static_cast<s32>(dim_u32.Height);
		}
		const core::rect<s32> draw_bounds = fitModuleHudBounds(*this, max_width, total_height);
        if (editing || g_settings->getBool("coords.background")) {
            driver->draw2DRectangle(background_color, draw_bounds);
            driver->draw2DRectangleOutline(draw_bounds, outline_color, 2);
        }
		const s32 padding = moduleHudPadding();
		s32 textX = draw_bounds.UpperLeftCorner.X + padding;
		s32 textY = draw_bounds.UpperLeftCorner.Y + padding;
		for (const auto &line : lines) {
			const core::dimension2d<u32> dim_u32 = font->getDimension(line.c_str());
			const core::dimension2d<s32> dim(dim_u32.Width, dim_u32.Height);
			font->draw(line.c_str(), core::rect<s32>(textX, textY, textX + dim.Width, textY + dim.Height), text_color);
			textY += dim.Height;
		}
    }
}
