#pragma once

#include "gui/cheatUIElement.h"
#include "client/client.h"
#include "client/clientenvironment.h"
#include "settings.h"
#include <string>

class pingHUD : public CheatUIElement {
public:
	pingHUD(Client *client, const core::rect<s32> &rect);
	void draw(video::IVideoDriver *driver, gui::IGUIFont *font, float dtime,
		ClientEnvironment &env, bool editing) override;

private:
	Client *m_client;
	float m_cache_timer = 0.0f;
	std::wstring m_cached_text = L"ping: -- ms";
};
