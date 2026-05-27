#pragma once

#include "gui/cheatUIElement.h"
#include "client/client.h"
#include "client/clientenvironment.h"
#include "settings.h"

class WailaHUD : public CheatUIElement {
public:
	WailaHUD(Client *client, const core::rect<s32> &rect);
	void draw(video::IVideoDriver *driver, gui::IGUIFont *font, float dtime,
		ClientEnvironment &env, bool editing) override;

private:
	Client *m_client;
};
