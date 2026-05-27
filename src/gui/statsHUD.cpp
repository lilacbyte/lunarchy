#include "gui/statsHUD.h"

#include "client/localplayer.h"
#include "client/playerstats.h"
#include "gui/moduleColor.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>

StatsHUD::StatsHUD(const core::rect<s32> &rect) : CheatUIElement(rect) {}

namespace {

static std::wstring formatJoinedDate(uint64_t milliseconds)
{
	const std::time_t timestamp = static_cast<std::time_t>(milliseconds / 1000);
	std::tm date{};
#ifdef _WIN32
	gmtime_s(&date, &timestamp);
#else
	gmtime_r(&timestamp, &date);
#endif
	std::wostringstream text;
	text << std::setfill(L'0') << std::setw(2) << date.tm_mon + 1
		 << L"/" << std::setw(2) << date.tm_mday
		 << L"/" << date.tm_year + 1900;
	return text.str();
}

static std::vector<std::wstring> buildLines(const std::optional<player_stats::Stats> &stats)
{
	std::vector<std::wstring> lines;
	if (!stats)
		return lines;

	if (g_settings->getBool("luna_stats.hud.pvp"))
		lines.push_back(player_stats::formatPvpLine(stats->kills, stats->deaths));
	if (g_settings->getBool("luna_stats.hud.messages"))
		lines.push_back(L"messages: " + std::to_wstring(stats->messages));
	if (g_settings->getBool("luna_stats.hud.joined"))
		lines.push_back(L"joined: " + formatJoinedDate(stats->first_ts));
	if (g_settings->getBool("luna_stats.hud.joins"))
		lines.push_back(L"joins: " + std::to_wstring(stats->joins));
	if (g_settings->getBool("luna_stats.hud.leaves"))
		lines.push_back(L"leaves: " + std::to_wstring(stats->leaves));
	return lines;
}

} // namespace

void StatsHUD::draw(video::IVideoDriver *driver, gui::IGUIFont *font, float dtime,
	ClientEnvironment &env, bool editing)
{
	const bool enabled = g_settings->getBool("luna_stats.enabled");
	const bool hud_enabled = enabled && g_settings->getBool("luna_stats.hud");
	if (!hudShouldRender(editing) || (!hud_enabled && !editing) || !font)
		return;

	m_cache_timer += dtime;
	if (editing || !m_cache_valid || m_cache_timer >= 0.5f) {
		LocalPlayer *player = env.getLocalPlayer();
		std::optional<player_stats::Stats> stats;
		if (hud_enabled && player)
			stats = player_stats::getStats(player->getName());
		m_cached_lines = buildLines(stats);
		m_cache_valid = true;
		m_cache_timer = 0.0f;
	}
	std::vector<std::wstring> lines = m_cached_lines;
	if (lines.empty() && editing)
		lines.emplace_back(L"stats");
	if (lines.empty())
		return;

	s32 max_width = 0;
	s32 total_height = 0;
	for (const auto &line : lines) {
		const core::dimension2d<u32> size = font->getDimension(line.c_str());
		max_width = std::max(max_width, static_cast<s32>(size.Width));
		total_height += static_cast<s32>(size.Height);
	}

	const core::rect<s32> draw_bounds = fitModuleHudBounds(*this, max_width, total_height);
	if (editing || g_settings->getBool("luna_stats.hud.background")) {
		driver->draw2DRectangle(readModuleBackgroundColor(), draw_bounds);
		driver->draw2DRectangleOutline(draw_bounds, readModuleBorderColor(), 2);
	}

	const s32 padding = moduleHudPadding();
	const s32 x = draw_bounds.UpperLeftCorner.X + padding;
	s32 y = draw_bounds.UpperLeftCorner.Y + padding;
	const video::SColor text_color = readModuleTextColor();
	for (const auto &line : lines) {
		const core::dimension2d<u32> size = font->getDimension(line.c_str());
		font->draw(line.c_str(), core::rect<s32>(x, y, x + size.Width, y + size.Height), text_color);
		y += static_cast<s32>(size.Height);
	}
}
