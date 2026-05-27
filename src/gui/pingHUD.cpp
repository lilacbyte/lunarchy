#include "gui/pingHUD.h"

#include "gui/moduleColor.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace {
static std::wstring buildPingText(Client *client)
{
	if (!client)
		return L"ping: -- ms";

	const float rtt_ms = std::max(0.0f, client->getRTT() * 1000.0f);
	std::wstringstream wss;
	wss << L"ping: " << std::fixed << std::setprecision(0) << rtt_ms << L" ms";
	return wss.str();
}
} // namespace

pingHUD::pingHUD(Client *client, const core::rect<s32> &rect) : CheatUIElement(rect), m_client(client) {}

void pingHUD::draw(video::IVideoDriver *driver, gui::IGUIFont *font, float dtime,
	ClientEnvironment &env, bool editing)
{
	(void)env;
	if (!hudShouldRender(editing))
		return;

	if (!g_settings->getBool("ping") && !editing)
		return;
	if (!font)
		return;

	m_cache_timer += dtime;
	if (editing || m_cache_timer >= 0.5f || m_cached_text == L"ping: -- ms") {
		m_cached_text = buildPingText(m_client);
		m_cache_timer = 0.0f;
	}
	const std::wstring &text = m_cached_text;
	const core::dimension2d<u32> dim_u32 = font->getDimension(text.c_str());
	const core::dimension2d<s32> dim(dim_u32.Width, dim_u32.Height);

	const core::rect<s32> draw_bounds = fitModuleHudBounds(*this, dim.Width, dim.Height);

	const video::SColor outline_color = readModuleBorderColor();
	const video::SColor background_color = readModuleBackgroundColor();
	const video::SColor text_color = readModuleTextColor();

	const bool draw_background = editing || g_settings->getBool("ping.background");
	if (draw_background) {
		driver->draw2DRectangle(background_color, draw_bounds);
		driver->draw2DRectangleOutline(draw_bounds, outline_color, 2);
	}

	const s32 padding = moduleHudPadding();
	const s32 x = draw_bounds.UpperLeftCorner.X + padding;
	const s32 y = draw_bounds.UpperLeftCorner.Y + padding;
	font->draw(text.c_str(), core::rect<s32>(x, y, x + dim.Width, y + dim.Height), text_color);
}
