// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "settings.h"
#include "server.h"
#include "util/string.h"

void migrate_settings()
{
	// Converts opaque_water to translucent_liquids
	if (g_settings->existsLocal("opaque_water")) {
		g_settings->set("translucent_liquids",
				g_settings->getBool("opaque_water") ? "false" : "true");
		g_settings->remove("opaque_water");
	}

	// Converts enable_touch to touch_controls/touch_gui
	if (g_settings->existsLocal("enable_touch")) {
		bool value = g_settings->getBool("enable_touch");
		g_settings->setBool("touch_controls", value);
		g_settings->setBool("touch_gui", value);
		g_settings->remove("enable_touch");
	}

	// Disables anticheat
	if (g_settings->existsLocal("disable_anticheat")) {
		if (g_settings->getBool("disable_anticheat")) {
			g_settings->setFlagStr("anticheat_flags", 0, flagdesc_anticheat);
		}
		g_settings->remove("disable_anticheat");
	}

	// Convert touch_use_crosshair to touch_interaction_style
	if (g_settings->existsLocal("touch_use_crosshair")) {
		bool value = g_settings->getBool("touch_use_crosshair");
		g_settings->set("touch_interaction_style", value ? "tap_crosshair" : "tap");
		g_settings->remove("touch_use_crosshair");
	}

	// Restore the moved HUD color setting under its new name while keeping
	// older configs readable.
	if (!g_settings->existsLocal("globalcolor") &&
			g_settings->existsLocal("global_color")) {
		g_settings->set("globalcolor", g_settings->get("global_color"));
	}

	// Preserve the legacy HUD color toggle/value pair.
	if (g_settings->existsLocal("hudcolor")) {
		const std::string legacy_hud_color = g_settings->get("hudcolor");
		video::SColor parsed_color(255, 255, 255, 255);
		if (parseColorString(legacy_hud_color, parsed_color, true, 0xff)) {
			g_settings->set("globalcolor", legacy_hud_color);
			g_settings->setBool("hud_color", true);
		} else {
			g_settings->setBool("hud_color", g_settings->getBool("hudcolor"));
		}
		g_settings->remove("hudcolor");
	}

	// Merge the old separate HUD/module color controls into the central HUD style.
	if (!g_settings->existsLocal("hud.enabled")) {
		const bool old_hud = g_settings->existsLocal("hud_color") &&
			g_settings->getBool("hud_color");
		const bool old_modules = g_settings->existsLocal("module_color") &&
			g_settings->getBool("module_color");
		if (old_hud || old_modules)
			g_settings->setBool("hud.enabled", true);
	}
	if (!g_settings->existsLocal("hud.accent_color") &&
			g_settings->existsLocal("globalcolor"))
		g_settings->set("hud.accent_color", g_settings->get("globalcolor"));
	if (!g_settings->existsLocal("hud.text_color") &&
			g_settings->existsLocal("module_color.color"))
		g_settings->set("hud.text_color", g_settings->get("module_color.color"));
	if (g_settings->existsLocal("hud.enabled")) {
		g_settings->remove("hud_color");
		g_settings->remove("globalcolor");
		g_settings->remove("global_color");
		g_settings->remove("module_color");
		g_settings->remove("module_color.color");
	}

	// Remove settings for displays which no longer exist in the client.
	g_settings->remove("enable_stats_esp");
	g_settings->remove("statsesp.messages");
	g_settings->remove("statsesp.joins");
	g_settings->remove("statsesp.leaves");
	g_settings->remove("statsesp.joined");
	g_settings->remove("luna_stats.statsesp");
	g_settings->remove("luna_stats.statsesp.pvp");
	g_settings->remove("luna_stats.statsesp.messages");
	g_settings->remove("luna_stats.statsesp.joins");
	g_settings->remove("luna_stats.statsesp.leaves");
	g_settings->remove("luna_stats.statsesp.joined");
	g_settings->remove("luna_stats.client_list.nearby");
	g_settings->remove("nametags.scale");
	g_settings->remove("nametags.stats");
}
