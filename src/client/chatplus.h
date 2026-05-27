// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes.h"
#include "irr_v2d.h"
#include <IGUIStaticText.h>

namespace ChatPlus {

struct HudStyle {
	bool enabled = true;
	s32 offset_x = 0;
	s32 offset_y = 0;
	bool background = false;
	video::SColor background_color = video::SColor(0, 0, 0, 0);
	bool border = false;
	video::SColor border_color = video::SColor(255, 0, 0, 0);
	s32 padding = 0;
};

HudStyle readHudStyle();

core::rect<s32> applyHudStyle(const core::rect<s32> &base_rect,
	const HudStyle &style, const v2u32 &window_size);

void applyAppearance(gui::IGUIStaticText *text, const HudStyle &style);

} // namespace ChatPlus
