#include "gui/totemsHUD.h"

#include "client/localplayer.h"
#include "gui/moduleColor.h"
#include "inventory.h"
#include "util/string.h"

#include <algorithm>

namespace {
static int countTotems(LocalPlayer *player)
{
	if (!player)
		return 0;

	int count = 0;
	const char *lists[] = {"main", "offhand"};
	for (const char *list_name : lists) {
		InventoryList *list = player->inventory.getList(list_name);
		if (!list)
			continue;
		for (u32 i = 0; i < list->getSize(); ++i) {
			const ItemStack &stack = list->getItem(i);
			if (stack.name == "mcl_totems:totem")
				count += stack.count;
		}
	}
	return count;
}
}

TotemsHUD::TotemsHUD(Client *client, const core::rect<s32> &rect) :
	CheatUIElement(rect), m_client(client)
{
}

void TotemsHUD::draw(video::IVideoDriver *driver, gui::IGUIFont *font, float dtime,
	ClientEnvironment &env, bool editing)
{
	(void)m_client;
	if (!hudShouldRender(editing))
		return;

	if (!g_settings->getBool("totems") && !editing)
		return;
	if (!font)
		return;

	m_cache_timer += dtime;
	if (editing || !m_cache_valid || m_cache_timer >= 0.25f) {
		const int count = countTotems(env.getLocalPlayer());
		m_cached_text = L"totems: " + utf8_to_wide(itos(count));
		m_cache_valid = true;
		m_cache_timer = 0.0f;
	}
	const std::wstring &text = m_cached_text;
	const core::dimension2d<u32> dim_u32 = font->getDimension(text.c_str());
	const core::dimension2d<s32> dim(dim_u32.Width, dim_u32.Height);

	const core::rect<s32> draw_bounds = fitModuleHudBounds(*this, dim.Width, dim.Height);

	const bool draw_background = editing || g_settings->getBool("totems.background");
	if (draw_background) {
		driver->draw2DRectangle(readModuleBackgroundColor(), draw_bounds);
		driver->draw2DRectangleOutline(draw_bounds, readModuleBorderColor(), 2);
	}

	const s32 padding = moduleHudPadding();
	const s32 x = draw_bounds.UpperLeftCorner.X + padding;
	const s32 y = draw_bounds.UpperLeftCorner.Y + padding;
	font->draw(text.c_str(), core::rect<s32>(x, y, x + dim.Width, y + dim.Height), readModuleTextColor());
}
