#pragma once

#include "gui/cheatUIElement.h"
#include <iostream>
#include <chrono>
#include <locale> 
#include <codecvt> 
#include "settings.h"
#include "client/client.h"
#include "client/localplayer.h"
#include <sstream>
#include <iomanip> 
#include <vector>

class coordsHUD : public CheatUIElement {
public:
	coordsHUD(const core::rect<s32>& rect);
    void draw(video::IVideoDriver* driver, gui::IGUIFont* font, float dtime, ClientEnvironment &env, bool editing) override;

private:
	float m_cache_timer = 0.0f;
	bool m_cache_valid = false;
	std::vector<std::wstring> m_cached_lines;
};
