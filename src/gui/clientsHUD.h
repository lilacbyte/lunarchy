#pragma once

#include "gui/cheatUIElement.h"
#include "client/clientenvironment.h"
#include "settings.h"
#include <vector>
#include <string>

class clientsHUD : public CheatUIElement {
public:
	enum class Section {
		Online,
		Nearby,
	};

	clientsHUD(const core::rect<s32> &rect, Section section);
	void draw(video::IVideoDriver *driver, gui::IGUIFont *font, float dtime,
		ClientEnvironment &env, bool editing) override;

private:
	core::rect<s32> calculateBounds(video::IVideoDriver *driver, gui::IGUIFont *font,
		const std::vector<std::wstring> &lines) const;
	std::vector<std::wstring> buildLines(ClientEnvironment &env) const;
	Section m_section;
	float m_cache_timer = 0.0f;
	bool m_cache_valid = false;
	std::vector<std::wstring> m_cached_lines;
};
