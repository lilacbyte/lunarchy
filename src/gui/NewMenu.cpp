/*
Lunarchy
Copyright (C) 2025 Maintainer_(Ivan Shkatov) <ivanskatov672@gmail.com>
Copyright (C) 2025 Prounce <prouncedev@gmail.com>
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.
You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

/***************************************************************************************************
 *                                          NewMenu.cpp:                                           *
 *                                                                                                 *
 *          The menu supports various interactive elements, such as categories, sliders,           * 
 *      selection boxes, and dropdowns, using core::rect<s32> for mouse events and collisions      *
 *                                                                                                 *
 *                                  Core functionality includes:                                   *
 *                                                                                                 *
 * - Dynamic creation and cleanup of GUI elements via `create()` and `close()` methods.            *
 * - Drawing methods (`drawCategory`, `drawSelectionBox`, etc.) for rendering menu components.     *
 * - Event handling through `OnEvent()` for togglable cheats, sliders, and selection boxes.        *
 * - Helper methods for geometric calculations and slider value mapping.                           *
 *                                                                                                 *
 *                            This code is cursed as fuck, but it works.                           *
 *                       When you work on this file, increment the counter:                        *
 *                                   Total hours spent here: 195                                   *
 *                                                                                                 *
 *                        This makes me want to jump off a bridge -plus22                          *
 ***************************************************************************************************/

#include "NewMenu.h"
#include "porting.h"
#include "filesys.h"
#include <chrono>
#include "client/localplayer.h"
#include "client/color_theme.h"
#include "gui/moduleColor.h"
#include "util/string.h"
#include <iomanip>
#include <optional>
#include <random>
#include <sstream>
#include <unordered_map>

std::chrono::high_resolution_clock::time_point NewMenu::lastTime = std::chrono::high_resolution_clock::now();

static core::rect<s32> normalizeRect(const core::rect<s32> &rect)
{
	const s32 left = std::min(rect.UpperLeftCorner.X, rect.LowerRightCorner.X);
	const s32 top = std::min(rect.UpperLeftCorner.Y, rect.LowerRightCorner.Y);
	const s32 right = std::max(rect.UpperLeftCorner.X, rect.LowerRightCorner.X);
	const s32 bottom = std::max(rect.UpperLeftCorner.Y, rect.LowerRightCorner.Y);
	return core::rect<s32>(left, top, right, bottom);
}

static bool hasValidBounds(const core::rect<s32> &rect)
{
	return rect.getWidth() > 0 && rect.getHeight() > 0;
}

static const std::wstring &cachedWideFromUtf8(const std::string &text)
{
	static std::unordered_map<std::string, std::wstring> cache;
	auto it = cache.find(text);
	if (it != cache.end())
		return it->second;

	auto converted = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(text);
	return cache.emplace(text, std::move(converted)).first->second;
}

static core::dimension2d<u32> cachedTextDimension(gui::IGUIFont *font, const std::wstring &text)
{
	static std::unordered_map<gui::IGUIFont *, std::unordered_map<std::wstring, core::dimension2d<u32>>> cache;
	auto &font_cache = cache[font];
	auto it = font_cache.find(text);
	if (it != font_cache.end())
		return it->second;

	const core::dimension2d<u32> dim = font->getDimension(text.c_str());
	font_cache.emplace(text, dim);
	return dim;
}

static bool loadHudBounds(const std::string &primary_key, const std::string &legacy_key,
		core::rect<s32> &out_bounds)
{
	const std::string primary_pos1 = "HudElement_Position1_" + primary_key;
	const std::string primary_pos2 = "HudElement_Position2_" + primary_key;
	if (g_settings->exists(primary_pos1) && g_settings->exists(primary_pos2)) {
		const v2f position1 = g_settings->getV2F(primary_pos1);
		const v2f position2 = g_settings->getV2F(primary_pos2);
		const core::rect<s32> primary_bounds = normalizeRect(core::rect<s32>(position1.X, position1.Y, position2.X, position2.Y));
		if (hasValidBounds(primary_bounds)) {
			out_bounds = primary_bounds;
			return true;
		}
	}

	if (!legacy_key.empty()) {
		const std::string legacy_pos1 = "HudElement_Position1_" + legacy_key;
		const std::string legacy_pos2 = "HudElement_Position2_" + legacy_key;
		if (g_settings->exists(legacy_pos1) && g_settings->exists(legacy_pos2)) {
			const v2f position1 = g_settings->getV2F(legacy_pos1);
			const v2f position2 = g_settings->getV2F(legacy_pos2);
			const core::rect<s32> legacy_bounds = normalizeRect(core::rect<s32>(position1.X, position1.Y, position2.X, position2.Y));
			if (hasValidBounds(legacy_bounds)) {
				out_bounds = legacy_bounds;
				return true;
			}
		}
	}

	return false;
}

static std::optional<video::SColor> readAccentColor()
{
	if (g_settings->getBool("hud.enabled")) {
		video::SColor hud_color(255, 255, 255, 255);
		if (parseColorString(g_settings->get("hud.accent_color"), hud_color, true, 0xff))
			return hud_color;
	}
	if (g_settings->existsLocal("hud.enabled"))
		return std::nullopt;

	if (g_settings->existsLocal("hud_color")) {
		if (!g_settings->getBool("hud_color"))
			return std::nullopt;
	} else if (g_settings->existsLocal("hudcolor")) {
		const std::string legacy_value = g_settings->get("hudcolor");
		video::SColor legacy_color(255, 255, 255, 255);
		if (parseColorString(legacy_value, legacy_color, true, 0xff))
			return legacy_color;
		if (!g_settings->getBool("hudcolor"))
			return std::nullopt;
	} else if (g_settings->exists("hud_color")) {
		return std::nullopt;
	}

	video::SColor color(255, 255, 255, 255);
	if (g_settings->existsLocal("globalcolor") &&
			parseColorString(g_settings->get("globalcolor"), color, true, 0xff))
		return color;
	if (g_settings->existsLocal("global_color") &&
			parseColorString(g_settings->get("global_color"), color, true, 0xff))
		return color;
	if (g_settings->exists("globalcolor") &&
			parseColorString(g_settings->get("globalcolor"), color, true, 0xff))
		return color;
	if (g_settings->exists("global_color") &&
			parseColorString(g_settings->get("global_color"), color, true, 0xff))
		return color;

	std::optional<v3f> legacy;
	g_settings->getV3FNoEx("cheat_hud.color", legacy);
	if (legacy) {
		return video::SColor(255,
			static_cast<u8>(std::clamp<float>(legacy->X, 0.0f, 255.0f)),
			static_cast<u8>(std::clamp<float>(legacy->Y, 0.0f, 255.0f)),
			static_cast<u8>(std::clamp<float>(legacy->Z, 0.0f, 255.0f)));
	}

	return std::nullopt;
}

static video::SColor colorForTextField(const std::string &setting_name, const video::SColor &fallback)
{
	if (!g_settings->exists(setting_name))
		return fallback;

	video::SColor color = fallback;
	if (parseColorString(g_settings->get(setting_name), color, true, 0xff))
		return color;

	return fallback;
}

static void syncHudAccentFromTheme(const ColorTheme &theme)
{
	const std::string accent = encodeHexColorString(theme.primary);
	g_settings->set("hud.accent_color", accent);
	g_settings->setBool("hud.enabled", true);
}

static video::SColor shadeColor(const video::SColor &color, float factor)
{
	auto scale = [factor](u8 channel) -> u8 {
		const int value = static_cast<int>(std::round(static_cast<float>(channel) * factor));
		return static_cast<u8>(std::clamp(value, 0, 255));
	};

	return video::SColor(color.getAlpha(), scale(color.getRed()), scale(color.getGreen()), scale(color.getBlue()));
}

static bool isValidHexColorInput(std::string value)
{
	if (!value.empty() && value.front() == '#')
		value.erase(value.begin());

	if (value.size() != 6)
		return false;

	return std::all_of(value.begin(), value.end(), [](unsigned char c) {
		return std::isxdigit(c) != 0;
	});
}

static std::optional<std::string> normalizeHexColorInput(const std::string &value)
{
	if (!isValidHexColorInput(value))
		return std::nullopt;

	if (!value.empty() && value.front() == '#')
		return value;

	return "#" + value;
}

static bool isColorTextSetting(const std::string &setting_id)
{
	return setting_id == "globalcolor" ||
		setting_id == "global_color" ||
		setting_id == "hud.accent_color" ||
		setting_id == "hud_color" ||
		setting_id == "hudcolor" ||
		setting_id.find("_color") != std::string::npos ||
		setting_id.find(".color") != std::string::npos;
}

static bool isColorCheatSetting(const ScriptApiCheatsCheatSetting *setting)
{
	return setting != nullptr &&
		(setting->m_type == "text" && isColorTextSetting(setting->m_setting));
}

static bool isChatColorSetting(const ScriptApiCheatsCheatSetting *setting)
{
	return setting != nullptr &&
		setting->m_type == "text" &&
		setting->m_setting == "chat_color";
}

static bool isBetterDropFilterSetting(const ScriptApiCheatsCheatSetting *setting)
{
	return setting != nullptr &&
		setting->m_type == "text" &&
		(setting->m_setting == "betterdrop_whitelist" ||
			setting->m_setting == "betterdrop_antidrop" ||
			setting->m_setting == "betterdrop_blacklist");
}

static bool isHudColorSetting(const ScriptApiCheatsCheatSetting *setting)
{
	return setting != nullptr &&
		setting->m_type == "text" &&
		(setting->m_setting == "hud.accent_color" ||
			setting->m_setting == "hud.text_color" ||
			setting->m_setting == "hud.background_color" ||
			setting->m_setting == "hud.border_color" ||
			setting->m_setting == "hand_view.color" ||
			setting->m_setting == "chatplus_background_color" ||
			setting->m_setting == "chatplus_border_color");
}

static bool isHudAccentColorSetting(const ScriptApiCheatsCheatSetting *setting)
{
	return setting != nullptr &&
		setting->m_type == "text" &&
		(setting->m_setting == "hud.accent_color" ||
			setting->m_setting == "globalcolor" ||
			setting->m_setting == "global_color" ||
			setting->m_setting == "hudcolor");
}

static std::vector<std::string> splitChatColorStops(const std::string &value)
{
	std::vector<std::string> stops;
	const std::string trimmed = std::string(trim(value));
	if (trimmed.empty() || trimmed == "rainbow")
		return stops;

	std::string token;
	std::istringstream stream(trimmed);
	while (std::getline(stream, token, ',')) {
		const std::string stop = std::string(trim(token));
		if (!stop.empty())
			stops.push_back(stop);
	}
	return stops;
}

static std::string joinChatColorStops(const std::vector<std::string> &stops)
{
	std::ostringstream oss;
	for (size_t i = 0; i < stops.size(); ++i) {
		if (i > 0)
			oss << ",";
		oss << stops[i];
	}
	return oss.str();
}

static video::SColor randomChatColor()
{
	static std::mt19937 rng(std::random_device{}());
	static std::uniform_int_distribution<int> channel_dist(0, 255);
	return video::SColor(255,
		static_cast<u8>(channel_dist(rng)),
		static_cast<u8>(channel_dist(rng)),
		static_cast<u8>(channel_dist(rng)));
}

static std::vector<video::SColor> parseColorPreviewSwatches(const std::string &value)
{
	std::vector<video::SColor> colors;
	for (const std::string &token : splitChatColorStops(value)) {
		if (token == "rainbow")
			continue;

		video::SColor color(255, 255, 255, 255);
		if (parseColorString(token, color, true, 0xff)) {
			colors.push_back(color);
			continue;
		}

		const std::optional<std::string> normalized = normalizeHexColorInput(token);
		if (normalized && parseColorString(*normalized, color, true, 0xff))
			colors.push_back(color);
	}
	return colors;
}

static void drawColorPreview(video::IVideoDriver *driver, const core::rect<s32> &rect,
		const std::string &value, const video::SColor &fallback)
{
	const std::vector<video::SColor> colors = parseColorPreviewSwatches(value);
	if (colors.empty()) {
		driver->draw2DRectangle(fallback, rect);
		return;
	}

	if (colors.size() == 1) {
		driver->draw2DRectangle(colors.front(), rect);
		return;
	}

	const s32 width = std::max<s32>(rect.getWidth(), 1);
	for (size_t i = 0; i < colors.size(); ++i) {
		const s32 left = rect.UpperLeftCorner.X + static_cast<s32>((width * i) / colors.size());
		const s32 right = rect.UpperLeftCorner.X + static_cast<s32>((width * (i + 1)) / colors.size());
		driver->draw2DRectangle(colors[i], core::rect<s32>(left, rect.UpperLeftCorner.Y, right, rect.LowerRightCorner.Y));
		if (i > 0) {
			driver->draw2DLine(
				core::position2d<s32>(left, rect.UpperLeftCorner.Y),
				core::position2d<s32>(left, rect.LowerRightCorner.Y),
				video::SColor(255, 0, 0, 0));
		}
	}
}

static std::string colorToHex(const video::SColor &color);

static std::string appendChatColorStop(const std::string &value)
{
	const std::vector<std::string> stops = splitChatColorStops(value);
	std::vector<video::SColor> colors = parseColorPreviewSwatches(value);
	const std::string next_color = colors.empty()
		? "#FFFFFF"
		: colorToHex(colors.back());

	if (stops.empty())
		return next_color;

	return joinChatColorStops(stops) + "," + next_color;
}

static std::string appendUniqueCsvValue(const std::string &value, const std::string &next)
{
	if (next.empty())
		return value;

	std::vector<std::string> values;
	for (const std::string &entry : str_split(value, ',')) {
		const std::string trimmed = std::string(trim(entry));
		if (trimmed.empty())
			continue;
		if (trimmed == next)
			return value;
		values.push_back(trimmed);
	}
	values.push_back(next);

	std::ostringstream out;
	for (size_t i = 0; i < values.size(); ++i) {
		if (i > 0)
			out << ",";
		out << values[i];
	}
	return out.str();
}

static std::string clearChatColorStops()
{
	return "rainbow";
}

static std::string randomChatColorStops()
{
	static std::mt19937 rng(std::random_device{}());
	static std::uniform_int_distribution<int> stop_dist(2, 5);

	const int stop_count = stop_dist(rng);
	std::vector<std::string> stops;
	stops.reserve(stop_count);
	for (int i = 0; i < stop_count; ++i)
		stops.push_back(colorToHex(randomChatColor()));
	return joinChatColorStops(stops);
}

static std::string setChatColorStop(const std::string &value, size_t index, const video::SColor &color)
{
	std::vector<std::string> stops = splitChatColorStops(value);
	if (stops.empty())
		stops.push_back("#FFFFFF");
	if (index >= stops.size())
		index = stops.size() - 1;
	stops[index] = colorToHex(color);
	return joinChatColorStops(stops);
}

static size_t pickChatColorStopIndex(const std::string &value, const core::rect<s32> &rect, const core::position2d<s32> &pointer)
{
	const std::vector<video::SColor> colors = parseColorPreviewSwatches(value);
	if (colors.size() <= 1)
		return 0;

	const s32 width = std::max<s32>(rect.getWidth(), 1);
	const s32 rel_x = std::clamp(pointer.X - rect.UpperLeftCorner.X, 0, width - 1);
	const size_t index = static_cast<size_t>((rel_x * colors.size()) / width);
	return std::min(index, colors.size() - 1);
}

struct HSVColor {
	float h = 0.0f;
	float s = 0.0f;
	float v = 0.0f;
};

static float clamp01(float value)
{
	return std::clamp(value, 0.0f, 1.0f);
}

static HSVColor colorToHSV(const video::SColor &color)
{
	const float r = static_cast<float>(color.getRed()) / 255.0f;
	const float g = static_cast<float>(color.getGreen()) / 255.0f;
	const float b = static_cast<float>(color.getBlue()) / 255.0f;

	const float max_value = std::max({r, g, b});
	const float min_value = std::min({r, g, b});
	const float delta = max_value - min_value;

	HSVColor hsv;
	hsv.v = max_value;
	hsv.s = (max_value <= 0.0f) ? 0.0f : (delta / max_value);

	if (delta <= 0.0f) {
		hsv.h = 0.0f;
		return hsv;
	}

	if (max_value == r) {
		hsv.h = std::fmod(((g - b) / delta), 6.0f);
	} else if (max_value == g) {
		hsv.h = ((b - r) / delta) + 2.0f;
	} else {
		hsv.h = ((r - g) / delta) + 4.0f;
	}

	hsv.h /= 6.0f;
	if (hsv.h < 0.0f)
		hsv.h += 1.0f;
	return hsv;
}

static video::SColor hsvToColor(float h, float s, float v)
{
	h = std::fmod(h, 1.0f);
	if (h < 0.0f)
		h += 1.0f;
	s = clamp01(s);
	v = clamp01(v);

	const float c = v * s;
	const float hp = h * 6.0f;
	const float x = c * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
	const float m = v - c;

	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;

	switch (static_cast<int>(hp) % 6) {
		case 0: r = c; g = x; b = 0.0f; break;
		case 1: r = x; g = c; b = 0.0f; break;
		case 2: r = 0.0f; g = c; b = x; break;
		case 3: r = 0.0f; g = x; b = c; break;
		case 4: r = x; g = 0.0f; b = c; break;
		case 5: r = c; g = 0.0f; b = x; break;
		default: break;
	}

	auto to_u8 = [m](float channel) -> u8 {
		const int value = static_cast<int>(std::round((channel + m) * 255.0f));
		return static_cast<u8>(std::clamp(value, 0, 255));
	};

	return video::SColor(255, to_u8(r), to_u8(g), to_u8(b));
}

