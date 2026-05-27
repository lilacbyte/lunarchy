// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "chatplus.h"

#include <algorithm>

#include "util/string.h"
#include "settings.h"

namespace ChatPlus {

static s32 readOffsetSetting(const char *primary, const char *legacy, s32 fallback = 0)
{
	s32 value = fallback;
	if (g_settings->getS32NoEx(primary, value))
		return value;
	if (legacy && g_settings->getS32NoEx(legacy, value))
		return value;
	return fallback;
}

static video::SColor readColorSetting(const char *primary, const char *legacy,
	const video::SColor &fallback)
{
	video::SColor color = fallback;
	if (g_settings->exists(primary) &&
			parseColorString(g_settings->get(primary), color, true, 0xff))
		return color;
	if (legacy && g_settings->exists(legacy) &&
			parseColorString(g_settings->get(legacy), color, true, 0xff))
		return color;
	return fallback;
}

HudStyle readHudStyle()
{
	HudStyle style;

	std::string enabled;
	if (g_settings->getNoEx("chatplus.enabled", enabled))
		style.enabled = enabled == "true";

	style.offset_x = readOffsetSetting("chatplus_offset_x", "chatpl7s_offset_x");
	style.offset_y = readOffsetSetting("chatplus_offset_y", nullptr);
	style.background = g_settings->exists("chatplus_background") &&
		g_settings->getBool("chatplus_background");
	style.padding = std::max<s32>(0, readOffsetSetting("chatplus_padding", nullptr));
	style.border = g_settings->exists("chatplus_border") &&
		g_settings->getBool("chatplus_border");
	style.background_color = readColorSetting("chatplus_background_color", nullptr,
		video::SColor(180, 40, 40, 40));
	style.border_color = readColorSetting("chatplus_border_color", nullptr,
		video::SColor(255, 120, 120, 255));
	s32 alpha = 180;
	if (g_settings->getS32NoEx("chatplus_background_alpha", alpha)) {
		alpha = std::clamp(alpha, 0, 255);
		style.background_color.setAlpha(alpha);
	}
	alpha = 255;
	if (g_settings->getS32NoEx("chatplus_border_alpha", alpha)) {
		alpha = std::clamp(alpha, 0, 255);
		style.border_color.setAlpha(alpha);
	}

	return style;
}

core::rect<s32> applyHudStyle(const core::rect<s32> &base_rect,
	const HudStyle &style, const v2u32 &window_size)
{
	core::rect<s32> rect = base_rect;
	rect.UpperLeftCorner.X += style.offset_x;
	rect.UpperLeftCorner.Y += style.offset_y;

	rect.LowerRightCorner.X = MYMIN(rect.LowerRightCorner.X, (s32)window_size.X);
	rect.LowerRightCorner.Y = MYMIN(rect.LowerRightCorner.Y, (s32)window_size.Y);

	if (rect.LowerRightCorner.X < rect.UpperLeftCorner.X)
		rect.LowerRightCorner.X = rect.UpperLeftCorner.X;
	if (rect.LowerRightCorner.Y < rect.UpperLeftCorner.Y)
		rect.LowerRightCorner.Y = rect.UpperLeftCorner.Y;

	return rect;
}

void applyAppearance(gui::IGUIStaticText *text, const HudStyle &style)
{
	if (!text)
		return;

	if (!style.enabled) {
		text->setDrawBackground(false);
		text->setDrawBorder(false);
		text->setTextPadding(0);
		return;
	}

	text->setBackgroundColor(style.background_color);
	text->setBorderColor(style.border_color);
	text->setDrawBackground(style.background);
	text->setDrawBorder(style.border);
	text->setTextPadding(style.padding);
}

} // namespace ChatPlus
