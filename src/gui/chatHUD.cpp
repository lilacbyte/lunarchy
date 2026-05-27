#include "gui/chatHUD.h"
#include "gui/moduleColor.h"

#include <algorithm>

ChatHUD::ChatHUD(const core::rect<s32> &rect) : CheatUIElement(rect) {}

void ChatHUD::draw(video::IVideoDriver *driver, gui::IGUIFont *font, float dtime,
	ClientEnvironment &env, bool editing)
{
	(void)font;
	(void)dtime;
	(void)env;

	if (!hudShouldRender(editing))
		return;

	const video::SColor outline_color = readModuleBorderColor();
	const video::SColor background_color = readModuleBackgroundColor();

	if (editing) {
		driver->draw2DRectangle(background_color, bounds);
		driver->draw2DRectangleOutline(bounds, outline_color, 3);
	}
}
