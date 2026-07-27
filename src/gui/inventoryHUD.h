#pragma once

#include "client/client.h"
#include "gui/cheatUIElement.h"

class InventoryHUD : public CheatUIElement {
public:
	InventoryHUD(Client *client, const core::rect<s32> &rect);
	void draw(video::IVideoDriver *driver, gui::IGUIFont *font, float dtime,
		ClientEnvironment &env, bool editing) override;

private:
	Client *m_client;
};