static std::string colorToHex(const video::SColor &color)
{
	std::ostringstream oss;
	oss << "#" << std::uppercase << std::hex << std::setfill('0')
		<< std::setw(2) << static_cast<int>(color.getRed())
		<< std::setw(2) << static_cast<int>(color.getGreen())
		<< std::setw(2) << static_cast<int>(color.getBlue());
	return oss.str();
}

static core::rect<s32> getColorValueRect(const core::rect<s32> &setting_rect, s32 category_height, s32 setting_bar_padding, s32 setting_bar_width)
{
	return core::rect<s32>(
		setting_rect.UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width,
		setting_rect.UpperLeftCorner.Y + category_height,
		setting_rect.LowerRightCorner.X - setting_bar_padding,
		setting_rect.LowerRightCorner.Y - setting_bar_padding);
}

static core::rect<s32> getHudColorSwatchRect(const core::rect<s32> &setting_rect, s32 category_height, s32 setting_bar_padding, s32 setting_bar_width, bool include_sync = false)
{
	const s32 button_size = category_height - (setting_bar_padding * 2);
	const s32 left = setting_rect.UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width;
	const s32 button_count = include_sync ? 2 : 1;
	const s32 right = setting_rect.LowerRightCorner.X - setting_bar_padding -
		(button_size * button_count) - (setting_bar_padding * button_count);
	const s32 top = setting_rect.UpperLeftCorner.Y + category_height + setting_bar_padding;
	return core::rect<s32>(left, top, std::max(left + 1, right), top + button_size);
}

static core::rect<s32> getHudColorRandomRect(const core::rect<s32> &setting_rect, s32 category_height, s32 setting_bar_padding, s32 setting_bar_width, bool include_sync = false)
{
	const s32 button_size = category_height - (setting_bar_padding * 2);
	const core::rect<s32> swatch_rect = getHudColorSwatchRect(setting_rect, category_height, setting_bar_padding, setting_bar_width, include_sync);
	const s32 left = swatch_rect.LowerRightCorner.X + setting_bar_padding;
	const s32 top = swatch_rect.UpperLeftCorner.Y;
	return core::rect<s32>(left, top, left + button_size, top + button_size);
}

static core::rect<s32> getHudColorSyncRect(const core::rect<s32> &setting_rect, s32 category_height, s32 setting_bar_padding, s32 setting_bar_width)
{
	const s32 button_size = category_height - (setting_bar_padding * 2);
	const core::rect<s32> random_rect = getHudColorRandomRect(setting_rect, category_height, setting_bar_padding, setting_bar_width, true);
	const s32 left = random_rect.LowerRightCorner.X + setting_bar_padding;
	const s32 top = random_rect.UpperLeftCorner.Y;
	return core::rect<s32>(left, top, left + button_size, top + button_size);
}

static core::rect<s32> getChatColorSwatchRect(const core::rect<s32> &setting_rect, s32 category_height, s32 setting_bar_padding, s32 setting_bar_width)
{
	const s32 button_size = category_height - (setting_bar_padding * 2);
	const s32 left = setting_rect.UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width;
	const s32 right = setting_rect.LowerRightCorner.X - setting_bar_padding - (button_size * 3) - (setting_bar_padding * 3);
	const s32 top = setting_rect.UpperLeftCorner.Y + category_height + setting_bar_padding;
	return core::rect<s32>(left, top, std::max(left + 1, right), top + button_size);
}

static core::rect<s32> getChatColorPlusRect(const core::rect<s32> &setting_rect, s32 category_height, s32 setting_bar_padding, s32 setting_bar_width)
{
	const s32 button_size = category_height - (setting_bar_padding * 2);
	const core::rect<s32> swatch_rect = getChatColorSwatchRect(setting_rect, category_height, setting_bar_padding, setting_bar_width);
	const s32 left = swatch_rect.LowerRightCorner.X + setting_bar_padding;
	const s32 top = swatch_rect.UpperLeftCorner.Y;
	return core::rect<s32>(left, top, left + button_size, top + button_size);
}

static core::rect<s32> getInlineSettingButtonRect(const core::rect<s32> &setting_rect,
	s32 category_height, s32 setting_bar_padding)
{
	const s32 button_size = category_height - (setting_bar_padding * 2);
	const s32 right = setting_rect.LowerRightCorner.X - setting_bar_padding;
	const s32 top = setting_rect.UpperLeftCorner.Y + setting_bar_padding;
	return core::rect<s32>(right - button_size, top, right, top + button_size);
}

static core::rect<s32> getChatColorClearRect(const core::rect<s32> &setting_rect, s32 category_height, s32 setting_bar_padding, s32 setting_bar_width)
{
	const s32 button_size = category_height - (setting_bar_padding * 2);
	const core::rect<s32> plus_rect = getChatColorPlusRect(setting_rect, category_height, setting_bar_padding, setting_bar_width);
	const s32 left = plus_rect.LowerRightCorner.X + setting_bar_padding;
	const s32 top = plus_rect.UpperLeftCorner.Y;
	return core::rect<s32>(left, top, left + button_size, top + button_size);
}

static core::rect<s32> getChatColorRandomRect(const core::rect<s32> &setting_rect, s32 category_height, s32 setting_bar_padding, s32 setting_bar_width)
{
	const s32 button_size = category_height - (setting_bar_padding * 2);
	const core::rect<s32> clear_rect = getChatColorClearRect(setting_rect, category_height, setting_bar_padding, setting_bar_width);
	const s32 left = clear_rect.LowerRightCorner.X + setting_bar_padding;
	const s32 top = clear_rect.UpperLeftCorner.Y;
	return core::rect<s32>(left, top, left + button_size, top + button_size);
}

static core::rect<s32> getChatColorTextRect(const core::rect<s32> &setting_rect, s32 category_height, s32 setting_bar_padding, s32 setting_bar_width)
{
	const core::rect<s32> random_rect = getChatColorRandomRect(setting_rect, category_height, setting_bar_padding, setting_bar_width);
	return core::rect<s32>(
		setting_rect.UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width,
		random_rect.LowerRightCorner.Y + setting_bar_padding,
		setting_rect.LowerRightCorner.X - setting_bar_padding,
		setting_rect.LowerRightCorner.Y - setting_bar_padding);
}

static core::rect<s32> getColorPickerFrameRect(const core::position2d<s32> &anchor, const core::dimension2d<u32> &screen_size)
{
	const s32 margin = 8;
	const s32 square_size = 176;
	const s32 hue_height = 16;
	const s32 title_height = 18;
	const s32 width = square_size + margin * 2;
	const s32 height = title_height + square_size + hue_height + margin * 3 + 2;
	s32 left = anchor.X - (width / 2);
	s32 top = anchor.Y - (height / 2);

	left = std::clamp(left, 0, std::max(0, static_cast<s32>(screen_size.Width) - width));
	top = std::clamp(top, 0, std::max(0, static_cast<s32>(screen_size.Height) - height));

	return core::rect<s32>(left, top, left + width, top + height);
}

static core::rect<s32> getColorPickerSquareRect(const core::rect<s32> &frame_rect)
{
	const s32 margin = 8;
	const s32 title_height = 18;
	const s32 square_size = 176;
	return core::rect<s32>(
		frame_rect.UpperLeftCorner.X + margin,
		frame_rect.UpperLeftCorner.Y + title_height,
		frame_rect.UpperLeftCorner.X + margin + square_size,
		frame_rect.UpperLeftCorner.Y + title_height + square_size);
}

static core::rect<s32> getColorPickerHueRect(const core::rect<s32> &frame_rect)
{
	const s32 margin = 8;
	const s32 title_height = 18;
	const s32 square_size = 176;
	const s32 hue_height = 16;
	return core::rect<s32>(
		frame_rect.UpperLeftCorner.X + margin,
		frame_rect.UpperLeftCorner.Y + title_height + square_size + 8,
		frame_rect.UpperLeftCorner.X + margin + square_size,
		frame_rect.UpperLeftCorner.Y + title_height + square_size + 8 + hue_height);
}

static video::SColor colorFromSettingText(const std::string &stored, const video::SColor &fallback)
{
	video::SColor color = fallback;
	if (parseColorString(stored, color, true, 0xff))
		return color;

	const std::optional<std::string> normalized_color = normalizeHexColorInput(stored);
	if (normalized_color && parseColorString(*normalized_color, color, true, 0xff))
		return color;

	return fallback;
}

static void syncHudColorTargets(const std::string &setting_id)
{
	const video::SColor parsed_color = colorFromSettingText(
		g_settings->get(setting_id), video::SColor(255, 255, 255, 255));
	const std::string color = colorToHex(parsed_color);
	std::ostringstream quickmenu_color;
	quickmenu_color << "(" << static_cast<unsigned int>(parsed_color.getRed())
		<< ", " << static_cast<unsigned int>(parsed_color.getGreen())
		<< ", " << static_cast<unsigned int>(parsed_color.getBlue()) << ")";

	g_settings->set("hud.text_color", color);
	g_settings->set("hud.background_color", color);
	g_settings->set("hud.border_color", color);
	g_settings->set("chatplus_background_color", color);
	g_settings->set("chatplus_border_color", color);
	g_settings->set("cheat_menu_active_bg_color", quickmenu_color.str());
}

float NewMenu::getDeltaTime() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> deltaTime = currentTime - lastTime;
    lastTime = currentTime;

    return deltaTime.count();
}

NewMenu::NewMenu(gui::IGUIEnvironment* env, 
    gui::IGUIElement* parent, 
    s32 id, IMenuManager* menumgr, 
    Client* client)
    : IGUIElement(gui::EGUIET_ELEMENT, env, parent, id, 
    core::rect<s32>(0, 0, 0, 0)), 
    m_menumgr(menumgr), isDragging(false), draggedRectIndex(-1),
    m_client(client)
{    
    infostream << "[NEWMENU] Successfully created" << std::endl;
    this->env = env;
}

NewMenu::~NewMenu()
{
    for (size_t i = 0; i < cheatSettingTextFields.size(); ++i) {
        for (size_t c = 0; c < cheatSettingTextFields[i].size(); ++c) {
            for (size_t s = 0; s < cheatSettingTextFields[i][c].size(); ++s) {
                delete cheatSettingTextFields[i][c][s];
            }
        }
    }

    for (auto element : hudElements) {
        delete element;
    }
    hudElements.clear();
}

void NewMenu::syncTextFieldStates()
{
    for (size_t i = 0; i < cheatSettingTextFields.size(); ++i) {
        for (size_t c = 0; c < cheatSettingTextFields[i].size(); ++c) {
            for (size_t s = 0; s < cheatSettingTextFields[i][c].size(); ++s) {
                if (cheatSettingTextFields[i][c][s] == nullptr)
                    continue;

                const bool visible = selectedCategory[i] && selectedCheat[i][c] && !isSelecting && !isEditing;
                cheatSettingTextFields[i][c][s]->setVisible(visible);
                cheatSettingTextFields[i][c][s]->setEnabled(visible);
            }
        }
    }
}

void NewMenu::commitColorSetting(size_t category_index, size_t cheat_index, size_t setting_index, const video::SColor &color)
{
	GET_SCRIPT_POINTER

	const std::string setting_id = script->m_cheat_categories[category_index]->m_cheats[cheat_index]->m_cheat_settings[setting_index]->m_setting;
	std::string next_color;
	if (setting_id == "chat_color") {
		const std::string current_value = g_settings->get(setting_id);
		next_color = setChatColorStop(current_value, static_cast<size_t>(std::max(0, selectingColorStopIndex)), color);
	} else {
		next_color = colorToHex(color);
	}
	g_settings->set(setting_id, next_color);

	if (cheatSettingTextFields[category_index][cheat_index][setting_index] != nullptr) {
		const std::wstring wide_color = utf8_to_wide(next_color);
		cheatSettingTextFields[category_index][cheat_index][setting_index]->setText(wide_color.c_str());
		cheatSettingTextLasts[category_index][cheat_index][setting_index] = wide_color;
	}

	if (setting_id == "hud.accent_color" ||
			setting_id == "globalcolor" ||
			setting_id == "global_color" ||
			setting_id == "hud_color" ||
			setting_id == "hudcolor") {
		applyAppearanceOverrides();
	}
}

bool NewMenu::handleColorPickerEvent(const irr::SEvent& event)
{
	if (!isColorSelecting)
		return false;

	GET_SCRIPT_POINTER_BOOL

	if (event.EventType != irr::EET_MOUSE_INPUT_EVENT)
		return true;

	if (selectingColorCategoryIndex < 0 || selectingColorCheatIndex < 0 || selectingColorSettingIndex < 0)
		return true;

	const core::rect<s32> &setting_rect = cheatSettingRects[selectingColorCategoryIndex][selectingColorCheatIndex][selectingColorSettingIndex];
	const core::position2d<s32> anchor = colorPickerAnchorValid
		? colorPickerAnchor
		: core::position2d<s32>(setting_rect.LowerRightCorner.X, setting_rect.LowerRightCorner.Y);
	const core::rect<s32> frame_rect = getColorPickerFrameRect(anchor, Environment->getVideoDriver()->getScreenSize());
	const core::rect<s32> square_rect = getColorPickerSquareRect(frame_rect);
	const core::rect<s32> hue_rect = getColorPickerHueRect(frame_rect);
	const core::vector2d<s32> pointer(event.MouseInput.X, event.MouseInput.Y);
	const std::string setting_id = script->m_cheat_categories[selectingColorCategoryIndex]->m_cheats[selectingColorCheatIndex]->m_cheat_settings[selectingColorSettingIndex]->m_setting;

	if (event.MouseInput.Event == irr::EMIE_LMOUSE_LEFT_UP) {
		if (ignoreColorPickerMouseUp) {
			ignoreColorPickerMouseUp = false;
			return true;
		}
		if (!frame_rect.isPointInside(pointer)) {
			isColorSelecting = false;
			colorPickerAnchorValid = false;
		}
		return true;
	}

	if (event.MouseInput.Event == irr::EMIE_MOUSE_MOVED && !event.MouseInput.isLeftPressed()) {
		return true;
	}

	if (event.MouseInput.Event == irr::EMIE_LMOUSE_PRESSED_DOWN ||
			(event.MouseInput.Event == irr::EMIE_MOUSE_MOVED && event.MouseInput.isLeftPressed())) {
		video::SColor current_color = colorFromSettingText(g_settings->get(setting_id), video::SColor(255, 255, 255, 255));
		if (setting_id == "chat_color") {
			const std::vector<video::SColor> preview_colors = parseColorPreviewSwatches(g_settings->get(setting_id));
			if (!preview_colors.empty()) {
				const size_t stop_index = std::min<size_t>(selectingColorStopIndex, preview_colors.size() - 1);
				current_color = preview_colors[stop_index];
			}
		}
		HSVColor hsv = colorToHSV(current_color);

		if (square_rect.isPointInside(pointer)) {
			const float saturation = static_cast<float>(pointer.X - square_rect.UpperLeftCorner.X) / std::max(1, square_rect.getWidth() - 1);
			const float value = 1.0f - static_cast<float>(pointer.Y - square_rect.UpperLeftCorner.Y) / std::max(1, square_rect.getHeight() - 1);
			hsv.s = clamp01(saturation);
			hsv.v = clamp01(value);
			commitColorSetting(static_cast<size_t>(selectingColorCategoryIndex), static_cast<size_t>(selectingColorCheatIndex), static_cast<size_t>(selectingColorSettingIndex), hsvToColor(hsv.h, hsv.s, hsv.v));
			return true;
		}

		if (hue_rect.isPointInside(pointer)) {
			const float hue = static_cast<float>(pointer.X - hue_rect.UpperLeftCorner.X) / std::max(1, hue_rect.getWidth() - 1);
			hsv.h = clamp01(hue);
			commitColorSetting(static_cast<size_t>(selectingColorCategoryIndex), static_cast<size_t>(selectingColorCheatIndex), static_cast<size_t>(selectingColorSettingIndex), hsvToColor(hsv.h, hsv.s, hsv.v));
			return true;
		}

		if (!frame_rect.isPointInside(pointer)) {
			isColorSelecting = false;
			colorPickerAnchorValid = false;
		}
		return true;
	}

	return true;
}

