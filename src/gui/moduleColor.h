#pragma once

#include "settings.h"
#include "util/string.h"
#include <SColor.h>
#include <algorithm>

inline irr::video::SColor readModuleTextColor()
{
	irr::video::SColor color(255, 255, 255, 255);
	if (!g_settings || !g_settings->getBool("hud.enabled"))
		return color;

	if (parseColorString(g_settings->get("hud.text_color"), color, true, 0xff))
		return color;

	return irr::video::SColor(255, 255, 255, 255);
}

inline irr::video::SColor readHudAccentColor(const irr::video::SColor &fallback)
{
	irr::video::SColor color = fallback;
	if (!g_settings || !g_settings->getBool("hud.enabled"))
		return color;

	if (parseColorString(g_settings->get("hud.accent_color"), color, true, 0xff))
		return color;

	return fallback;
}

inline irr::video::SColor readModuleBackgroundColor()
{
	irr::video::SColor color(180, 25, 25, 25);
	if (!g_settings || !g_settings->getBool("hud.enabled"))
		return color;

	parseColorString(g_settings->get("hud.background_color"), color, true, 0xb4);
	color.setAlpha(static_cast<u8>(std::clamp(g_settings->getS32("hud.background_alpha"), 0, 255)));
	return color;
}

inline irr::video::SColor readModuleBorderColor()
{
	irr::video::SColor color(255, 0, 0, 0);
	if (!g_settings || !g_settings->getBool("hud.enabled"))
		return color;

	parseColorString(g_settings->get("hud.border_color"), color, true, 0xff);
	color.setAlpha(static_cast<u8>(std::clamp(g_settings->getS32("hud.border_alpha"), 0, 255)));
	return color;
}

inline irr::s32 readModulePadding()
{
	if (!g_settings || !g_settings->getBool("hud.enabled"))
		return 5;
	return std::clamp(g_settings->getS32("hud.padding"), 0, 20);
}
