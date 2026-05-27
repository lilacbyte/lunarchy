#include "gui/cheatHUD.h"

#include "client/fontengine.h"
#include "gui/moduleColor.h"
#include "script/scripting_client.h"
#include "util/string.h"
#include <algorithm>
#include <cctype>
#include <tuple>

namespace {
static std::vector<std::string> collectEnabledCheatTexts(Client *client)
{
	std::vector<std::string> enabled_cheats;
	ClientScripting *script = client ? client->getScript() : nullptr;
	if (!script || !script->m_cheats_loaded)
		return enabled_cheats;

	for (auto category = script->m_cheat_categories.begin();
			category != script->m_cheat_categories.end(); ++category) {
		for (auto cheat = (*category)->m_cheats.begin();
				cheat != (*category)->m_cheats.end(); ++cheat) {
			if (!(*cheat)->is_enabled())
				continue;

			std::string cheat_str = (*cheat)->m_name;
			std::string info_text = (*cheat)->get_info_text();
			if (!info_text.empty())
				cheat_str += " [" + info_text + "]";
			enabled_cheats.push_back(cheat_str);
		}
	}

	return enabled_cheats;
}

static bool hasStoredBounds()
{
	return g_settings->exists("HudElement_Position1_cheathud") &&
		g_settings->exists("HudElement_Position2_cheathud");
}

static std::string getHudTextAlign()
{
	if (g_settings->exists("cheat_hud.align"))
		return g_settings->get("cheat_hud.align");

	// Legacy fallback for configs saved before the alignment rename.
	if (g_settings->exists("cheat_hud.position"))
		return "Right";

	return "Right";
}

static std::string getCheatHudOrder()
{
	std::string order = g_settings->get("cheat_hud.order");
	std::transform(order.begin(), order.end(), order.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return order;
}

static core::rect<s32> getStoredBounds()
{
	v2f position1 = g_settings->getV2F("HudElement_Position1_cheathud");
	v2f position2 = g_settings->getV2F("HudElement_Position2_cheathud");
	return core::rect<s32>(position1.X, position1.Y, position2.X, position2.Y);
}

static core::rect<s32> getHudBounds(video::IVideoDriver *driver, Client *client,
	gui::IGUIFont *font)
{
	if (hasStoredBounds())
		return getStoredBounds();

	std::vector<std::string> enabled_cheats = collectEnabledCheatTexts(client);
	const core::dimension2d<u32> screensize = driver->getScreenSize();

	s32 max_width = 0;
	s32 line_height = 0;
	for (const auto &cheat : enabled_cheats) {
		const core::dimension2d<u32> dim = font->getDimension(utf8_to_wide(cheat).c_str());
		max_width = std::max(max_width, static_cast<s32>(dim.Width));
		line_height = std::max(line_height, static_cast<s32>(dim.Height));
	}

	if (line_height <= 0)
		line_height = font ? static_cast<s32>(font->getDimension(L"M").Height) : 16;
	if (max_width <= 0)
		max_width = 120;

	const s32 padding = 10;
	const s32 width = max_width + padding;
	const s32 line_count = std::max<s32>(static_cast<s32>(enabled_cheats.size()), 1);
	const s32 height = std::max(line_height * line_count, line_height) + padding;
	s32 offset = 0;
	g_settings->getS32NoEx("cheat_hud.offset", offset);
	const std::string align = getHudTextAlign();
	s32 x = 5;
	if (align == "Center") {
		x = std::max<s32>(5, static_cast<s32>(screensize.Width / 2) - width / 2);
	} else if (align == "Right") {
		x = screensize.Width > static_cast<u32>(width + 5) ?
			static_cast<s32>(screensize.Width) - width - 5 : 5;
	}
	s32 y = 5 + offset;

	return core::rect<s32>(x, y, x + width, y + height);
}

static s32 getAlignedTextX(const core::rect<s32> &bounds, s32 text_width,
	const std::string &align, s32 padding)
{
	const s32 inner_left = bounds.UpperLeftCorner.X + padding;
	const s32 inner_width = std::max<s32>(bounds.getWidth() - padding * 2, 0);

	if (align == "Center")
		return inner_left + std::max<s32>(0, (inner_width - text_width) / 2);
	if (align == "Right")
		return inner_left + std::max<s32>(0, inner_width - text_width);
	return inner_left;
}
} // namespace

CheatHUD::CheatHUD(Client *client, const core::rect<s32> &rect) :
	CheatUIElement(rect), m_client(client)
{
}

void CheatHUD::draw(video::IVideoDriver *driver, gui::IGUIFont *font, float dtime,
	ClientEnvironment &env, bool editing)
{
	(void)env;
	(void)dtime;
	if (!hudShouldRender(editing))
		return;

	if (!g_settings->getBool("cheat_hud") && !editing)
		return;
	if (!font)
		return;

	ClientScripting *script = m_client ? m_client->getScript() : nullptr;
	const u32 cheat_revision = script ? script->get_cheat_state_revision() : 0;
	m_cache_timer += dtime;
	if (editing || !m_cache_valid || cheat_revision != m_cached_cheat_revision ||
			m_cache_timer >= 0.35f) {
		m_cached_enabled_cheats = collectEnabledCheatTexts(m_client);
		m_cache_valid = true;
		m_cached_cheat_revision = cheat_revision;
		m_cache_timer = 0.0f;
	}

	std::vector<std::string> enabled_cheats = m_cached_enabled_cheats;
	if (enabled_cheats.empty() && !editing)
		return;

	const video::SColor hud_color = readHudAccentColor(readModuleTextColor());
	const video::SColor infoColor(230, 230, 230, 230);
	const std::string align = getHudTextAlign();
	const s32 line_padding = 5;

	std::sort(enabled_cheats.begin(), enabled_cheats.end(),
		[font](const std::string &a, const std::string &b) {
			const std::string order = getCheatHudOrder();
			const bool by_length = g_settings->getBool("cheat_hud.by_length");
			const s32 width_a = static_cast<s32>(font->getDimension(utf8_to_wide(a).c_str()).Width);
			const s32 width_b = static_cast<s32>(font->getDimension(utf8_to_wide(b).c_str()).Width);
			if (order == "ascending")
				return by_length ? width_a < width_b : a < b;
			if (order == "descending")
				return by_length ? width_a > width_b : a > b;
			if (width_a == width_b)
				return a < b;
			return width_a < width_b;
		});

	if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
		bounds = getHudBounds(driver, m_client, font);
	s32 max_width = 0;
	s32 total_height = 0;
	for (const auto &text : enabled_cheats) {
		const auto dim = font->getDimension(utf8_to_wide(text).c_str());
		max_width = std::max(max_width, static_cast<s32>(dim.Width));
		total_height += static_cast<s32>(dim.Height);
	}
	if (enabled_cheats.empty()) {
		const auto dim = font->getDimension(L"cheathud");
		max_width = dim.Width;
		total_height = dim.Height;
	}
	const s32 anchored_right = bounds.LowerRightCorner.X;
	core::rect<s32> draw_bounds = fitModuleHudBounds(*this, max_width, total_height, line_padding);
	if (align == "Right") {
		const s32 dx = anchored_right - draw_bounds.LowerRightCorner.X;
		bounds += core::position2d<s32>(dx, 0);
		resizeBounds += core::position2d<s32>(dx, 0);
		draw_bounds += core::position2d<s32>(dx, 0);
	}
	if (draw_bounds.UpperLeftCorner.X < 0)
		draw_bounds.UpperLeftCorner.X = 0;
	if (draw_bounds.UpperLeftCorner.Y < 0)
		draw_bounds.UpperLeftCorner.Y = 0;

	if (editing || g_settings->getBool("cheat_hud.background")) {
		driver->draw2DRectangle(readModuleBackgroundColor(), draw_bounds);
		driver->draw2DRectangleOutline(draw_bounds, readModuleBorderColor(), 2);
	}

	s32 y = draw_bounds.UpperLeftCorner.Y + line_padding;
	for (size_t i = 0; i < enabled_cheats.size(); ++i) {
		const video::SColor line_color = hud_color;
		std::string cheat_full_str = enabled_cheats[i];
		size_t brace_position = cheat_full_str.find('[');
		if (brace_position != std::string::npos) {
			std::string cheat_str = cheat_full_str.substr(0, brace_position);
			std::string info_str = cheat_full_str.substr(brace_position);

			core::dimension2d<u32> cheat_dim = font->getDimension(utf8_to_wide(cheat_str).c_str());
			core::dimension2d<u32> info_dim = font->getDimension(utf8_to_wide(info_str).c_str());
			const s32 line_width = static_cast<s32>(cheat_dim.Width + info_dim.Width);
			const s32 line_height = static_cast<s32>(std::max(cheat_dim.Height, info_dim.Height));
			s32 x_cheat = getAlignedTextX(draw_bounds, line_width, align, line_padding);
			s32 x_info = x_cheat + static_cast<s32>(cheat_dim.Width);
			const s32 line_top = y;
			core::rect<s32> cheat_bounds(x_cheat, line_top, x_cheat + cheat_dim.Width, line_top + cheat_dim.Height);
			core::rect<s32> info_bounds(x_info, line_top, x_info + info_dim.Width, line_top + info_dim.Height);
			font->draw(cheat_str.c_str(), cheat_bounds, line_color, false, false);
			font->draw(info_str.c_str(), info_bounds, infoColor, false, false);
			y += line_height;
		} else {
			core::dimension2d<u32> cheat_dim = font->getDimension(utf8_to_wide(cheat_full_str).c_str());
			s32 x = getAlignedTextX(draw_bounds, static_cast<s32>(cheat_dim.Width), align, line_padding);
			const s32 line_height = static_cast<s32>(cheat_dim.Height);
			const s32 line_top = y;
			core::rect<s32> text_bounds(x, line_top, x + cheat_dim.Width, line_top + cheat_dim.Height);
			font->draw(cheat_full_str.c_str(), text_bounds, line_color, false, false);
			y += line_height;
		}
	}
}