void NewMenu::drawColorPicker(video::IVideoDriver* driver, gui::IGUIFont* font, size_t category_index, size_t cheat_index, size_t setting_index)
{
	if (!isColorSelecting)
		return;

	GET_SCRIPT_POINTER

	const std::string setting_id = script->m_cheat_categories[category_index]->m_cheats[cheat_index]->m_cheat_settings[setting_index]->m_setting;
	const core::rect<s32> &setting_rect = cheatSettingRects[category_index][cheat_index][setting_index];
	const core::position2d<s32> anchor = colorPickerAnchorValid
		? colorPickerAnchor
		: core::position2d<s32>(setting_rect.LowerRightCorner.X, setting_rect.LowerRightCorner.Y);
	const core::rect<s32> frame_rect = getColorPickerFrameRect(anchor, driver->getScreenSize());
	const core::rect<s32> square_rect = getColorPickerSquareRect(frame_rect);
	const core::rect<s32> hue_rect = getColorPickerHueRect(frame_rect);
	video::SColor current_color = colorFromSettingText(g_settings->get(setting_id), video::SColor(255, 255, 255, 255));
	if (setting_id == "chat_color") {
		const std::vector<video::SColor> preview_colors = parseColorPreviewSwatches(g_settings->get(setting_id));
		if (!preview_colors.empty()) {
			const size_t stop_index = std::min<size_t>(selectingColorStopIndex, preview_colors.size() - 1);
			current_color = preview_colors[stop_index];
		}
	}
	const HSVColor hsv = colorToHSV(current_color);
	const std::string hex = colorToHex(current_color);

	driver->draw2DRectangle(current_theme.background, frame_rect);
	driver->draw2DRectangleOutline(frame_rect, current_theme.primary, 2);

	const core::rect<s32> title_rect(
		frame_rect.UpperLeftCorner.X + 8,
		frame_rect.UpperLeftCorner.Y + 2,
		frame_rect.LowerRightCorner.X - 8,
		frame_rect.UpperLeftCorner.Y + 18);
	font->draw(L"Hex Color Picker", title_rect, current_theme.text, false, true);

	const s32 square_steps = 64;
	for (s32 y = 0; y < square_steps; ++y) {
		const float value = 1.0f - static_cast<float>(y) / static_cast<float>(square_steps - 1);
		const s32 row_top = square_rect.UpperLeftCorner.Y + (square_rect.getHeight() * y) / square_steps;
		const s32 row_bottom = square_rect.UpperLeftCorner.Y + (square_rect.getHeight() * (y + 1)) / square_steps;
		for (s32 x = 0; x < square_steps; ++x) {
			const float saturation = static_cast<float>(x) / static_cast<float>(square_steps - 1);
			const s32 col_left = square_rect.UpperLeftCorner.X + (square_rect.getWidth() * x) / square_steps;
			const s32 col_right = square_rect.UpperLeftCorner.X + (square_rect.getWidth() * (x + 1)) / square_steps;
			driver->draw2DRectangle(hsvToColor(hsv.h, saturation, value), core::rect<s32>(col_left, row_top, col_right, row_bottom));
		}
	}

	driver->draw2DRectangleOutline(square_rect, video::SColor(255, 0, 0, 0), 2);
	const s32 square_x = square_rect.UpperLeftCorner.X + static_cast<s32>(hsv.s * std::max(1, square_rect.getWidth() - 1));
	const s32 square_y = square_rect.UpperLeftCorner.Y + static_cast<s32>((1.0f - hsv.v) * std::max(1, square_rect.getHeight() - 1));
	const core::rect<s32> square_marker(square_x - 4, square_y - 4, square_x + 4, square_y + 4);
	driver->draw2DRectangleOutline(square_marker, video::SColor(255, 255, 255, 255), 2);
	driver->draw2DRectangleOutline(square_marker, video::SColor(255, 0, 0, 0), 1);

	const s32 hue_steps = 180;
	for (s32 x = 0; x < hue_steps; ++x) {
		const float hue = static_cast<float>(x) / static_cast<float>(hue_steps - 1);
		const s32 col_left = hue_rect.UpperLeftCorner.X + (hue_rect.getWidth() * x) / hue_steps;
		const s32 col_right = hue_rect.UpperLeftCorner.X + (hue_rect.getWidth() * (x + 1)) / hue_steps;
		driver->draw2DRectangle(hsvToColor(hue, 1.0f, 1.0f), core::rect<s32>(col_left, hue_rect.UpperLeftCorner.Y, col_right, hue_rect.LowerRightCorner.Y));
	}

	driver->draw2DRectangleOutline(hue_rect, video::SColor(255, 0, 0, 0), 2);
	const s32 hue_x = hue_rect.UpperLeftCorner.X + static_cast<s32>(hsv.h * std::max(1, hue_rect.getWidth() - 1));
	const core::rect<s32> hue_marker(hue_x - 2, hue_rect.UpperLeftCorner.Y - 2, hue_x + 2, hue_rect.LowerRightCorner.Y + 2);
	driver->draw2DRectangleOutline(hue_marker, video::SColor(255, 255, 255, 255), 2);
	driver->draw2DRectangleOutline(hue_marker, video::SColor(255, 0, 0, 0), 1);

	const core::rect<s32> preview_rect(
		frame_rect.UpperLeftCorner.X + 8,
		hue_rect.LowerRightCorner.Y + 6,
		frame_rect.LowerRightCorner.X - 8,
		hue_rect.LowerRightCorner.Y + 24);
	driver->draw2DRectangle(current_color, preview_rect);
	driver->draw2DRectangleOutline(preview_rect, current_theme.primary, 2);

	const std::wstring hex_wide = cachedWideFromUtf8("Hex: " + hex);
	font->draw(hex_wide.c_str(), core::rect<s32>(
		preview_rect.UpperLeftCorner.X + 4,
		preview_rect.UpperLeftCorner.Y + 1,
		preview_rect.LowerRightCorner.X - 4,
		preview_rect.LowerRightCorner.Y - 1),
		current_theme.text, false, true);
}

s32 NewMenu::roundToGrid(s32 num) {
    return std::round(num / (category_height / 2)) * (category_height / 2);
}

float slowTransitionHue(float h) {
    if (h >= 2.0f && h < 3.0f) {
        return 2.0f + (h - 2.0f) / 2.0f;
    } else if (h >= 3.0f) {
        return 2.5f + (h - 3.0f);
    } else {
        return h;
    }
}

video::SColor getNextRainbowColor() {
    static float m_rainbow_offset = 0.0f;
    static auto last_update = std::chrono::steady_clock::now();

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<float> elapsed = now - last_update;

    if (elapsed.count() >= 0.05f) {
        m_rainbow_offset += 0.06f;
        if (m_rainbow_offset >= 6.0f)
            m_rainbow_offset -= 6.0f;
        last_update = now;
    }

    float h = m_rainbow_offset;
    h = slowTransitionHue(h);

    float x = (1 - fabs(fmod(h, 2.0f) - 1.0f)) * 255.0f;

    switch ((int)h) {
        case 0: return video::SColor(255, 255, (u8)x, 0);
        case 1: return video::SColor(255, (u8)x, 255, 0);
        case 2: return video::SColor(255, 0, 255, (u8)x);
        case 3: return video::SColor(255, 0, (u8)x, 255);
        case 4: return video::SColor(255, (u8)x, 0, 255);
        case 5: return video::SColor(255, 255, 0, (u8)x);
        default: return video::SColor(255, 255, 0, 0);
    }
}


void NewMenu::setColorsFromTheme(const ColorTheme theme)
{
    settingBackgroundColor = theme.background_top;
    settingBarColor = theme.primary;
    sliderColor = theme.text;
    sliderBarColor = theme.text;
    sliderColorActive = theme.text_muted;
    sliderBarColorActive = theme.primary_muted;
    option_color = theme.background_top;
}

void NewMenu::applyAppearanceOverrides()
{
	current_theme = theme_manager.GetThemeByName(current_theme_name);
	setColorsFromTheme(current_theme);

	const std::optional<video::SColor> accent = readAccentColor();
	if (!accent)
		return;

	current_theme.background_top = shadeColor(*accent, 0.22f);
	current_theme.background = shadeColor(*accent, 0.18f);
	current_theme.background_bottom = shadeColor(*accent, 0.14f);
	current_theme.border = shadeColor(*accent, 0.82f);
	current_theme.primary = *accent;
	current_theme.primary_muted = shadeColor(*accent, 0.78f);
	current_theme.secondary = shadeColor(*accent, 0.94f);
	current_theme.secondary_muted = shadeColor(*accent, 0.66f);
	settingBarColor = current_theme.primary;
	sliderColorActive = current_theme.secondary;
	sliderBarColorActive = current_theme.primary_muted;
	option_color = current_theme.background_top;
}

void NewMenu::setWidthFromMultiplier(const s32 multiplier)
{
    const s32 scaled_height = Environment->getVideoDriver()->getScreenSize().Height * 0.035 * (multiplier / 10.0f);
    const s32 font_line_height = static_cast<s32>(
        g_fontengine->getLineHeight(FontSpec(g_fontengine->getDefaultFontSize(), FM_Standard, false, false)));
    const s32 minimum_height = font_line_height + 8;

    category_height = std::max<s32>(scaled_height, minimum_height);

    category_width  = category_height * 5;

    if (m_initialized) {
        GET_SCRIPT_POINTER
        for (size_t i = 0; i < script->m_cheat_categories.size(); ++i) {
            respaceMenu(i);
        }
    }
}

void NewMenu::adjustCategoryPositions() {
    if (!m_initialized) return;

    video::IVideoDriver* driver = Environment->getVideoDriver();
    auto newSize = driver->getScreenSize();

    if (newSize == lastScreenSize)
        return;

    float scaleX = float(newSize.Width) / lastScreenSize.Width;
    float scaleY = float(newSize.Height) / lastScreenSize.Height;

    for (size_t i = 0; i < category_positions.size(); ++i) {
        category_positions[i].X = std::round(category_positions[i].X * scaleX);
        category_positions[i].Y = std::round(category_positions[i].Y * scaleY);
    }

    for (size_t i = 0; i < category_positions.size(); ++i) {
        respaceMenu(i);
    }

    lastScreenSize = newSize;
}

