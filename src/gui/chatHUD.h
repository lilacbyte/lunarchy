#pragma once

#include "gui/cheatUIElement.h"

class ChatHUD : public CheatUIElement {
public:
	ChatHUD(const core::rect<s32> &rect);
	void draw(video::IVideoDriver *driver, gui::IGUIFont *font, float dtime,
		ClientEnvironment &env, bool editing) override;
};
