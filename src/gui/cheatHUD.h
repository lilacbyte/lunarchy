#pragma once

#include "gui/cheatUIElement.h"
#include "client/client.h"
#include "client/clientenvironment.h"
#include "settings.h"
#include <vector>
#include <string>

class CheatHUD : public CheatUIElement {
public:
	CheatHUD(Client *client, const core::rect<s32> &rect);
	void draw(video::IVideoDriver *driver, gui::IGUIFont *font, float dtime,
		ClientEnvironment &env, bool editing) override;

private:
	Client *m_client;
	std::vector<std::string> m_cached_enabled_cheats;
	float m_cache_timer = 0.0f;
	bool m_cache_valid = false;
	u32 m_cached_cheat_revision = 0;
};