void NewMenu::create()
{
    GET_SCRIPT_POINTER

    if (script->m_cheat_categories.empty()) {
        std::cout << "No categories available." << std::endl;
        return;
    }

    if (!m_initialized) {
        for (auto element : hudElements) {
            delete element;
        }
        hudElements.clear();

        themes_path = porting::path_user + DIR_DELIM + "themes";
        theme_manager = ThemeManager();
        theme_manager.LoadThemes(themes_path);

        s32 multiplier = g_settings->getS32("WidthMultiplier");

        setWidthFromMultiplier(multiplier);

		if (g_settings->exists("ColorTheme")) {
        current_theme_name = g_settings->get("ColorTheme");
    } else {
        current_theme_name = "Modern";
        g_settings->set("ColorTheme", current_theme_name);
    }

    current_theme = theme_manager.GetThemeByName(current_theme_name);
    setColorsFromTheme(current_theme);
    applyAppearanceOverrides();

        video::IVideoDriver* driver = Environment->getVideoDriver();
        lastScreenSize = driver->getScreenSize();

        // INITIALIZE CHEAT HUD ELEMENTS
        if (g_settings->exists("HudElement_Position1_target")) {
            v2f position = g_settings->getV2F("HudElement_Position1_target");
            v2f position2 = g_settings->getV2F("HudElement_Position2_target");
            if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid")) {
                hudElements.push_back(new TargetHUD(core::rect<s32>(roundToGrid(position.X), roundToGrid(position.Y), roundToGrid(position2.X), roundToGrid(position2.Y))));
            } else {
                hudElements.push_back(new TargetHUD(core::rect<s32>(position.X, position.Y, position2.X, position2.Y)));
            }
        } else {
            hudElements.push_back(new TargetHUD(core::rect<s32>(400, 400, 600, 500)));
        }

        hudElements[0]->elementName = "target";

        if (g_settings->exists("HudElement_Position1_coords")) {
            v2f position = g_settings->getV2F("HudElement_Position1_coords");
            v2f position2 = g_settings->getV2F("HudElement_Position2_coords");
            if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid")) {
                hudElements.push_back(new coordsHUD(core::rect<s32>(roundToGrid(position.X), roundToGrid(position.Y), roundToGrid(position2.X), roundToGrid(position2.Y))));
            } else {
                hudElements.push_back(new coordsHUD(core::rect<s32>(position.X, position.Y, position2.X, position2.Y)));
            }
        } else {
            hudElements.push_back(new coordsHUD(core::rect<s32>(400, 400, 600, 500)));
        }

        hudElements[1]->elementName = "coords";

        {
            core::rect<s32> online_bounds;
            if (loadHudBounds("online_hud", "clients_hud", online_bounds) ||
                    loadHudBounds("clients", "", online_bounds)) {
                if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid")) {
                    hudElements.push_back(new clientsHUD(core::rect<s32>(
                        roundToGrid(online_bounds.UpperLeftCorner.X),
                        roundToGrid(online_bounds.UpperLeftCorner.Y),
                        roundToGrid(online_bounds.LowerRightCorner.X),
                        roundToGrid(online_bounds.LowerRightCorner.Y)),
                        clientsHUD::Section::Online));
                } else {
                    hudElements.push_back(new clientsHUD(online_bounds, clientsHUD::Section::Online));
                }
            } else {
                hudElements.push_back(new clientsHUD(core::rect<s32>(10, 10, 310, 90), clientsHUD::Section::Online));
            }
        }
        hudElements.back()->elementName = "online_hud";

        {
            core::rect<s32> nearby_bounds;
            if (loadHudBounds("nearby_hud", "", nearby_bounds)) {
                if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid")) {
                    hudElements.push_back(new clientsHUD(core::rect<s32>(
                        roundToGrid(nearby_bounds.UpperLeftCorner.X),
                        roundToGrid(nearby_bounds.UpperLeftCorner.Y),
                        roundToGrid(nearby_bounds.LowerRightCorner.X),
                        roundToGrid(nearby_bounds.LowerRightCorner.Y)),
                        clientsHUD::Section::Nearby));
                } else {
                    hudElements.push_back(new clientsHUD(nearby_bounds, clientsHUD::Section::Nearby));
                }
            } else {
                core::rect<s32> old_clients_bounds;
                if (loadHudBounds("clients_hud", "clients", old_clients_bounds)) {
                    old_clients_bounds += core::position2d<s32>(0, old_clients_bounds.getHeight() + 8);
                    hudElements.push_back(new clientsHUD(old_clients_bounds, clientsHUD::Section::Nearby));
                } else {
                    hudElements.push_back(new clientsHUD(core::rect<s32>(10, 105, 310, 220), clientsHUD::Section::Nearby));
                }
            }
        }
        hudElements.back()->elementName = "nearby_hud";

        {
            core::rect<s32> ping_bounds;
            if (loadHudBounds("ping_hud", "ping", ping_bounds)) {
                if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid")) {
                    hudElements.push_back(new pingHUD(m_client, core::rect<s32>(
                        roundToGrid(ping_bounds.UpperLeftCorner.X),
                        roundToGrid(ping_bounds.UpperLeftCorner.Y),
                        roundToGrid(ping_bounds.LowerRightCorner.X),
                        roundToGrid(ping_bounds.LowerRightCorner.Y))));
                } else {
                    hudElements.push_back(new pingHUD(m_client, ping_bounds));
                }
        } else {
            hudElements.push_back(new pingHUD(m_client, core::rect<s32>(10, 150, 190, 190)));
        }
        }
        hudElements.back()->elementName = "ping_hud";

        {
            core::rect<s32> fps_bounds;
            if (loadHudBounds("fps_hud", "fps", fps_bounds)) {
                if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid")) {
                    hudElements.push_back(new FpsHUD(core::rect<s32>(
                        roundToGrid(fps_bounds.UpperLeftCorner.X),
                        roundToGrid(fps_bounds.UpperLeftCorner.Y),
                        roundToGrid(fps_bounds.LowerRightCorner.X),
                        roundToGrid(fps_bounds.LowerRightCorner.Y))));
                } else {
                    hudElements.push_back(new FpsHUD(fps_bounds));
                }
            } else {
                hudElements.push_back(new FpsHUD(core::rect<s32>(10, 195, 140, 230)));
            }
        }
        hudElements.back()->elementName = "fps_hud";

        {
            core::rect<s32> totems_bounds;
            if (loadHudBounds("totems_hud", "totems", totems_bounds)) {
                if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid")) {
                    hudElements.push_back(new TotemsHUD(m_client, core::rect<s32>(
                        roundToGrid(totems_bounds.UpperLeftCorner.X),
                        roundToGrid(totems_bounds.UpperLeftCorner.Y),
                        roundToGrid(totems_bounds.LowerRightCorner.X),
                        roundToGrid(totems_bounds.LowerRightCorner.Y))));
                } else {
                    hudElements.push_back(new TotemsHUD(m_client, totems_bounds));
                }
            } else {
                hudElements.push_back(new TotemsHUD(m_client, core::rect<s32>(10, 235, 170, 270)));
            }
        }
        hudElements.back()->elementName = "totems_hud";

        {
            core::rect<s32> waila_bounds;
            if (loadHudBounds("waila_hud", "waila", waila_bounds)) {
                if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid")) {
                    hudElements.push_back(new WailaHUD(m_client, core::rect<s32>(
                        roundToGrid(waila_bounds.UpperLeftCorner.X),
                        roundToGrid(waila_bounds.UpperLeftCorner.Y),
                        roundToGrid(waila_bounds.LowerRightCorner.X),
                        roundToGrid(waila_bounds.LowerRightCorner.Y))));
                } else {
                    hudElements.push_back(new WailaHUD(m_client, waila_bounds));
                }
            } else {
                hudElements.push_back(new WailaHUD(m_client, core::rect<s32>(10, 275, 260, 325)));
            }
        }
        hudElements.back()->elementName = "waila_hud";

        {
            core::rect<s32> cheathud_bounds;
            if (loadHudBounds("cheathud", "", cheathud_bounds)) {
                if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid")) {
                    hudElements.push_back(new CheatHUD(m_client, core::rect<s32>(
                        roundToGrid(cheathud_bounds.UpperLeftCorner.X),
                        roundToGrid(cheathud_bounds.UpperLeftCorner.Y),
                        roundToGrid(cheathud_bounds.LowerRightCorner.X),
                        roundToGrid(cheathud_bounds.LowerRightCorner.Y))));
                } else {
                    hudElements.push_back(new CheatHUD(m_client, cheathud_bounds));
                }
        } else {
            hudElements.push_back(new CheatHUD(m_client, core::rect<s32>(400, 400, 650, 520)));
        }
        }

        hudElements.back()->elementName = "cheathud";

        {
            core::rect<s32> equipment_bounds;
            if (loadHudBounds("equipment_hud", "equipmenthud", equipment_bounds)) {
                if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid")) {
                    hudElements.push_back(new EquipmentHUD(m_client, core::rect<s32>(
                        roundToGrid(equipment_bounds.UpperLeftCorner.X),
                        roundToGrid(equipment_bounds.UpperLeftCorner.Y),
                        roundToGrid(equipment_bounds.LowerRightCorner.X),
                        roundToGrid(equipment_bounds.LowerRightCorner.Y))));
                } else {
                    hudElements.push_back(new EquipmentHUD(m_client, equipment_bounds));
                }
            } else {
                hudElements.push_back(new EquipmentHUD(m_client, core::rect<s32>(10, lastScreenSize.Height / 2 - 90, 260, lastScreenSize.Height / 2 + 90)));
            }
        }

        hudElements.back()->elementName = "equipment_hud";

        {
            core::rect<s32> chat_bounds;
            if (loadHudBounds("chat_hud", "", chat_bounds)) {
                if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid")) {
                    hudElements.push_back(new ChatHUD(core::rect<s32>(
                        roundToGrid(chat_bounds.UpperLeftCorner.X),
                        roundToGrid(chat_bounds.UpperLeftCorner.Y),
                        roundToGrid(chat_bounds.LowerRightCorner.X),
                        roundToGrid(chat_bounds.LowerRightCorner.Y))));
                } else {
                    hudElements.push_back(new ChatHUD(chat_bounds));
                }
            } else {
                hudElements.push_back(new ChatHUD(core::rect<s32>(10, 5, 460, 120)));
            }
        }

        hudElements.back()->elementName = "chat_hud";
        editHUDbuttonBounds = core::rect<s32>(
            roundToGrid(lastScreenSize.Width - ((category_height/2) * 9)),
            roundToGrid(lastScreenSize.Height - ((category_height/2) * 3)),
            roundToGrid(lastScreenSize.Width - (category_height/2)) + 1,
            roundToGrid(lastScreenSize.Height - (category_height/2)) + 1
        );

        // Ensure "Client" category exists
        ScriptApiCheatsCategory *client_category = nullptr;

        for (auto *category : script->m_cheat_categories) {
            if (category && category->m_name == "Client") {
                client_category = category;
                break;
            }
        }

        if (!client_category) {
            client_category = new ScriptApiCheatsCategory("Client");
            script->m_cheat_categories.push_back(client_category);
        }

        // Client-side quick access categories
        ScriptApiCheatsCheat *grid = nullptr;
        ScriptApiCheatsCheat *hints_cheat = nullptr;

        for (auto *cheat : client_category->m_cheats) {
            if (cheat && cheat->m_name == "MenuGrid") {
                grid = cheat;
                break;
            }
        }

        for (auto *cheat : client_category->m_cheats) {
            if (cheat && cheat->m_name == "Hints") {
                hints_cheat = cheat;
                break;
            }
        }

        if (!grid) {
            grid = new ScriptApiCheatsCheat("MenuGrid", "use_menu_grid", "");
            client_category->m_cheats.push_back(grid);
        }

        if (!hints_cheat) {
            hints_cheat = new ScriptApiCheatsCheat("Hints", "use_hints", "");
            client_category->m_cheats.push_back(hints_cheat);
        }

        // START RESIZING ALL ARRAYS
        category_positions.resize(script->m_cheat_categories.size(), core::position2d<s32>(0, 0));
        categoryRects.resize(script->m_cheat_categories.size(), core::rect<s32>(0,0,0,0));
        dropdownRects.resize(script->m_cheat_categories.size(), core::rect<s32>(0,0,0,0));
        textRects.resize(script->m_cheat_categories.size(), core::rect<s32>(0,0,0,0));
        selectionBoxRects.resize(script->m_cheat_categories.size());
        selectedCategory.resize(script->m_cheat_categories.size(), false);
        selectedCheat.resize(script->m_cheat_categories.size());
        dropdownHovered.resize(script->m_cheat_categories.size(), false);
        textHovered.resize(script->m_cheat_categories.size(), false);
        cheatRects.resize(script->m_cheat_categories.size());
        cheatRectAnimationProgress.resize(script->m_cheat_categories.size());
        cheatTextRects.resize(script->m_cheat_categories.size());
        cheatDropdownRects.resize(script->m_cheat_categories.size());
        cheatDropdownHovered.resize(script->m_cheat_categories.size());
        cheat_positions.resize(script->m_cheat_categories.size());
        cheatTextHovered.resize(script->m_cheat_categories.size());
        cheatSettingRects.resize(script->m_cheat_categories.size());
        cheatSliderRects.resize(script->m_cheat_categories.size());
        cheatSliderBarRects.resize(script->m_cheat_categories.size());
        cheatSettingTextRects.resize(script->m_cheat_categories.size());
        cheatSettingColorRects.resize(script->m_cheat_categories.size());
        cheatSettingPlusRects.resize(script->m_cheat_categories.size());
        cheatSettingClearRects.resize(script->m_cheat_categories.size());
        cheatSettingRandomRects.resize(script->m_cheat_categories.size());
        cheatSettingSyncRects.resize(script->m_cheat_categories.size());
        cheatSettingTextFields.resize(script->m_cheat_categories.size());
        cheatSettingTextLasts.resize(script->m_cheat_categories.size());
        cheatSettingTextHovered.resize(script->m_cheat_categories.size());
        cheatSliderHovered.resize(script->m_cheat_categories.size());
        selectionBoxHovered.resize(script->m_cheat_categories.size());
        cheat_setting_positions.resize(script->m_cheat_categories.size());
        cheatSettingOptionHovered.resize(script->m_cheat_categories.size());
        cheatSettingOptionRects.resize(script->m_cheat_categories.size());

        for (size_t i = 0; i < script->m_cheat_categories.size(); ++i) {
            if (g_settings->exists("Category_Dropdown_" + std::to_string(i)) && g_settings->getBool("save_menu_category_positions")) {
                selectedCategory[i] = g_settings->getBool("Category_Dropdown_" + std::to_string(i));
            }
            
            if (g_settings->exists("Category_Position_" + std::to_string(i)) && g_settings->getBool("save_menu_category_positions")) {
                v2f position = g_settings->getV2F("Category_Position_" + std::to_string(i));
                category_positions[i] = core::position2d<s32>(position.X, position.Y);
            } else {
                category_positions[i] = core::position2d<s32>(category_height / 2, category_height / 2 + ((category_height + category_height / 2) * i));
            }
            categoryRects[i] = core::rect<s32>(category_positions[i].X, category_positions[i].Y, 
                                            category_positions[i].X + category_width, category_positions[i].Y + category_height);

            textRects[i] = core::rect<s32>(category_positions[i].X, category_positions[i].Y, 
                                        category_positions[i].X + (category_width - category_height), category_positions[i].Y + category_height);

            dropdownRects[i] = core::rect<s32>(category_positions[i].X + (category_width - category_height), category_positions[i].Y, 
                                            category_positions[i].X + category_width, category_positions[i].Y + category_height);
                                    
            cheatRects[i].resize(script->m_cheat_categories[i]->m_cheats.size(), core::rect<s32>(0,0,0,0));
            cheatRectAnimationProgress[i].resize(script->m_cheat_categories[i]->m_cheats.size(), 1.0f);
            cheatTextRects[i].resize(script->m_cheat_categories[i]->m_cheats.size(), core::rect<s32>(0,0,0,0));
            cheatDropdownRects[i].resize(script->m_cheat_categories[i]->m_cheats.size(), core::rect<s32>(0,0,0,0));
            cheatDropdownHovered[i].resize(script->m_cheat_categories[i]->m_cheats.size(), false);
            cheatTextHovered[i].resize(script->m_cheat_categories[i]->m_cheats.size(), false);
            selectedCheat[i].resize(script->m_cheat_categories[i]->m_cheats.size(), false);
            cheat_positions[i].resize(script->m_cheat_categories[i]->m_cheats.size(), core::position2d<s32>(0, 0));
            cheatSettingRects[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSliderRects[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSliderBarRects[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            selectionBoxRects[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSettingTextRects[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSettingColorRects[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSettingPlusRects[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSettingClearRects[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSettingRandomRects[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSettingSyncRects[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSettingTextFields[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSettingTextLasts[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSettingTextHovered[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSliderHovered[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            selectionBoxHovered[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheat_setting_positions[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSettingOptionHovered[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSettingOptionRects[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            for (size_t c = 0; c < script->m_cheat_categories[i]->m_cheats.size(); ++c) {
                if (g_settings->exists("Cheat_Dropdown_" + std::to_string(i) + "_" + std::to_string(c)) && g_settings->getBool("save_menu_category_positions")) {
                    selectedCheat[i][c] = g_settings->getBool("Cheat_Dropdown_" + std::to_string(i) + "_" + std::to_string(c));
                }
                cheatRectAnimationProgress[i][c] = static_cast<float>(!script->m_cheat_categories[i]->m_cheats[c]->is_enabled());
                cheatSettingRects[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSliderRects[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                selectionBoxRects[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSliderBarRects[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSettingTextRects[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSettingColorRects[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSettingPlusRects[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSettingClearRects[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSettingRandomRects[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSettingSyncRects[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSettingTextFields[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSettingTextLasts[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSettingTextHovered[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size(), false);
                cheatSliderHovered[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size(), false);
                selectionBoxHovered[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size(), false);
                cheat_setting_positions[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSettingOptionHovered[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSettingOptionRects[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                for (size_t s = 0; s < script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size(); ++s) {
                    if (script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_type == "text") {
                        std::wstring wname = cachedWideFromUtf8(g_settings->get(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_setting));
                        cheatSettingTextLasts[i][c][s] = wname;
                        cheatSettingTextFields[i][c][s] = env->addEditBox(wname.c_str(), core::rect<s32>(0, 0, category_width, category_height), true);
                        cheatSettingTextFields[i][c][s]->setDrawBackground(false);
                        cheatSettingTextFields[i][c][s]->setWordWrap(true);
                        cheatSettingTextFields[i][c][s]->setOverrideColor(video::SColor(173, 35, 45, 56));
                        cheatSettingTextFields[i][c][s]->setVisible(false);
                    } else if (script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_type == "selectionbox") {
                        for (size_t o = 0; o < script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_options.size(); ++o) {
                            cheatSettingOptionHovered[i][c][s].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_options.size(), false);
                            cheatSettingOptionRects[i][c][s].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_options.size());
                        }
                    }
                }
                
                moveMenu(i, category_positions[i]);
            }
        }
        m_initialized = true;
    } else {
        syncTextFieldStates();
    }

    g_settings->setBool("new_menu_open", true);

    core::rect<s32> screenRect(0, 0, 
        Environment->getVideoDriver()->getScreenSize().Width, 
        Environment->getVideoDriver()->getScreenSize().Height);
    setRelativePosition(screenRect);

    IGUIElement::setVisible(true);
    Environment->setFocus(this);
    m_menumgr->createdMenu(this);
    m_is_open = true;
}

void NewMenu::close()
{
    for (size_t i = 0; i < cheatSettingTextFields.size(); ++i) {
        for (size_t c = 0; c < cheatSettingTextFields[i].size(); ++c) {
            for (size_t s = 0; s < cheatSettingTextFields[i][c].size(); ++s) {
                if (cheatSettingTextFields[i][c][s] != nullptr) {
                    cheatSettingTextFields[i][c][s]->setVisible(false);
                }
            }
        }
    }
    Environment->removeFocus(this);
    m_menumgr->deletingMenu(this);
    IGUIElement::setVisible(false);
    m_is_open = false;
    isColorSelecting = false;
    ignoreColorPickerMouseUp = false;
    colorPickerAnchorValid = false;
    g_settings->setBool("new_menu_open", false);
}

double NewMenu::roundToNearestStep(double number, double m_min, double m_max, double m_steps)
{
    double stepSize = (m_max - m_min) / (m_steps - 1);
    return m_min + stepSize * round((number - m_min) / stepSize);
}

void NewMenu::drawInterpolatedRectangle(video::IVideoDriver* driver, const core::rect<s32>& rect, video::SColor innerColor, video::SColor outerColor, float interpolation)
{
    interpolation = core::clamp(interpolation, 0.0f, 1.0f);

    if (interpolation == 1.0f) {
        driver->draw2DRectangle(outerColor, rect);
        return;
    }

    if (interpolation == 0.0f) {
        driver->draw2DRectangle(innerColor, rect);
        return;
    }

    s32 midX = (rect.UpperLeftCorner.X + rect.LowerRightCorner.X) / 2;
    s32 interpWidth = static_cast<s32>((rect.getWidth() * (1.0f - interpolation)) / 2);

    core::rect<s32> innerRect(midX - interpWidth, rect.UpperLeftCorner.Y, midX + interpWidth, rect.LowerRightCorner.Y);
    driver->draw2DRectangle(innerColor, innerRect);
    
    driver->draw2DRectangle(outerColor, core::rect<s32>(rect.UpperLeftCorner.X, rect.UpperLeftCorner.Y, innerRect.UpperLeftCorner.X, rect.LowerRightCorner.Y));
    driver->draw2DRectangle(outerColor, core::rect<s32>(innerRect.LowerRightCorner.X, rect.UpperLeftCorner.Y, rect.LowerRightCorner.X, rect.LowerRightCorner.Y));
}

void NewMenu::calculateSliderSplit(
    const core::rect<s32>& sliderRect, 
    double value, 
    double minValue, 
    double maxValue, 
    core::rect<s32>& filledRect, 
    core::rect<s32>& remainingRect
) {
    if (minValue >= maxValue) {
        filledRect = sliderRect;
        remainingRect = core::rect<s32>(0, 0, 0, 0);
    }

    value = std::max(minValue, std::min(value, maxValue));

    double ratio = (value - minValue) / (maxValue - minValue);
    int splitX = sliderRect.UpperLeftCorner.X + static_cast<int>(sliderRect.getWidth() * ratio);

    filledRect = core::rect<s32>(
        sliderRect.UpperLeftCorner.X,
        sliderRect.UpperLeftCorner.Y,
        splitX,
        sliderRect.LowerRightCorner.Y
    );

    remainingRect = core::rect<s32>(
        splitX,
        sliderRect.UpperLeftCorner.Y,
        sliderRect.LowerRightCorner.X,
        sliderRect.LowerRightCorner.Y
    );
}

double mapValue(double value, double oldMin, double oldMax, double newMin, double newMax) {
    return newMin + (value - oldMin) * (newMax - newMin) / (oldMax - oldMin);
}

double NewMenu::calculateSliderValueFromPosition(const core::rect<s32>& sliderBarRect, const core::position2d<s32>& pointerPosition, double m_min, double m_max, double m_steps)
{

    s32 clampedX = std::clamp(pointerPosition.X, sliderBarRect.UpperLeftCorner.X, sliderBarRect.LowerRightCorner.X);


    double sliderValue = mapValue(clampedX, sliderBarRect.UpperLeftCorner.X, sliderBarRect.LowerRightCorner.X, m_min, m_max);

    return roundToNearestStep(sliderValue, m_min, m_max, m_steps);
}

s32 NewMenu::respaceMenu(size_t i)
{
    GET_SCRIPT_POINTER_S32
    s32 last_height = category_positions[i].Y + category_height + 1;
    categoryRects[i] = core::rect<s32>(category_positions[i].X, category_positions[i].Y, 
                                category_positions[i].X + category_width, category_positions[i].Y + category_height);

    textRects[i] = core::rect<s32>(category_positions[i].X, category_positions[i].Y, 
                                category_positions[i].X + (category_width - category_height), category_positions[i].Y + category_height);

    dropdownRects[i] = core::rect<s32>(category_positions[i].X + (category_width - category_height), category_positions[i].Y, 
                                    category_positions[i].X + category_width, category_positions[i].Y + category_height);
    

    for (size_t c = 0; c < script->m_cheat_categories[i]->m_cheats.size(); ++c) {
        cheat_positions[i][c] = core::position2d<s32>(category_positions[i].X, last_height);
        last_height += category_height;
        cheatRects[i][c] = core::rect<s32>(cheat_positions[i][c].X, cheat_positions[i][c].Y, 
                                            cheat_positions[i][c].X + category_width, cheat_positions[i][c].Y + category_height);
        if (script->m_cheat_categories[i]->m_cheats[c]->has_settings()) {
            
            cheatTextRects[i][c] = core::rect<s32>(cheat_positions[i][c].X, cheat_positions[i][c].Y, 
                                                cheat_positions[i][c].X + (category_width - category_height), cheat_positions[i][c].Y + category_height);

            cheatDropdownRects[i][c] = core::rect<s32>(cheat_positions[i][c].X + (category_width - category_height), cheat_positions[i][c].Y, 
                                                    cheat_positions[i][c].X + category_width, cheat_positions[i][c].Y + category_height);
            if (selectedCheat[i][c]) {
                for (size_t s = 0; s < script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size(); ++s) {
                    cheat_setting_positions[i][c][s] = core::position2d<s32>(category_positions[i].X, last_height);
                    if (script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_type == "selectionbox" || script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_type == "slider_int" || script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_type == "slider_float") {
                        cheatSettingRects[i][c][s] = core::rect<s32>(cheat_setting_positions[i][c][s].X,                  cheat_setting_positions[i][c][s].Y, 
                                                                    cheat_setting_positions[i][c][s].X + category_width, cheat_setting_positions[i][c][s].Y + category_height * 2);
                    } else if (script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_type == "text") {
                        cheatSettingRects[i][c][s] = core::rect<s32>(cheat_setting_positions[i][c][s].X,                  cheat_setting_positions[i][c][s].Y, 
                                                                    cheat_setting_positions[i][c][s].X + category_width, cheat_setting_positions[i][c][s].Y + category_height * 4);
                        if (isHudColorSetting(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s])) {
                            cheatSettingRects[i][c][s].LowerRightCorner.Y =
                                cheat_setting_positions[i][c][s].Y + category_height * 2;
                        } else if (isBetterDropFilterSetting(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s])) {
                            cheatSettingRects[i][c][s].LowerRightCorner.Y =
                                cheat_setting_positions[i][c][s].Y + category_height * 3;
                        }
                        if (isChatColorSetting(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s])) {
                            cheatSettingColorRects[i][c][s] = getChatColorSwatchRect(
                                cheatSettingRects[i][c][s], category_height, setting_bar_padding, setting_bar_width);
                            cheatSettingPlusRects[i][c][s] = getChatColorPlusRect(
                                cheatSettingRects[i][c][s], category_height, setting_bar_padding, setting_bar_width);
                            cheatSettingClearRects[i][c][s] = getChatColorClearRect(
                                cheatSettingRects[i][c][s], category_height, setting_bar_padding, setting_bar_width);
                            cheatSettingRandomRects[i][c][s] = getChatColorRandomRect(
                                cheatSettingRects[i][c][s], category_height, setting_bar_padding, setting_bar_width);
                            cheatSettingTextRects[i][c][s] = getChatColorTextRect(
                                cheatSettingRects[i][c][s], category_height, setting_bar_padding, setting_bar_width);
                            cheatSettingTextFields[i][c][s]->setRelativePosition(cheatSettingTextRects[i][c][s]);
                        } else if (isBetterDropFilterSetting(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s])) {
                            cheatSettingPlusRects[i][c][s] = getInlineSettingButtonRect(
                                cheatSettingRects[i][c][s], category_height, setting_bar_padding);
                            cheatSettingTextFields[i][c][s]->setRelativePosition(core::rect<s32>(
                                cheatSettingRects[i][c][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width,
                                cheatSettingRects[i][c][s].UpperLeftCorner.Y + category_height + setting_bar_padding,
                                cheatSettingRects[i][c][s].LowerRightCorner.X - setting_bar_padding,
                                cheatSettingRects[i][c][s].LowerRightCorner.Y - setting_bar_padding
                            ));
                        } else if (isColorCheatSetting(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s])) {
                            cheatSettingTextFields[i][c][s] = nullptr;
                            if (isHudColorSetting(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s])) {
                                const bool include_sync = isHudAccentColorSetting(
                                    script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]);
                                cheatSettingTextRects[i][c][s] = getHudColorSwatchRect(
                                    cheatSettingRects[i][c][s], category_height, setting_bar_padding, setting_bar_width, include_sync);
                                cheatSettingRandomRects[i][c][s] = getHudColorRandomRect(
                                    cheatSettingRects[i][c][s], category_height, setting_bar_padding, setting_bar_width, include_sync);
                                if (include_sync) {
                                    cheatSettingSyncRects[i][c][s] = getHudColorSyncRect(
                                        cheatSettingRects[i][c][s], category_height, setting_bar_padding, setting_bar_width);
                                }
                            } else {
                                cheatSettingTextRects[i][c][s] = getColorValueRect(
                                    cheatSettingRects[i][c][s], category_height, setting_bar_padding, setting_bar_width);
                            }
                        } else {
                            cheatSettingTextFields[i][c][s]->setRelativePosition(core::rect<s32>(
                                cheatSettingRects[i][c][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width,
                                cheatSettingRects[i][c][s].UpperLeftCorner.Y + category_height,
                                cheatSettingRects[i][c][s].LowerRightCorner.X - setting_bar_padding,
                                cheatSettingRects[i][c][s].LowerRightCorner.Y - setting_bar_padding
                            ));
                        }
                    } else {
                        cheatSettingRects[i][c][s] = core::rect<s32>(cheat_setting_positions[i][c][s].X,                  cheat_setting_positions[i][c][s].Y, 
                                                                    cheat_setting_positions[i][c][s].X + category_width, cheat_setting_positions[i][c][s].Y + category_height);
                        cheatSettingTextRects[i][c][s] = core::rect<s32>(cheat_setting_positions[i][c][s].X + (setting_bar_padding * 2) + setting_bar_width, cheat_setting_positions[i][c][s].Y, 
                                                                        cheat_setting_positions[i][c][s].X + category_width,                                cheat_setting_positions[i][c][s].Y + category_height);
                    }
                    
                    last_height += cheatSettingRects[i][c][s].getHeight();
                }
            }
        } else {
            cheatTextRects[i][c] = core::rect<s32>(cheat_positions[i][c].X, cheat_positions[i][c].Y, 
                                                cheat_positions[i][c].X + category_width, cheat_positions[i][c].Y + category_height);

            cheatDropdownRects[i][c] = core::rect<s32>(cheat_positions[i][c].X + category_width, cheat_positions[i][c].Y, 
                                                    cheat_positions[i][c].X + category_width, cheat_positions[i][c].Y);
        }
    }
    return last_height;
}

void NewMenu::moveMenu(size_t i, core::position2d<s32> new_position)
{
    respaceMenu(i);
    category_positions[i] = new_position;
    s32 newX = category_positions[i].X;
    s32 newY = category_positions[i].Y;

    if (newX < 0) {
        newX = 0;
    }
    for (size_t c = 0; c < cheatSettingTextFields[i].size(); ++c) {
        g_settings->setBool("Category_Dropdown_" + std::to_string(i), selectedCategory[i]);
        for (size_t s = 0; s < cheatSettingTextFields[i][c].size(); ++s) {
            g_settings->setBool("Cheat_Dropdown_" + std::to_string(i) + "_" + std::to_string(c), selectedCheat[i][c]); 
        }
    }
    if (newY < 0) {
        newY = 0;
    }
    if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid") == true) {
        newX = std::round(newX / (category_height / 2)) * (category_height / 2);
        newY = std::round(newY / (category_height / 2)) * (category_height / 2);
    }

    category_positions[i] = core::position2d<s32>(newX, newY);
    
    respaceMenu(i);
    syncTextFieldStates();

    g_settings->setV2F("Category_Position_" + std::to_string(i), v2f(newX, newY));
}

bool NewMenu::OnEvent(const irr::SEvent& event) 
{
    if (!m_is_open) {
        return false;
    }
    GET_SCRIPT_POINTER_BOOL  
    if (event.EventType == irr::EET_KEY_INPUT_EVENT)
    {
        if (event.KeyInput.Key == KEY_ESCAPE)
        {
            close();
            return true; 
        }
    }
    
    setWidthFromMultiplier(g_settings->getS32("WidthMultiplier"));
    adjustCategoryPositions();
    
    if (event.EventType == irr::EET_MOUSE_INPUT_EVENT && !isEditing) {
        if (isColorSelecting) {
            return handleColorPickerEvent(event);
        }

        if (event.MouseInput.Event == irr::EMIE_LMOUSE_PRESSED_DOWN) {
            if (isSelecting) {
                for (size_t o = 0; o < script->m_cheat_categories[selectingCategoryIndex]->m_cheats[selectingCheatIndex]->m_cheat_settings[selectingSettingIndex]->m_options.size(); ++o) {
                    if (cheatSettingOptionRects[selectingCategoryIndex][selectingCheatIndex][selectingSettingIndex][o].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                        const std::string setting_id = script->m_cheat_categories[selectingCategoryIndex]->m_cheats[selectingCheatIndex]->m_cheat_settings[selectingSettingIndex]->m_setting;
                        g_settings->set(setting_id,
                            script->m_cheat_categories[selectingCategoryIndex]->m_cheats[selectingCheatIndex]->m_cheat_settings[selectingSettingIndex]->m_options[o]->c_str());
                        if (setting_id == "ColorTheme") {
                            current_theme_name = g_settings->get("ColorTheme");
                            current_theme = theme_manager.GetThemeByName(current_theme_name);
                            setColorsFromTheme(current_theme);
                            syncHudAccentFromTheme(current_theme);
                            applyAppearanceOverrides();
                        } else if (setting_id == "hud.accent_color" ||
                                setting_id == "globalcolor" ||
                                setting_id == "global_color" ||
                                setting_id == "hud_color" ||
                                setting_id == "hudcolor") {
                            applyAppearanceOverrides();
                        }
                    }
                    if(script->m_cheat_categories[selectingCategoryIndex]->m_cheats[selectingCheatIndex]->m_cheat_settings[selectingSettingIndex]->m_setting == "WidthMultiplier") {
                        s32 multiplier = g_settings->getS32("WidthMultiplier");
                        setWidthFromMultiplier(multiplier);
                    }
                }

                isSelecting = false;
                syncTextFieldStates();
                return true;
            }

            if (editHUDbuttonBounds.isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                isEditing = !isEditing;
                syncTextFieldStates();
                return true;
            }

            for (size_t i = 0; i < script->m_cheat_categories.size(); ++i) {
                if (dropdownRects[i].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                    selectedCategory[i] = !selectedCategory[i];

                    moveMenu(i, category_positions[i]);
                    return true;
                }

                if (textRects[i].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                    isDragging = true;
                    draggedRectIndex = i;
                    offset = core::vector2d<s32>(event.MouseInput.X - textRects[i].UpperLeftCorner.X, event.MouseInput.Y - textRects[i].UpperLeftCorner.Y);
                    return true; 
                }
                if (selectedCategory[i]) {
                    for (size_t c = 0; c < script->m_cheat_categories[i]->m_cheats.size(); ++c) {
                        if (cheatTextRects[i][c].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                            script->toggle_cheat(script->m_cheat_categories[i]->m_cheats[c]);
                            if (script->m_cheat_categories[i]->m_cheats[c]->m_setting == "hud.enabled")
                                applyAppearanceOverrides();
                            return true;
                        }

                        if (cheatDropdownRects[i][c].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                            selectedCheat[i][c] = !selectedCheat[i][c];
                            
                            moveMenu(i, category_positions[i]);
                            return true; 
                        }

                        for (size_t s = 0; s < script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size(); ++s) {
                            if (selectedCheat[i][c]) {
                        const std::string setting_id = script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_setting;
                        if (isChatColorSetting(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s])) {
                            const core::rect<s32> color_rect = cheatSettingColorRects[i][c][s];
                            const core::rect<s32> plus_rect = cheatSettingPlusRects[i][c][s];
                            const core::rect<s32> clear_rect = cheatSettingClearRects[i][c][s];
                            const core::rect<s32> random_rect = cheatSettingRandomRects[i][c][s];
                            const core::vector2d<s32> pointer(event.MouseInput.X, event.MouseInput.Y);
                            if (color_rect.isPointInside(pointer)) {
                                selectingColorStopIndex = pickChatColorStopIndex(g_settings->get(setting_id), color_rect, pointer);
                                if (isColorSelecting &&
                                        selectingColorCategoryIndex == static_cast<int>(i) &&
                                        selectingColorCheatIndex == static_cast<int>(c) &&
                                        selectingColorSettingIndex == static_cast<int>(s)) {
                                    isColorSelecting = false;
                                    ignoreColorPickerMouseUp = false;
                                    colorPickerAnchorValid = false;
                                } else {
                                    isColorSelecting = true;
                                    ignoreColorPickerMouseUp = true;
                                    selectingColorCategoryIndex = static_cast<int>(i);
                                    selectingColorCheatIndex = static_cast<int>(c);
                                    selectingColorSettingIndex = static_cast<int>(s);
                                    colorPickerAnchor = core::position2d<s32>(event.MouseInput.X, event.MouseInput.Y);
                                    colorPickerAnchorValid = true;
                                }
                                return true;
                            }
                            if (plus_rect.isPointInside(pointer)) {
                                const std::string new_value = appendChatColorStop(g_settings->get(setting_id));
                                g_settings->set(setting_id, new_value);
                                if (cheatSettingTextFields[i][c][s] != nullptr) {
                                    const std::wstring wide_value = utf8_to_wide(new_value);
                                    cheatSettingTextFields[i][c][s]->setText(wide_value.c_str());
                                    cheatSettingTextLasts[i][c][s] = wide_value;
                                }
                                return true;
                            }
                            if (clear_rect.isPointInside(pointer)) {
                                const std::string new_value = clearChatColorStops();
                                g_settings->set(setting_id, new_value);
                                if (cheatSettingTextFields[i][c][s] != nullptr) {
                                    const std::wstring wide_value = utf8_to_wide(new_value);
                                    cheatSettingTextFields[i][c][s]->setText(wide_value.c_str());
                                    cheatSettingTextLasts[i][c][s] = wide_value;
                                }
                                selectingColorStopIndex = 0;
                                return true;
                            }
                            if (random_rect.isPointInside(pointer)) {
                                const std::string new_value = randomChatColorStops();
                                g_settings->set(setting_id, new_value);
                                if (cheatSettingTextFields[i][c][s] != nullptr) {
                                    const std::wstring wide_value = utf8_to_wide(new_value);
                                    cheatSettingTextFields[i][c][s]->setText(wide_value.c_str());
                                    cheatSettingTextLasts[i][c][s] = wide_value;
                                }
                                selectingColorStopIndex = 0;
                                return true;
                            }
                        } else if (isBetterDropFilterSetting(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s])) {
                            const core::rect<s32> plus_rect = cheatSettingPlusRects[i][c][s];
                            const core::vector2d<s32> pointer(event.MouseInput.X, event.MouseInput.Y);
                            if (plus_rect.isPointInside(pointer)) {
                                std::string item_name;
                                if (m_client) {
                                    if (LocalPlayer *player = m_client->getEnv().getLocalPlayer()) {
                                        ItemStack selected_item;
                                        player->getWieldedItem(&selected_item, nullptr);
                                        item_name = selected_item.name;
                                    }
                                }
                                const std::string new_value = appendUniqueCsvValue(g_settings->get(setting_id), item_name);
                                g_settings->set(setting_id, new_value);
                                if (cheatSettingTextFields[i][c][s] != nullptr) {
                                    const std::wstring wide_value = utf8_to_wide(new_value);
                                    cheatSettingTextFields[i][c][s]->setText(wide_value.c_str());
                                    cheatSettingTextLasts[i][c][s] = wide_value;
                                }
                                return true;
                            }
                        } else if (isHudColorSetting(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s])) {
                            const core::vector2d<s32> pointer(event.MouseInput.X, event.MouseInput.Y);
                            const core::rect<s32> color_rect = cheatSettingTextRects[i][c][s];
                            const core::rect<s32> random_rect = cheatSettingRandomRects[i][c][s];
                            const core::rect<s32> sync_rect = cheatSettingSyncRects[i][c][s];
                            if (color_rect.isPointInside(pointer)) {
                                selectingColorStopIndex = 0;
                                if (isColorSelecting &&
                                        selectingColorCategoryIndex == static_cast<int>(i) &&
                                        selectingColorCheatIndex == static_cast<int>(c) &&
                                        selectingColorSettingIndex == static_cast<int>(s)) {
                                    isColorSelecting = false;
                                    ignoreColorPickerMouseUp = false;
                                    colorPickerAnchorValid = false;
                                } else {
                                    isColorSelecting = true;
                                    ignoreColorPickerMouseUp = true;
                                    selectingColorCategoryIndex = static_cast<int>(i);
                                    selectingColorCheatIndex = static_cast<int>(c);
                                    selectingColorSettingIndex = static_cast<int>(s);
                                    colorPickerAnchor = core::position2d<s32>(event.MouseInput.X, event.MouseInput.Y);
                                    colorPickerAnchorValid = true;
                                }
                                return true;
                            }
                            if (random_rect.isPointInside(pointer)) {
                                const std::string new_value = colorToHex(randomChatColor());
                                g_settings->set(setting_id, new_value);
                                applyAppearanceOverrides();
                                return true;
                            }
                            if (isHudAccentColorSetting(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]) &&
                                    sync_rect.isPointInside(pointer)) {
                                syncHudColorTargets(setting_id);
                                return true;
                            }
                        } else if (isColorCheatSetting(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s])) {
                            const core::vector2d<s32> pointer(event.MouseInput.X, event.MouseInput.Y);
                            if (cheatSettingTextRects[i][c][s].isPointInside(pointer)) {
                                selectingColorStopIndex = 0;
                                if (isColorSelecting &&
                                        selectingColorCategoryIndex == static_cast<int>(i) &&
                                        selectingColorCheatIndex == static_cast<int>(c) &&
                                        selectingColorSettingIndex == static_cast<int>(s)) {
                                    isColorSelecting = false;
                                    ignoreColorPickerMouseUp = false;
                                    colorPickerAnchorValid = false;
                                } else {
                                    isColorSelecting = true;
                                    ignoreColorPickerMouseUp = true;
                                    selectingColorCategoryIndex = static_cast<int>(i);
                                    selectingColorCheatIndex = static_cast<int>(c);
                                    selectingColorSettingIndex = static_cast<int>(s);
                                    colorPickerAnchor = core::position2d<s32>(event.MouseInput.X, event.MouseInput.Y);
                                    colorPickerAnchorValid = true;
                                }
                                return true;
                            }
                        }
                        if (cheatSettingTextRects[i][c][s].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                            if (script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_type == "selectionbox") {
                                selectingCategoryIndex = i;
                                selectingCheatIndex = c;
                                selectingSettingIndex = s;
                                isSelecting = true;
                                syncTextFieldStates();
                                return true;
                            }

                            if (script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_setting == "ReloadThemes") {
                                theme_manager.LoadThemes(themes_path);
                                m_initialized = false;
                                create();
                                    } else {
                                        script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->toggle();
                                        const std::string setting_id = script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_setting;
                                        if (setting_id == "hud.accent_color" ||
                                                setting_id == "globalcolor" ||
                                                setting_id == "global_color" ||
                                                setting_id == "hud_color" ||
                                                setting_id == "hudcolor") {
                                            applyAppearanceOverrides();
                                        }
                                    }
                                }
                                if (cheatSliderRects[i][c][s].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y)) || cheatSliderBarRects[i][c][s].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                                    draggedSliderCategoryIndex = i;
                                    draggedSliderCheatIndex = c;
                                    draggedSliderSettingIndex = s;
                                    isSliding = true;
                                    ScriptApiCheatsCheatSetting* cheatSetting = script->m_cheat_categories[draggedSliderCategoryIndex]->m_cheats[draggedSliderCheatIndex]->m_cheat_settings[draggedSliderSettingIndex];
                                    cheatSetting->set_value(calculateSliderValueFromPosition(cheatSliderBarRects[draggedSliderCategoryIndex][draggedSliderCheatIndex][draggedSliderSettingIndex], core::position2d<s32>(event.MouseInput.X, event.MouseInput.Y), cheatSetting->m_min, cheatSetting->m_max, cheatSetting->m_steps));
                                }
                                if (selectionBoxRects[i][c][s].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                                    selectingCategoryIndex = i;
                                    selectingCheatIndex = c;
                                    selectingSettingIndex = s;
                                    isSelecting = true;
                                    syncTextFieldStates();
                                }
                            }
                        }
                    }
                }
            }
            return false; 
        } else if (event.MouseInput.Event == irr::EMIE_LMOUSE_LEFT_UP) {
            isDragging = false;
            isSliding = false;
            draggedRectIndex = -1;
            draggedSliderCategoryIndex = 0;
            draggedSliderCheatIndex = 0;
            draggedSliderSettingIndex = 0;
            return true;
        } else if (event.MouseInput.Event == irr::EMIE_MOUSE_MOVED && isDragging && draggedRectIndex != -1) {
            moveMenu(draggedRectIndex, core::vector2d<s32>(event.MouseInput.X - offset.X, event.MouseInput.Y - offset.Y));
            return true;
        } else if (event.MouseInput.Event == irr::EMIE_MOUSE_MOVED && isSliding) {
            ScriptApiCheatsCheatSetting* cheatSetting = script->m_cheat_categories[draggedSliderCategoryIndex]->m_cheats[draggedSliderCheatIndex]->m_cheat_settings[draggedSliderSettingIndex];
            cheatSetting->set_value(calculateSliderValueFromPosition(cheatSliderBarRects[draggedSliderCategoryIndex][draggedSliderCheatIndex][draggedSliderSettingIndex], core::position2d<s32>(event.MouseInput.X, event.MouseInput.Y), cheatSetting->m_min, cheatSetting->m_max, cheatSetting->m_steps));
        } else if (event.MouseInput.Event == irr::EMIE_MOUSE_MOVED) {
            isEditingHovered = editHUDbuttonBounds.isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));

            for (size_t i = 0; i < script->m_cheat_categories.size(); ++i) {

                dropdownHovered[i] = dropdownRects[i].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));

                textHovered[i] = textRects[i].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));

                for (size_t c = 0; c < script->m_cheat_categories[i]->m_cheats.size(); ++c) {

                    cheatTextHovered[i][c] = cheatTextRects[i][c].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));

                    cheatDropdownHovered[i][c] = cheatDropdownRects[i][c].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));

                    for (size_t s = 0; s < script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size(); ++s) {
                        cheatSettingTextHovered[i][c][s] = cheatSettingTextRects[i][c][s].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));
                        cheatSliderHovered[i][c][s] = cheatSliderRects[i][c][s].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));
                        selectionBoxHovered[i][c][s] = selectionBoxRects[i][c][s].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));

                        for (size_t o = 0; o < script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_options.size(); ++o) {
                            cheatSettingOptionHovered[i][c][s][o] = cheatSettingOptionRects[i][c][s][o].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));
                        }
                    }
                }
            }
        }
    } else if (event.EventType == irr::EET_MOUSE_INPUT_EVENT) {
        if (event.MouseInput.Event == irr::EMIE_LMOUSE_PRESSED_DOWN) {
            if (editHUDbuttonBounds.isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                isEditing = !isEditing;
                syncTextFieldStates();
                return true;
            }
            for (size_t e = hudElements.size(); e > 0; --e) {
                const size_t idx = e - 1;
                if (hudElements[idx]->resizeBounds.isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                    isResizingHUDElement = true;
                    resizingHUDElementIndex = idx;
                    resizingHUDElementOffset = core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y) - hudElements[idx]->bounds.LowerRightCorner;
                    return true;
                } else if (hudElements[idx]->bounds.isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y)))
                {
                    isDraggingHUDElement = true;
                    draggingHUDElementIndex = idx;
                    draggingHUDElementOffset = hudElements[idx]->bounds.UpperLeftCorner - core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y);
                    return true;
                }
            }
        } else if (event.MouseInput.Event == irr::EMIE_LMOUSE_LEFT_UP) {
            isResizingHUDElement = false;
            isDraggingHUDElement = false;
        } else if (event.MouseInput.Event == irr::EMIE_MOUSE_MOVED) {
            isEditingHovered = editHUDbuttonBounds.isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));

            if (isResizingHUDElement) {
                if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid")) {
                    hudElements[resizingHUDElementIndex]->bounds = normalizeRect(core::rect<s32>(
                        roundToGrid(hudElements[resizingHUDElementIndex]->bounds.UpperLeftCorner.X),
                        roundToGrid(hudElements[resizingHUDElementIndex]->bounds.UpperLeftCorner.Y),
                        roundToGrid(event.MouseInput.X + resizingHUDElementOffset.X),
                        roundToGrid(event.MouseInput.Y + resizingHUDElementOffset.Y)
                    ));
                } else {
                    hudElements[resizingHUDElementIndex]->bounds = normalizeRect(core::rect<s32>(
                        hudElements[resizingHUDElementIndex]->bounds.UpperLeftCorner.X,
                        hudElements[resizingHUDElementIndex]->bounds.UpperLeftCorner.Y,
                        event.MouseInput.X + resizingHUDElementOffset.X,
                        event.MouseInput.Y + resizingHUDElementOffset.Y
                    ));
                }
                hudElements[resizingHUDElementIndex]->resizeBounds = core::rect<s32>(
                    hudElements[resizingHUDElementIndex]->bounds.LowerRightCorner.X - 5,
                    hudElements[resizingHUDElementIndex]->bounds.LowerRightCorner.Y - 5,
                    hudElements[resizingHUDElementIndex]->bounds.LowerRightCorner.X + 5,
                    hudElements[resizingHUDElementIndex]->bounds.LowerRightCorner.Y + 5
                );

                g_settings->setV2F("HudElement_Position1_" + hudElements[resizingHUDElementIndex]->elementName, v2f(hudElements[resizingHUDElementIndex]->bounds.UpperLeftCorner.X, hudElements[resizingHUDElementIndex]->bounds.UpperLeftCorner.Y));
                g_settings->setV2F("HudElement_Position2_" + hudElements[resizingHUDElementIndex]->elementName, v2f(hudElements[resizingHUDElementIndex]->bounds.LowerRightCorner.X, hudElements[resizingHUDElementIndex]->bounds.LowerRightCorner.Y));
            } else if (isDraggingHUDElement) {
                s32 rectWidth = hudElements[draggingHUDElementIndex]->bounds.getWidth();
                s32 rectHeight = hudElements[draggingHUDElementIndex]->bounds.getHeight();
                if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid")) {
                    hudElements[draggingHUDElementIndex]->bounds = normalizeRect(core::rect<s32>(
                        roundToGrid(event.MouseInput.X + draggingHUDElementOffset.X),
                        roundToGrid(event.MouseInput.Y + draggingHUDElementOffset.Y),
                        roundToGrid(event.MouseInput.X + draggingHUDElementOffset.X + rectWidth),
                        roundToGrid(event.MouseInput.Y + draggingHUDElementOffset.Y + rectHeight)
                    ));
                } else {
                    hudElements[draggingHUDElementIndex]->bounds = normalizeRect(core::rect<s32>(
                        event.MouseInput.X + draggingHUDElementOffset.X,
                        event.MouseInput.Y + draggingHUDElementOffset.Y,
                        event.MouseInput.X + draggingHUDElementOffset.X + rectWidth,
                        event.MouseInput.Y + draggingHUDElementOffset.Y + rectHeight
                    ));
                }
                hudElements[draggingHUDElementIndex]->resizeBounds = core::rect<s32>(
                    hudElements[draggingHUDElementIndex]->bounds.LowerRightCorner.X - 5,
                    hudElements[draggingHUDElementIndex]->bounds.LowerRightCorner.Y - 5,
                    hudElements[draggingHUDElementIndex]->bounds.LowerRightCorner.X + 5,
                    hudElements[draggingHUDElementIndex]->bounds.LowerRightCorner.Y + 5
                );
                g_settings->setV2F("HudElement_Position1_" + hudElements[draggingHUDElementIndex]->elementName, v2f(hudElements[draggingHUDElementIndex]->bounds.UpperLeftCorner.X, hudElements[draggingHUDElementIndex]->bounds.UpperLeftCorner.Y));
                g_settings->setV2F("HudElement_Position2_" + hudElements[draggingHUDElementIndex]->elementName, v2f(hudElements[draggingHUDElementIndex]->bounds.LowerRightCorner.X, hudElements[draggingHUDElementIndex]->bounds.LowerRightCorner.Y));
            }
            
            for (auto element : hudElements) {
                element->isResizeHovered = element->resizeBounds.isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));
                element->isHovered = (element->bounds.isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y)) && !element->isResizeHovered);
            }
            return true;
        }
    }
    
    return Parent ? Parent->OnEvent(event) : false; 
}

