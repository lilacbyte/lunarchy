#include "gui/clientsHUD.h"

#include "gui/moduleColor.h"
#include "client/content_cao.h"
#include "client/client.h"
#include "client/localplayer.h"
#include "util/string.h"
#include <cctype>
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

namespace {
static std::string getClientsMode()
{
	std::string mode = g_settings->exists("clients.mode") ? g_settings->get("clients.mode") : "both";
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return mode;
}

static float getNearbyRange()
{
	return g_settings->getFloat("nearby_clients.range", 16.0f, 512.0f);
}

static bool getNearbyDistanceEnabled()
{
	if (g_settings->exists("clients.nearby_distance"))
		return g_settings->getBool("clients.nearby_distance");
	return g_settings->getBool("nearby_clients.distance");
}

static bool getNearbyHealthEnabled()
{
	if (g_settings->exists("clients.nearby_health"))
		return g_settings->getBool("clients.nearby_health");
	return g_settings->getBool("nearby_clients.health");
}

static std::vector<std::wstring> buildOnlineLines(ClientEnvironment &env)
{
	std::vector<std::wstring> lines;
	const std::set<std::string> *names = nullptr;
	Client *client = env.getGameDef();
	if (client)
		names = &client->getConnectedPlayerNames();
	if (!names)
		names = &env.getPlayerNames();

	std::wstringstream header;
	header << L"online (" << names->size() << L")";
	if (names->empty())
		return lines;

	lines.push_back(header.str());

	for (const std::string &name : *names) {
		std::wstringstream line;
		line << utf8_to_wide(name);
		lines.push_back(line.str());
	}

	return lines;
}

static std::vector<std::wstring> buildNearbyLines(ClientEnvironment &env, float radius)
{
	struct Entry {
		std::wstring text;
		float distance;
	};

	std::vector<Entry> entries;
	LocalPlayer *lp = env.getLocalPlayer();
	if (!lp)
		return {};

	const v3f origin = lp->getPosition();
	std::vector<DistanceSortedActiveObject> objects;
	env.getAllActiveObjects(origin, objects);

	for (const auto &sortedObj : objects) {
		auto *obj = dynamic_cast<GenericCAO *>(sortedObj.obj);
		if (!obj || !obj->isPlayer() || obj->isLocalPlayer())
			continue;

		const float dist_blocks = origin.getDistanceFrom(obj->getPosition()) / BS;
		if (dist_blocks > radius)
			continue;

		std::wstringstream line;
		line << utf8_to_wide(obj->getName());
		if (getNearbyDistanceEnabled())
			line << L" (" << static_cast<int>(std::round(dist_blocks)) << L"m away)";
		if (getNearbyHealthEnabled()) {
			const u16 hp = obj->getHp();
			const u16 hp_max = obj->getProperties().hp_max;
			if (hp_max > 0)
				line << L" (" << hp << L"/" << hp_max << L")";
		}
		entries.push_back({line.str(), dist_blocks});
	}

	std::sort(entries.begin(), entries.end(), [](
		const auto &a, const auto &b) { return a.distance < b.distance; });

	std::vector<std::wstring> lines;
	if (entries.empty())
		return lines;

	std::wstringstream header;
	header << L"nearby (" << entries.size() << L")";
	lines.push_back(header.str());

	for (const auto &entry : entries)
		lines.push_back(entry.text);

	return lines;
}

static void appendSection(std::vector<std::wstring> &lines, const std::vector<std::wstring> &section)
{
	if (section.empty())
		return;

	if (!lines.empty())
		lines.emplace_back(L"");
	lines.insert(lines.end(), section.begin(), section.end());
}
} // namespace

clientsHUD::clientsHUD(const core::rect<s32> &rect, Section section) :
	CheatUIElement(rect),
	m_section(section)
{}

std::vector<std::wstring> clientsHUD::buildLines(ClientEnvironment &env) const
{
	const std::string mode = getClientsMode();
	const float nearby_range = getNearbyRange();
	std::vector<std::wstring> lines;

	if (m_section == Section::Online && (mode == "online" || mode == "both"))
		appendSection(lines, buildOnlineLines(env));
	if (m_section == Section::Nearby && (mode == "nearby" || mode == "both"))
		appendSection(lines, buildNearbyLines(env, nearby_range));

	return lines;
}

core::rect<s32> clientsHUD::calculateBounds(video::IVideoDriver *driver,
	gui::IGUIFont *font, const std::vector<std::wstring> &lines) const
{
	const core::dimension2d<u32> screensize = driver->getScreenSize();
	s32 max_width = 0;
	s32 line_height = 0;

	for (const std::wstring &line : lines) {
		if (!font)
			break;
		const core::dimension2d<u32> dim = font->getDimension(line.c_str());
		max_width = std::max(max_width, static_cast<s32>(dim.Width));
		line_height = std::max(line_height, static_cast<s32>(dim.Height));
	}

	if (line_height <= 0)
		line_height = font ? static_cast<s32>(font->getDimension(L"M").Height) : 16;
	if (max_width <= 0)
		max_width = 140;

	const s32 padding = 10;
	const s32 width = max_width + padding;
	const s32 height = std::max<s32>(line_height * static_cast<s32>(lines.size()), line_height) + padding;
	const s32 x = screensize.Width > static_cast<u32>(width + 5)
		? static_cast<s32>(screensize.Width) - width - 5
		: 5;
	const s32 y = 5;

	return core::rect<s32>(x, y, x + width, y + height);
}

void clientsHUD::draw(video::IVideoDriver *driver, gui::IGUIFont *font, float dtime,
	ClientEnvironment &env, bool editing)
{
	(void)dtime;
	if (!hudShouldRender(editing))
		return;

	if (!g_settings->getBool("clients") && !editing)
		return;
	if (!font)
		return;

	m_cache_timer += dtime;
	if (editing || !m_cache_valid || m_cache_timer >= 0.35f) {
		m_cached_lines = buildLines(env);
		m_cache_valid = true;
		m_cache_timer = 0.0f;
	}

	const std::vector<std::wstring> lines = m_cached_lines;
	if (lines.empty() && !editing)
		return;

	s32 max_width = 0;
	s32 total_height = 0;
	for (const auto &line : lines) {
		const core::dimension2d<u32> dim = font->getDimension(line.c_str());
		max_width = std::max(max_width, static_cast<s32>(dim.Width));
		total_height += static_cast<s32>(dim.Height);
	}
	if (lines.empty()) {
		const core::dimension2d<u32> dim = font->getDimension(L"clients");
		max_width = dim.Width;
		total_height = dim.Height;
	}
	const core::rect<s32> draw_bounds = fitModuleHudBounds(*this, max_width, total_height);

	const video::SColor outline_color = readModuleBorderColor();
	const video::SColor background_color = readModuleBackgroundColor();
	const video::SColor text_color = readModuleTextColor();

	const bool draw_background = editing || g_settings->getBool("clients.background");
	if (draw_background) {
		driver->draw2DRectangle(background_color, draw_bounds);
		driver->draw2DRectangleOutline(draw_bounds, outline_color, 2);
	}

	const s32 padding = moduleHudPadding();
	s32 y = draw_bounds.UpperLeftCorner.Y + padding;
	for (const std::wstring &line : lines) {
		const core::dimension2d<u32> dim_u32 = font->getDimension(line.c_str());
		const core::dimension2d<s32> dim(dim_u32.Width, dim_u32.Height);
		const s32 x = draw_bounds.UpperLeftCorner.X + padding;
		font->draw(line.c_str(), core::rect<s32>(x, y, x + dim.Width, y + dim.Height), text_color);
		y += dim.Height;
	}
}
