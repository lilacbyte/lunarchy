#pragma once
#include <irrlicht.h>
#include <memory>
#include "client/content_cao.h"
#include "gui/moduleColor.h"
#include "settings.h"
#include <IVideoDriver.h>
#include <IGUIFont.h>
#include "gui/mainmenumanager.h"
#include <algorithm>

using namespace irr;
using namespace gui;

class CheatUIElement {

public:
    CheatUIElement(const core::rect<s32>& rect) : bounds(rect), resizeBounds( 
        rect.LowerRightCorner.X - 5,
        rect.LowerRightCorner.Y - 5,
        rect.LowerRightCorner.X + 5,
        rect.LowerRightCorner.Y + 5
    ) {}

    virtual ~CheatUIElement() = default;

    virtual void draw(video::IVideoDriver* driver, gui::IGUIFont* font, float dtime, ClientEnvironment &env, bool editing) = 0;

    core::rect<s32> bounds;
    core::rect<s32> resizeBounds;
    bool isHovered = false;
    bool isResizeHovered = false;
    std::string elementName;
};

static inline bool hudShouldRender(bool editing)
{
	return editing || !isMenuActive();
}

static inline s32 moduleHudPadding()
{
	return readModulePadding();
}

static inline core::rect<s32> fitModuleHudBounds(CheatUIElement &element,
	s32 content_width, s32 content_height, s32 padding = -1)
{
	if (padding < 0)
		padding = moduleHudPadding();
	const s32 width = std::max<s32>(1, content_width) + padding * 2;
	const s32 height = std::max<s32>(1, content_height) + padding * 2;
	element.bounds.LowerRightCorner = element.bounds.UpperLeftCorner +
		core::position2d<s32>(width, height);
	element.resizeBounds = core::rect<s32>(
		element.bounds.LowerRightCorner.X - 5,
		element.bounds.LowerRightCorner.Y - 5,
		element.bounds.LowerRightCorner.X + 5,
		element.bounds.LowerRightCorner.Y + 5);
	return element.bounds;
}