void NewMenu::drawCategory(video::IVideoDriver* driver, gui::IGUIFont* font, const size_t i, float dtime)
{
    GET_SCRIPT_POINTER
    // TODO REMOVE THESE AND STORE THEM AS SETTINGS
    const s32 unit_size = category_height / 9;

    const video::SColor rainbow_category_color = video::SColor(255, 21, 21, 21);
    const video::SColor rainbow_cheat_color = getNextRainbowColor();
    const video::SColor rainbow_cheat_color_enabled = video::SColor(255, 21, 21, 21);
    const video::SColor rainbow_outline_color = getNextRainbowColor();
    
    ///////////////////////// DRAW CATEGORY HEADER /////////////////////////
    video::SColor arrow_color = current_theme.text;
    video::SColor text_color = current_theme.text;
    if (dropdownHovered[i]) {
        arrow_color = current_theme.text_muted;
    }
    if (textHovered[i]) {
        text_color = current_theme.text_muted;
    }
    if (g_settings->get("ColorTheme") == "Rainbow") {
        driver->draw2DRectangle(rainbow_category_color, categoryRects[i]);
        driver->draw2DRectangleOutline(core::rect<s32>(categoryRects[i].UpperLeftCorner.X, categoryRects[i].UpperLeftCorner.Y, categoryRects[i].LowerRightCorner.X - 1, categoryRects[i].LowerRightCorner.Y - 1), rainbow_outline_color);
        driver->draw2DRectangleOutline(core::rect<s32>(categoryRects[i].UpperLeftCorner.X - 1, categoryRects[i].UpperLeftCorner.Y - 1, categoryRects[i].LowerRightCorner.X, categoryRects[i].LowerRightCorner.Y), rainbow_outline_color);
    } else {
        driver->draw2DRectangle(current_theme.background, categoryRects[i]);
        driver->draw2DRectangleOutline(core::rect<s32>(categoryRects[i].UpperLeftCorner.X, categoryRects[i].UpperLeftCorner.Y, categoryRects[i].LowerRightCorner.X - 1, categoryRects[i].LowerRightCorner.Y - 1), current_theme.border);
        driver->draw2DRectangleOutline(core::rect<s32>(categoryRects[i].UpperLeftCorner.X - 1, categoryRects[i].UpperLeftCorner.Y - 1, categoryRects[i].LowerRightCorner.X, categoryRects[i].LowerRightCorner.Y), current_theme.border);
    }

    const std::string& categoryName = script->m_cheat_categories[i]->m_name;
    const std::wstring &wCategoryName = cachedWideFromUtf8(categoryName);
    core::dimension2d<u32> textSizeU32 = cachedTextDimension(font, wCategoryName);
    core::dimension2d<s32> textSize(textSizeU32.Width, textSizeU32.Height);

    s32 textX = textRects[i].UpperLeftCorner.X + (textRects[i].getWidth() - textSize.Width) / 2;
    s32 textY = textRects[i].UpperLeftCorner.Y + (textRects[i].getHeight() - textSize.Height) / 2;


    font->draw(wCategoryName.c_str(), core::rect<s32>(textX, textY, textX + textSize.Width, textY + textSize.Height), text_color);

    core::position2d<s32> dropdown_center(dropdownRects[i].UpperLeftCorner.X + dropdownRects[i].getWidth() / 2 ,dropdownRects[i].UpperLeftCorner.Y + dropdownRects[i].getHeight() / 2);

    if (selectedCategory[i]) {
        
        driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, dropdown_center.Y - (unit_size * 1.5)), arrow_color);
        driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), 1 + dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y - (unit_size * 1.5)), arrow_color);
        driver->draw2DLine(core::position2d<s32>(dropdown_center.X, dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), dropdown_center.Y + (unit_size * 1.5)), arrow_color);
        driver->draw2DLine(core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), 1 + dropdown_center.Y + (unit_size * 1.5)), arrow_color);
        ///////////////////////// DRAW CHEATS /////////////////////////
        for (size_t cheat_index = 0; cheat_index < script->m_cheat_categories[i]->m_cheats.size(); ++cheat_index) {

            arrow_color = current_theme.text;
            text_color = current_theme.text;
            if (cheatDropdownHovered[i][cheat_index]) {
                arrow_color = current_theme.text_muted;
            }
            if (cheatTextHovered[i][cheat_index]) {
                text_color = current_theme.text_muted;
            }

            
            if (!script->m_cheat_categories[i]->m_cheats[cheat_index]->is_enabled() && cheatRectAnimationProgress[i][cheat_index] <= 1) {
                cheatRectAnimationProgress[i][cheat_index] += dtime * 8;
            } else if (script->m_cheat_categories[i]->m_cheats[cheat_index]->is_enabled() && cheatRectAnimationProgress[i][cheat_index] >= 0) {
                cheatRectAnimationProgress[i][cheat_index] -= dtime * 8;
            }
            if (g_settings->get("ColorTheme") == "Rainbow") {
                drawInterpolatedRectangle(driver, cheatRects[i][cheat_index], rainbow_cheat_color, rainbow_cheat_color_enabled, cheatRectAnimationProgress[i][cheat_index]);
            } else {
                drawInterpolatedRectangle(driver, cheatRects[i][cheat_index], current_theme.primary, current_theme.background_bottom, cheatRectAnimationProgress[i][cheat_index]);
            }

            const std::string& cheatName = script->m_cheat_categories[i]->m_cheats[cheat_index]->m_name;
            const std::wstring &wCheatName = cachedWideFromUtf8(cheatName);
            textSizeU32 = cachedTextDimension(font, wCheatName);
            core::dimension2d<s32> textSize(textSizeU32.Width, textSizeU32.Height);
            s32 textX = cheatTextRects[i][cheat_index].UpperLeftCorner.X + (cheatTextRects[i][cheat_index].getWidth() - textSize.Width) / 2;
            s32 textY = cheatTextRects[i][cheat_index].UpperLeftCorner.Y + (cheatTextRects[i][cheat_index].getHeight() - textSize.Height) / 2;

            font->draw(wCheatName.c_str(), core::rect<s32>(textX, textY, textX + textSize.Width, textY + textSize.Height), text_color);
            if (script->m_cheat_categories[i]->m_cheats[cheat_index]->has_settings()) {
                core::position2d<s32> dropdown_center(cheatDropdownRects[i][cheat_index].UpperLeftCorner.X + cheatDropdownRects[i][cheat_index].getWidth() / 2, 
                                                    cheatDropdownRects[i][cheat_index].UpperLeftCorner.Y + cheatDropdownRects[i][cheat_index].getHeight() / 2);

                if (selectedCheat[i][cheat_index]) {
                    driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, dropdown_center.Y - (unit_size * 1.5)), arrow_color);
                    driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), 1 + dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y - (unit_size * 1.5)), arrow_color);
                    driver->draw2DLine(core::position2d<s32>(dropdown_center.X, dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), dropdown_center.Y + (unit_size * 1.5)), arrow_color);
                    driver->draw2DLine(core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), 1 + dropdown_center.Y + (unit_size * 1.5)), arrow_color);
                    ///////////////////////// DRAW CHEAT SETTINGS /////////////////////////

                    driver->draw2DRectangle(settingBackgroundColor, core::rect<s32>(cheatSettingRects[i][cheat_index][0].UpperLeftCorner.X,
                                                                                    cheatSettingRects[i][cheat_index][0].UpperLeftCorner.Y, 
                                                                                    cheatSettingRects[i][cheat_index][cheatSettingRects[i][cheat_index].size() - 1].LowerRightCorner.X,
                                                                                    cheatSettingRects[i][cheat_index][cheatSettingRects[i][cheat_index].size() - 1].LowerRightCorner.Y));
                    if (g_settings->get("ColorTheme") == "Rainbow") {
                        driver->draw2DRectangle(rainbow_cheat_color, core::rect<s32>(cheatSettingRects[i][cheat_index][0].UpperLeftCorner.X + setting_bar_padding,
                                                                            cheatSettingRects[i][cheat_index][0].UpperLeftCorner.Y + setting_bar_padding, 
                                                                            cheatSettingRects[i][cheat_index][cheatSettingRects[i][cheat_index].size() - 1].UpperLeftCorner.X + setting_bar_width + setting_bar_padding,
                                                                            cheatSettingRects[i][cheat_index][cheatSettingRects[i][cheat_index].size() - 1].LowerRightCorner.Y - setting_bar_padding));
                    } else {
                        driver->draw2DRectangle(settingBarColor, core::rect<s32>(cheatSettingRects[i][cheat_index][0].UpperLeftCorner.X + setting_bar_padding,
                                                                            cheatSettingRects[i][cheat_index][0].UpperLeftCorner.Y + setting_bar_padding, 
                                                                            cheatSettingRects[i][cheat_index][cheatSettingRects[i][cheat_index].size() - 1].UpperLeftCorner.X + setting_bar_width + setting_bar_padding,
                                                                            cheatSettingRects[i][cheat_index][cheatSettingRects[i][cheat_index].size() - 1].LowerRightCorner.Y - setting_bar_padding));
                    }

                    for (size_t s = 0; s < script->m_cheat_categories[i]->m_cheats[cheat_index]->m_cheat_settings.size(); ++s) {
                        ScriptApiCheatsCheatSetting* cheatSetting = script->m_cheat_categories[i]->m_cheats[cheat_index]->m_cheat_settings[s];

                        text_color = current_theme.text;
                        if (cheatSetting->m_type == "selectionbox") {
                            const std::string& settingName = cheatSetting->m_name;
                            const std::wstring &wSettingName = cachedWideFromUtf8(settingName);
                            textSizeU32 = cachedTextDimension(font, wSettingName);
                            core::dimension2d<s32> textSize(textSizeU32.Width, textSizeU32.Height);

                            core::rect<s32> settingTextRect = core::rect<s32>(cheatSettingRects[i][cheat_index][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width,
                                                                            cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y, 
                                                                            cheatSettingRects[i][cheat_index][s].LowerRightCorner.X,
                                                                            cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y + category_height);
                            if (isBetterDropFilterSetting(cheatSetting)) {
                                settingTextRect.LowerRightCorner.X =
                                    cheatSettingPlusRects[i][cheat_index][s].UpperLeftCorner.X - setting_bar_padding;
                            }

                            s32 textX = settingTextRect.UpperLeftCorner.X + (settingTextRect.getWidth() - textSize.Width) / 2;
                            s32 textY = settingTextRect.UpperLeftCorner.Y + (settingTextRect.getHeight() - textSize.Height) / 2;

                            font->draw(wSettingName.c_str(), core::rect<s32>(textX, textY, textX + textSize.Width, textY + textSize.Height), text_color);

                            arrow_color = current_theme.text;
                            if (selectionBoxHovered[i][cheat_index][s]) {
                                arrow_color = current_theme.text_muted;
                            }

                            selectionBoxRects[i][cheat_index][s] = core::rect<s32>( cheatSettingRects[i][cheat_index][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width + selection_box_padding,
                                                                                    cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y + category_height, 
                                                                                    cheatSettingRects[i][cheat_index][s].LowerRightCorner.X - selection_box_padding,
                                                                                    cheatSettingRects[i][cheat_index][s].LowerRightCorner.Y - selection_box_padding);

                            core::rect<s32> textBox = core::rect<s32>(  cheatSettingRects[i][cheat_index][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width + selection_box_padding,
                                                                        cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y + category_height, 
                                                                        cheatSettingRects[i][cheat_index][s].LowerRightCorner.X - category_height,
                                                                        cheatSettingRects[i][cheat_index][s].LowerRightCorner.Y);

                            core::rect<s32> dropdownBox = core::rect<s32>(  cheatSettingRects[i][cheat_index][s].LowerRightCorner.X - category_height,
                                                                            cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y + category_height, 
                                                                            (cheatSettingRects[i][cheat_index][s].LowerRightCorner.X - selection_box_padding),
                                                                            cheatSettingRects[i][cheat_index][s].LowerRightCorner.Y - selection_box_padding);
                            if (g_settings->get("ColorTheme") == "Rainbow") {
                                driver->draw2DRectangle(rainbow_cheat_color, selectionBoxRects[i][cheat_index][s]);
                                driver->draw2DRectangleOutline(selectionBoxRects[i][cheat_index][s], rainbow_cheat_color);
                                driver->draw2DRectangleOutline(dropdownBox, rainbow_cheat_color);
                            } else {
                                driver->draw2DRectangle(current_theme.background_bottom, selectionBoxRects[i][cheat_index][s]);
                                driver->draw2DRectangleOutline(selectionBoxRects[i][cheat_index][s], settingBarColor);
                                driver->draw2DRectangleOutline(dropdownBox, settingBarColor);
                            }

                            core::position2d<s32> dropdown_center(dropdownBox.UpperLeftCorner.X + dropdownBox.getWidth() / 2 , dropdownBox.UpperLeftCorner.Y + dropdownBox.getHeight() / 2);
                            if (isSelecting && selectingCategoryIndex == static_cast<int>(i)
                                    && selectingCheatIndex == static_cast<int>(cheat_index)
                                    && selectingSettingIndex == static_cast<int>(s)) {
                                driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, dropdown_center.Y + (unit_size * 1.5)), arrow_color);
                                driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), 1 + dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y + (unit_size * 1.5)), arrow_color);
                                driver->draw2DLine(core::position2d<s32>(dropdown_center.X, dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), dropdown_center.Y - (unit_size * 1.5)), arrow_color);
                                driver->draw2DLine(core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), 1 + dropdown_center.Y - (unit_size * 1.5)), arrow_color);
                            } else {
                                driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, dropdown_center.Y - (unit_size * 1.5)), arrow_color);
                                driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), 1 + dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y - (unit_size * 1.5)), arrow_color);
                                driver->draw2DLine(core::position2d<s32>(dropdown_center.X, dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), dropdown_center.Y + (unit_size * 1.5)), arrow_color);
                                driver->draw2DLine(core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), 1 + dropdown_center.Y + (unit_size * 1.5)), arrow_color);
                            }
                            

                            if (!g_settings->exists(cheatSetting->m_setting)) {
                                std::string defaultValue = *(cheatSetting->m_options[0]);

                                cheatSetting->set_value(defaultValue);
                            }
                            const std::wstring &wSelectedOption = cachedWideFromUtf8(g_settings->get(cheatSetting->m_setting));
                            textSizeU32 = cachedTextDimension(font, wSelectedOption);
                            textSize = core::dimension2d<s32>(textSizeU32.Width, textSizeU32.Height);

                            textX = textBox.UpperLeftCorner.X + (textBox.getWidth() - textSize.Width) / 2;
                            textY = textBox.UpperLeftCorner.Y + (textBox.getHeight() - textSize.Height) / 2;

                            font->draw(wSelectedOption.c_str(), core::rect<s32>(textX, textY, textX + textSize.Width, textY + textSize.Height), arrow_color);
                        } else if (cheatSetting->m_type == "bool") {
                            if(g_settings->exists(cheatSetting->m_setting) && g_settings->getBool(cheatSetting->m_setting) == true) {
                                if (g_settings->get("ColorTheme") == "Rainbow") {
                                    text_color = rainbow_cheat_color;
                                } else {
                                    text_color = current_theme.primary;
                                }
                            }
                            if(cheatSettingTextHovered[i][cheat_index][s] == true) {
                                if (g_settings->exists(cheatSetting->m_setting) && g_settings->getBool(cheatSetting->m_setting) == true) {
                                    if (g_settings->get("ColorTheme") == "Rainbow") {
                                        text_color = rainbow_cheat_color;
                                    } else {
                                        text_color = current_theme.primary_muted;
                                    }
                                } else {
                                    text_color = current_theme.text_muted;
                                }
                            }
                            const std::string &settingName = cheatSetting->m_name;
                            const std::wstring &wSettingName = cachedWideFromUtf8(settingName);
                            gui::IGUIFont* dropFont = g_fontengine->getFont(
                                g_fontengine->getFontSize(FM_Standard) * 0.95, FM_Standard);
                            textSizeU32 = cachedTextDimension(dropFont, wSettingName);
                            core::dimension2d<s32> textSize(textSizeU32.Width, textSizeU32.Height);

                            core::rect<s32> settingTextRect = core::rect<s32>(cheatSettingRects[i][cheat_index][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width,
                                                                            cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y, 
                                                                            cheatSettingRects[i][cheat_index][s].LowerRightCorner.X,
                                                                            cheatSettingRects[i][cheat_index][s].LowerRightCorner.Y);

                            s32 textX = settingTextRect.UpperLeftCorner.X + (settingTextRect.getWidth() - textSize.Width) / 2;
                            s32 textY = settingTextRect.UpperLeftCorner.Y + (settingTextRect.getHeight() - textSize.Height) / 2;

                            dropFont->draw(wSettingName.c_str(), core::rect<s32>(textX, textY, textX + textSize.Width, textY + textSize.Height), text_color);
                        } else if (cheatSetting->m_type == "slider_int" || cheatSetting->m_type == "slider_float") {
                            std::string settingName = cheatSetting->m_name;

                            if (!g_settings->exists(cheatSetting->m_setting)) {
                                cheatSetting->set_value(roundToNearestStep((cheatSetting->m_min + cheatSetting->m_max) / 2, cheatSetting->m_min, cheatSetting->m_max, cheatSetting->m_steps));
                            }

                            if (cheatSetting->m_type == "slider_int") {
                                std::ostringstream oss;
                                oss << " ( " << round(g_settings->getFloat(cheatSetting->m_setting)) << " )"; 
                                settingName = settingName + oss.str();
                            } else if (cheatSetting->m_type == "slider_float") {
                                std::ostringstream oss;
                                oss.precision(1);
                                oss << std::fixed << g_settings->getFloat(cheatSetting->m_setting);
                                settingName = settingName + "( " + oss.str() + " )";
                            }
                            const std::wstring &wSettingName = cachedWideFromUtf8(settingName);
                            textSizeU32 = cachedTextDimension(font, wSettingName);
                            core::dimension2d<s32> textSize(textSizeU32.Width, textSizeU32.Height);

                            core::rect<s32> settingTextRect = core::rect<s32>(cheatSettingRects[i][cheat_index][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width,
                                                                            cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y, 
                                                                            cheatSettingRects[i][cheat_index][s].LowerRightCorner.X,
                                                                            cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y + category_height);

                            s32 textX = settingTextRect.UpperLeftCorner.X + (settingTextRect.getWidth() - textSize.Width) / 2;
                            s32 textY = settingTextRect.UpperLeftCorner.Y + (settingTextRect.getHeight() - textSize.Height) / 2;

                            font->draw(wSettingName.c_str(), core::rect<s32>(textX, textY, textX + textSize.Width, textY + textSize.Height), text_color);

                            cheatSliderBarRects[i][cheat_index][s] = core::rect<s32>(cheatSettingRects[i][cheat_index][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width + slider_width_padding,
                                                                            cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y + category_height + slider_height_padding, 
                                                                            cheatSettingRects[i][cheat_index][s].LowerRightCorner.X - slider_width_padding,
                                                                            cheatSettingRects[i][cheat_index][s].LowerRightCorner.Y - slider_height_padding);

                            core::rect<s32> filledRect;
                            core::rect<s32> remainingRect;
                            calculateSliderSplit(cheatSliderBarRects[i][cheat_index][s], g_settings->getFloat(cheatSetting->m_setting), cheatSetting->m_min, cheatSetting->m_max, filledRect, remainingRect);
                            if (g_settings->get("ColorTheme") == "Rainbow") {
                                driver->draw2DRectangle(rainbow_cheat_color, filledRect);
                                driver->draw2DRectangle(sliderBarColor, remainingRect);
                            } else {
                                driver->draw2DRectangle(sliderBarColorActive, filledRect);
                                driver->draw2DRectangle(sliderBarColor, remainingRect);
                            }


                            cheatSliderRects[i][cheat_index][s] = core::rect<s32>(filledRect.LowerRightCorner.X - slider_width_padding / 2,
                                                                                filledRect.UpperLeftCorner.Y - slider_height_padding / 1.5,
                                                                                filledRect.LowerRightCorner.X + slider_width_padding / 2,
                                                                                filledRect.LowerRightCorner.Y + slider_height_padding / 1.5);

                            if (cheatSliderHovered[i][cheat_index][s]) {
                                driver->draw2DRectangle(sliderColorActive, cheatSliderRects[i][cheat_index][s]);
                            } else {
                                driver->draw2DRectangle(sliderColor, cheatSliderRects[i][cheat_index][s]);
                            }
                        } else if (cheatSetting->m_type == "text" || cheatSetting->m_type == "color") {
                            const std::string setting_id = script->m_cheat_categories[i]->m_cheats[cheat_index]->m_cheat_settings[s]->m_setting;
                            const std::wstring &wSettingName = cachedWideFromUtf8(cheatSetting->m_name);
                            textSizeU32 = cachedTextDimension(font, wSettingName);
                            core::dimension2d<s32> textSize(textSizeU32.Width, textSizeU32.Height);

                            core::rect<s32> settingTextRect = core::rect<s32>(cheatSettingRects[i][cheat_index][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width,
                                                                            cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y, 
                                                                            cheatSettingRects[i][cheat_index][s].LowerRightCorner.X,
                                                                            cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y + category_height);

                            s32 textX = settingTextRect.UpperLeftCorner.X + (settingTextRect.getWidth() - textSize.Width) / 2;
                            s32 textY = settingTextRect.UpperLeftCorner.Y + (settingTextRect.getHeight() - textSize.Height) / 2;

                            font->draw(wSettingName.c_str(), core::rect<s32>(textX, textY, textX + textSize.Width, textY + textSize.Height), text_color);

                            if (isChatColorSetting(cheatSetting)) {
                                const core::rect<s32> color_rect = cheatSettingColorRects[i][cheat_index][s];
                                const core::rect<s32> plus_rect = cheatSettingPlusRects[i][cheat_index][s];
                                const core::rect<s32> clear_rect = cheatSettingClearRects[i][cheat_index][s];
                                const core::rect<s32> random_rect = cheatSettingRandomRects[i][cheat_index][s];
                                const std::string value = g_settings->get(setting_id);
                                driver->draw2DRectangle(current_theme.background_bottom, color_rect);
                                drawColorPreview(driver, color_rect, value, video::SColor(255, 255, 255, 255));
                                driver->draw2DRectangleOutline(color_rect, current_theme.primary, 2);

                                driver->draw2DRectangle(current_theme.background_bottom, plus_rect);
                                driver->draw2DRectangleOutline(plus_rect, current_theme.primary, 2);
                                const std::wstring plus_wide = L"+";
                                const core::dimension2d<u32> plus_size = cachedTextDimension(font, plus_wide);
                                const s32 plus_x = plus_rect.UpperLeftCorner.X + (plus_rect.getWidth() - static_cast<s32>(plus_size.Width)) / 2;
                                const s32 plus_y = plus_rect.UpperLeftCorner.Y + (plus_rect.getHeight() - static_cast<s32>(plus_size.Height)) / 2;
                                font->draw(plus_wide.c_str(), core::rect<s32>(
                                    plus_x, plus_y,
                                    plus_x + static_cast<s32>(plus_size.Width),
                                    plus_y + static_cast<s32>(plus_size.Height)),
                                    current_theme.text);

                                driver->draw2DRectangle(current_theme.background_bottom, clear_rect);
                                driver->draw2DRectangleOutline(clear_rect, current_theme.primary, 2);
                                const std::wstring clear_wide = L"x";
                                const core::dimension2d<u32> clear_size = cachedTextDimension(font, clear_wide);
                                const s32 clear_x = clear_rect.UpperLeftCorner.X + (clear_rect.getWidth() - static_cast<s32>(clear_size.Width)) / 2;
                                const s32 clear_y = clear_rect.UpperLeftCorner.Y + (clear_rect.getHeight() - static_cast<s32>(clear_size.Height)) / 2;
                                font->draw(clear_wide.c_str(), core::rect<s32>(
                                    clear_x, clear_y,
                                    clear_x + static_cast<s32>(clear_size.Width),
                                    clear_y + static_cast<s32>(clear_size.Height)),
                                    current_theme.text);

                                driver->draw2DRectangle(current_theme.background_bottom, random_rect);
                                driver->draw2DRectangleOutline(random_rect, current_theme.primary, 2);
                                const std::wstring random_wide = L"R";
                                const core::dimension2d<u32> random_size = cachedTextDimension(font, random_wide);
                                const s32 random_x = random_rect.UpperLeftCorner.X + (random_rect.getWidth() - static_cast<s32>(random_size.Width)) / 2;
                                const s32 random_y = random_rect.UpperLeftCorner.Y + (random_rect.getHeight() - static_cast<s32>(random_size.Height)) / 2;
                                font->draw(random_wide.c_str(), core::rect<s32>(
                                    random_x, random_y,
                                    random_x + static_cast<s32>(random_size.Width),
                                    random_y + static_cast<s32>(random_size.Height)),
                                    current_theme.text);

                                if (cheatSettingTextFields[i][cheat_index][s] != nullptr) {
                                    std::wstring currentText = cheatSettingTextFields[i][cheat_index][s]->getText();
                                    if (currentText != cheatSettingTextLasts[i][cheat_index][s]) {
                                        const std::string new_value = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(cheatSettingTextFields[i][cheat_index][s]->getText());
                                        g_settings->set(setting_id, new_value);
                                        cheatSettingTextLasts[i][cheat_index][s] = cheatSettingTextFields[i][cheat_index][s]->getText();
                                    }
                                    cheatSettingTextFields[i][cheat_index][s]->setOverrideColor(
                                        colorForTextField(cheatSetting->m_setting, current_theme.text)
                                    );
                                }
                            } else if (isBetterDropFilterSetting(cheatSetting)) {
                                const core::rect<s32> plus_rect = cheatSettingPlusRects[i][cheat_index][s];
                                driver->draw2DRectangle(current_theme.background_bottom, plus_rect);
                                driver->draw2DRectangleOutline(plus_rect, current_theme.primary, 2);
                                const std::wstring plus_wide = L"+";
                                const core::dimension2d<u32> plus_size = cachedTextDimension(font, plus_wide);
                                const s32 plus_x = plus_rect.UpperLeftCorner.X + (plus_rect.getWidth() - static_cast<s32>(plus_size.Width)) / 2;
                                const s32 plus_y = plus_rect.UpperLeftCorner.Y + (plus_rect.getHeight() - static_cast<s32>(plus_size.Height)) / 2;
                                font->draw(plus_wide.c_str(), core::rect<s32>(
                                    plus_x, plus_y,
                                    plus_x + static_cast<s32>(plus_size.Width),
                                    plus_y + static_cast<s32>(plus_size.Height)),
                                    current_theme.text);

                                if (cheatSettingTextFields[i][cheat_index][s] != nullptr) {
                                    std::wstring currentText = cheatSettingTextFields[i][cheat_index][s]->getText();
                                    if (currentText != cheatSettingTextLasts[i][cheat_index][s]) {
                                        const std::string new_value = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(cheatSettingTextFields[i][cheat_index][s]->getText());
                                        g_settings->set(setting_id, new_value);
                                        cheatSettingTextLasts[i][cheat_index][s] = cheatSettingTextFields[i][cheat_index][s]->getText();
                                    }
                                    cheatSettingTextFields[i][cheat_index][s]->setOverrideColor(
                                        colorForTextField(cheatSetting->m_setting, current_theme.text)
                                    );
                                }
                            } else if (isHudColorSetting(cheatSetting)) {
                                const core::rect<s32> color_rect = cheatSettingTextRects[i][cheat_index][s];
                                const core::rect<s32> random_rect = cheatSettingRandomRects[i][cheat_index][s];
                                const core::rect<s32> sync_rect = cheatSettingSyncRects[i][cheat_index][s];
                                const std::string value = g_settings->get(setting_id);
                                const video::SColor swatch_color = colorFromSettingText(value, video::SColor(255, 255, 255, 255));

                                driver->draw2DRectangle(current_theme.background_bottom, color_rect);
                                driver->draw2DRectangle(swatch_color, color_rect);
                                driver->draw2DRectangleOutline(color_rect, current_theme.primary, 2);

                                driver->draw2DRectangle(current_theme.background_bottom, random_rect);
                                driver->draw2DRectangleOutline(random_rect, current_theme.primary, 2);
                                const std::wstring random_wide = L"R";
                                const core::dimension2d<u32> random_size = cachedTextDimension(font, random_wide);
                                const s32 random_x = random_rect.UpperLeftCorner.X + (random_rect.getWidth() - static_cast<s32>(random_size.Width)) / 2;
                                const s32 random_y = random_rect.UpperLeftCorner.Y + (random_rect.getHeight() - static_cast<s32>(random_size.Height)) / 2;
                                font->draw(random_wide.c_str(), core::rect<s32>(
                                    random_x, random_y,
                                    random_x + static_cast<s32>(random_size.Width),
                                    random_y + static_cast<s32>(random_size.Height)),
                                    current_theme.text);

                                if (isHudAccentColorSetting(cheatSetting)) {
                                    driver->draw2DRectangle(current_theme.background_bottom, sync_rect);
                                    driver->draw2DRectangleOutline(sync_rect, current_theme.primary, 2);
                                    const std::wstring sync_wide = L"S";
                                    const core::dimension2d<u32> sync_size = cachedTextDimension(font, sync_wide);
                                    const s32 sync_x = sync_rect.UpperLeftCorner.X + (sync_rect.getWidth() - static_cast<s32>(sync_size.Width)) / 2;
                                    const s32 sync_y = sync_rect.UpperLeftCorner.Y + (sync_rect.getHeight() - static_cast<s32>(sync_size.Height)) / 2;
                                    font->draw(sync_wide.c_str(), core::rect<s32>(
                                        sync_x, sync_y,
                                        sync_x + static_cast<s32>(sync_size.Width),
                                        sync_y + static_cast<s32>(sync_size.Height)),
                                        current_theme.text);
                                }

                            } else if (isColorCheatSetting(cheatSetting)) {
                                const core::rect<s32> color_rect = cheatSettingTextRects[i][cheat_index][s];
                                const video::SColor swatch_color = colorFromSettingText(g_settings->get(setting_id), video::SColor(255, 255, 255, 255));
                                const core::rect<s32> inner_rect(
                                    color_rect.UpperLeftCorner.X + 4,
                                    color_rect.UpperLeftCorner.Y + 4,
                                    color_rect.LowerRightCorner.X - 4,
                                    color_rect.LowerRightCorner.Y - 4);

                                driver->draw2DRectangle(current_theme.background_bottom, color_rect);
                                driver->draw2DRectangle(swatch_color, inner_rect);
                                driver->draw2DRectangleOutline(color_rect, current_theme.primary, 2);
                                driver->draw2DRectangleOutline(inner_rect, video::SColor(255, 0, 0, 0));

                            } else if (cheatSettingTextFields[i][cheat_index][s] != nullptr) {
                                std::wstring currentText = cheatSettingTextFields[i][cheat_index][s]->getText();
                                if (currentText != cheatSettingTextLasts[i][cheat_index][s]) {
                                    const std::string new_value = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(cheatSettingTextFields[i][cheat_index][s]->getText());
                                    g_settings->set(setting_id, new_value);
                                    cheatSettingTextLasts[i][cheat_index][s] = cheatSettingTextFields[i][cheat_index][s]->getText();
                                }
                                cheatSettingTextFields[i][cheat_index][s]->setOverrideColor(
                                    colorForTextField(cheatSetting->m_setting, current_theme.text)
                                );
                            }
                        }
                    }
                } else {
                    driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, dropdown_center.Y + (unit_size * 1.5)), arrow_color);
                    driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), 1 + dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y + (unit_size * 1.5)), arrow_color);
                    driver->draw2DLine(core::position2d<s32>(dropdown_center.X, dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), dropdown_center.Y - (unit_size * 1.5)), arrow_color);
                    driver->draw2DLine(core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), 1 + dropdown_center.Y - (unit_size * 1.5)), arrow_color);
                }
                
            }
        }
    } else {
        driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, dropdown_center.Y + (unit_size * 1.5)), arrow_color);
        driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), 1 + dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y + (unit_size * 1.5)), arrow_color);
        driver->draw2DLine(core::position2d<s32>(dropdown_center.X, dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), dropdown_center.Y - (unit_size * 1.5)), arrow_color);
        driver->draw2DLine(core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), 1 + dropdown_center.Y - (unit_size * 1.5)), arrow_color);
    }
}

