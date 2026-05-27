#pragma once

#include "gui/cheatUIElement.h"
#include "client/clientenvironment.h"
#include <string>
#include <vector>

class StatsHUD : public CheatUIElement {
public:
	explicit StatsHUD(const core::rect<s32> &rect);
	void draw(video::IVideoDriver *driver, gui::IGUIFont *font, float dtime,
		ClientEnvironment &env, bool editing) override;

private:
	float m_cache_timer = 0.0f;
	bool m_cache_valid = false;
	std::vector<std::wstring> m_cached_lines;
};
