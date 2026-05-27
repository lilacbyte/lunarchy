#pragma once

#include "gui/cheatUIElement.h"
#include "settings.h"
#include <string>

class FpsHUD : public CheatUIElement {
public:
	FpsHUD(const core::rect<s32> &rect);
	void draw(video::IVideoDriver *driver, gui::IGUIFont *font, float dtime,
		ClientEnvironment &env, bool editing) override;

private:
	float m_sample_timer = 0.0f;
	unsigned int m_sample_frames = 0;
	std::wstring m_cached_text = L"fps: --";
};