void NewMenu::drawSelectionBox(video::IVideoDriver * driver, gui::IGUIFont * font, const size_t i, const size_t c, const size_t s)
{
    GET_SCRIPT_POINTER

    for (size_t o = 0; o < script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_options.size(); o++) {
        cheatSettingOptionRects[i][c][s][o] = core::rect<s32>(cheatSettingRects[i][c][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width + selection_box_padding,
                                                            cheatSettingRects[i][c][s].UpperLeftCorner.Y + (category_height - selection_box_padding) + ((o+1) * category_height), 
                                                            (cheatSettingRects[i][c][s].LowerRightCorner.X - selection_box_padding),
                                                            cheatSettingRects[i][c][s].LowerRightCorner.Y + ((o+1) * category_height));

        driver->draw2DRectangle(option_color, cheatSettingOptionRects[i][c][s][o]);
        const std::wstring &wOptionName = cachedWideFromUtf8(*script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_options[o]);
        core::dimension2d<u32> textSizeU32 = cachedTextDimension(font, wOptionName);
        core::dimension2d<s32> textSize(textSizeU32.Width, textSizeU32.Height);

        s32 textX = cheatSettingOptionRects[i][c][s][o].UpperLeftCorner.X + (cheatSettingOptionRects[i][c][s][o].getWidth() - textSize.Width) / 2;
        s32 textY = cheatSettingOptionRects[i][c][s][o].UpperLeftCorner.Y + (cheatSettingOptionRects[i][c][s][o].getHeight() - textSize.Height) / 2;
        video::SColor option_text_color = current_theme.text;
        if (cheatSettingOptionHovered[i][c][s][o]) {
            option_text_color = current_theme.text_muted;
        }
        font->draw(wOptionName.c_str(), core::rect<s32>(textX, textY, textX + textSize.Width, textY + textSize.Height), option_text_color);
    }
    core::rect<s32> outlineBox = core::rect<s32>(cheatSettingRects[i][c][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width + selection_box_padding,
                                                cheatSettingRects[i][c][s].UpperLeftCorner.Y + (category_height - selection_box_padding) + category_height, 
                                                (cheatSettingRects[i][c][s].LowerRightCorner.X - selection_box_padding),
                                                cheatSettingRects[i][c][s].LowerRightCorner.Y + (script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_options.size() * category_height));
    if (g_settings->get("ColorTheme") == "Rainbow") {
        driver->draw2DRectangleOutline(outlineBox, getNextRainbowColor());
    } else {
        driver->draw2DRectangleOutline(outlineBox, settingBarColor);
    }
}

void NewMenu::drawEditHudButton(video::IVideoDriver *driver, gui::IGUIFont *font)
{
    std::wstring editHUDtext = isEditing ? L"Stop Editing" : L"Edit HUD";
    core::dimension2d<u32> textSizeU32 = cachedTextDimension(font, editHUDtext);
    core::dimension2d<s32> textSize(textSizeU32.Width, textSizeU32.Height);
    const core::dimension2d<u32> screenSize = driver->getScreenSize();
    const s32 pad_x = 18;
    const s32 pad_y = 10;
    const s32 width = textSize.Width + pad_x * 2;
    const s32 height = textSize.Height + pad_y * 2;

    editHUDbuttonBounds = core::rect<s32>(
        std::max(8, static_cast<s32>(screenSize.Width) - width - 10),
        std::max(8, static_cast<s32>(screenSize.Height) - height - 10),
        std::max(8, static_cast<s32>(screenSize.Width) - 10),
        std::max(8, static_cast<s32>(screenSize.Height) - 10)
    );

    driver->draw2DRectangle(current_theme.background, editHUDbuttonBounds);
    driver->draw2DRectangleOutline(editHUDbuttonBounds, current_theme.background_bottom, 2);

    video::SColor text_color = isEditingHovered ? current_theme.text_muted : current_theme.text;

    s32 textX = editHUDbuttonBounds.UpperLeftCorner.X + (editHUDbuttonBounds.getWidth() - textSize.Width) / 2;
    s32 textY = editHUDbuttonBounds.UpperLeftCorner.Y + (editHUDbuttonBounds.getHeight() - textSize.Height) / 2;


    font->draw(editHUDtext.c_str(), core::rect<s32>(textX, textY, textX + textSize.Width, textY + textSize.Height), text_color);
}

void NewMenu::drawHints(video::IVideoDriver* driver, gui::IGUIFont* font, const size_t i)
{
    GET_SCRIPT_POINTER

    if (selectedCategory[i]) {
        for (size_t cheat_index = 0; cheat_index < script->m_cheat_categories[i]->m_cheats.size(); ++cheat_index) {
            if (cheatTextHovered[i][cheat_index] && g_settings->getBool("use_hints") && !isSelecting) {
                const std::wstring &wCheatDes = cachedWideFromUtf8(script->m_cheat_categories[i]->m_cheats[cheat_index]->m_description);
                core::dimension2d<u32> textSizeU32_des = cachedTextDimension(font, wCheatDes);
                core::dimension2d<s32> textSize_des(textSizeU32_des.Width, textSizeU32_des.Height);
                const s32 padding = 8;

            core::dimension2d<u32> screenSize = driver->getScreenSize();

            s32 backgroundWidth = textSize_des.Width + 2 * padding;
            s32 backgroundHeight = textSize_des.Height + 2 * padding;

            s32 backgroundX = cheatRects[i][cheat_index].UpperLeftCorner.X + 
                            (cheatRects[i][cheat_index].getWidth() - backgroundWidth) / 2;
            s32 backgroundY = cheatRects[i][cheat_index].LowerRightCorner.Y + padding / 2;

            if (backgroundX < 0)
                backgroundX = 0;
            if (backgroundX + backgroundWidth > (s32)screenSize.Width)
                backgroundX = screenSize.Width - backgroundWidth;

            if (backgroundY + backgroundHeight > (s32)screenSize.Height)
                backgroundY = screenSize.Height - backgroundHeight;

            if (backgroundY < 0)
               backgroundY = 0;

                if (!wCheatDes.empty()) {
                    const core::rect<s32> tooltip_rect(backgroundX, backgroundY,
                        backgroundX + backgroundWidth, backgroundY + backgroundHeight);
                    const bool hud_style = g_settings->getBool("hud.enabled") &&
                        g_settings->getBool("hud.tooltips_theme");
                    driver->draw2DRectangle(hud_style ? readModuleBackgroundColor() :
                        current_theme.secondary_muted, tooltip_rect);
                    driver->draw2DRectangleOutline(tooltip_rect, hud_style ?
                        readModuleBorderColor() : current_theme.secondary, 2);
                }

                // Text should be inside the padded area
                s32 textXD = backgroundX + padding;
                s32 textYD = backgroundY + padding;

                font->draw(wCheatDes.c_str(), 
                        core::rect<s32>(textXD, textYD, textXD + textSize_des.Width, textYD + textSize_des.Height), 
                        current_theme.text);
            }
        }
    }
}

void NewMenu::draw() 
{
	if (!m_client)
		return;

    if (m_client == nullptr || m_client->stop_running_menu || m_client->isShutdown()) {
        return;
    }

    float dtime = getDeltaTime();

    video::IVideoDriver* driver = Environment->getVideoDriver();

    const irr::core::dimension2du screensize = driver->getScreenSize();

	gui::IGUIFont *font = g_fontengine->getFont(
		FontSpec(g_fontengine->getDefaultFontSize(), FM_Standard, false, false));

    if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid") && (isDragging || isEditing)) {
        for (size_t x = 0; x <= screensize.Width; x += category_height / 2) {
            driver->draw2DLine(core::position2d<s32>(x, 0), core::position2d<s32>(x, screensize.Height), video::SColor(50, 255, 255, 255));
        }

        for (size_t y = 0; y <= screensize.Height; y += category_height / 2) {
            driver->draw2DLine(core::position2d<s32>(0, y), core::position2d<s32>(screensize.Width, y), video::SColor(50, 255, 255, 255));
        }
    }
    
    if (m_is_open) {
        GET_SCRIPT_POINTER

        if (isEditing) {
            for (auto element : hudElements) {
                element->draw(driver, font, dtime, m_client->getEnv(), true);
                video::SColor move_color = element->isHovered ? video::SColor(127, 50, 50, 50) : video::SColor(127, 127, 127, 127);

                driver->draw2DRectangle(move_color, element->bounds);

                video::SColor resize_color = element->isResizeHovered ? video::SColor(255, 127, 127, 127) : video::SColor(255, 200, 200, 200);

                driver->draw2DRectangle(resize_color, element->resizeBounds);
            }
        } else {
            for (size_t i = 0; i < script->m_cheat_categories.size(); i++) {
                if (screensize != lastScreenSize) {
                    moveMenu(i, category_positions[i]);
                }
                drawCategory(driver, font, i, dtime);
            }
            for (size_t i = 0; i < script->m_cheat_categories.size(); i++) {
                drawHints(driver, font, i);
            }
            if (isSelecting) {
                drawSelectionBox(driver, font, selectingCategoryIndex, selectingCheatIndex, selectingSettingIndex);
            }
        }

        drawEditHudButton(driver, font);

        if (isColorSelecting &&
                selectingColorCategoryIndex >= 0 &&
                selectingColorCheatIndex >= 0 &&
                selectingColorSettingIndex >= 0) {
            drawColorPicker(driver, font,
                    static_cast<size_t>(selectingColorCategoryIndex),
                    static_cast<size_t>(selectingColorCheatIndex),
                    static_cast<size_t>(selectingColorSettingIndex));
        }
    } else if (!m_is_open) {
        isEditing = false;
        for (auto element : hudElements) {
            element->draw(driver, font, dtime, m_client->getEnv(), false);
        }
    }
}
