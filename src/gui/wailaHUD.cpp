#include "gui/wailaHUD.h"

#include "client/game.h"
#include "gui/drawItemStack.h"
#include "gui/moduleColor.h"
#include "inventory.h"
#include "map.h"
#include "nodedef.h"
#include "util/pointedthing.h"
#include "util/string.h"

#include <algorithm>
#include <optional>

namespace {
struct WailaInfo {
	std::string name;
	std::string item_name;
	bool has_icon = false;
};

static std::optional<WailaInfo> getWailaInfo(Client *client, ClientEnvironment &env)
{
	if (!client || !g_game)
		return std::nullopt;

	const PointedThing &pointed = g_game->getPointedOld();
	if (pointed.type == POINTEDTHING_NODE) {
		bool valid = false;
		const MapNode node = env.getMap().getNode(pointed.node_undersurface, &valid);
		if (!valid || node.getContent() == CONTENT_AIR || node.getContent() == CONTENT_IGNORE)
			return std::nullopt;

		const ContentFeatures &features = client->getNodeDefManager()->get(node);
		if (features.name == "air" || features.name == "ignore")
			return std::nullopt;

		WailaInfo info;
		info.name = features.name;
		info.item_name = features.name;
		info.has_icon = true;
		return info;
	}

	if (pointed.type == POINTEDTHING_OBJECT) {
		ClientActiveObject *obj = env.getActiveObject(pointed.object_id);
		if (!obj)
			return std::nullopt;

		WailaInfo info;
		info.name = obj->infoText();
		if (info.name.empty())
			info.name = "entity";
		return info;
	}

	return std::nullopt;
}
}

WailaHUD::WailaHUD(Client *client, const core::rect<s32> &rect) :
	CheatUIElement(rect), m_client(client)
{
}

void WailaHUD::draw(video::IVideoDriver *driver, gui::IGUIFont *font, float dtime,
	ClientEnvironment &env, bool editing)
{
	(void)dtime;
	if (!hudShouldRender(editing))
		return;

	if (!g_settings->getBool("waila") && !editing)
		return;
	if (!font)
		return;

	const std::optional<WailaInfo> info = editing
		? std::optional<WailaInfo>(WailaInfo{"WAILA", "default:stone", true})
		: getWailaInfo(m_client, env);
	if (!info && !editing)
		return;

	const std::wstring text = utf8_to_wide(info ? info->name : "WAILA");
	const core::dimension2d<u32> dim_u32 = font->getDimension(text.c_str());
	const core::dimension2d<s32> dim(dim_u32.Width, dim_u32.Height);

	const s32 icon_size = info && info->has_icon ?
		std::max<s32>(20, std::min<s32>(static_cast<s32>(dim.Height) + 10, 32)) : 0;
	const s32 gap = icon_size > 0 ? 5 : 0;
	const s32 content_width = icon_size + gap + dim.Width;
	const s32 content_height = std::max<s32>(icon_size, dim.Height);
	const core::rect<s32> draw_bounds = fitModuleHudBounds(*this, content_width, content_height);

	const bool draw_background = editing || g_settings->getBool("waila.background");
	if (draw_background) {
		driver->draw2DRectangle(readModuleBackgroundColor(), draw_bounds);
		driver->draw2DRectangleOutline(draw_bounds, readModuleBorderColor(), 2);
	}

	const s32 padding = moduleHudPadding();
	const s32 icon_x = draw_bounds.UpperLeftCorner.X + padding;
	const s32 icon_y = draw_bounds.UpperLeftCorner.Y + (draw_bounds.getHeight() - icon_size) / 2;
	if (info && info->has_icon) {
		ItemStack stack(info->item_name, 1, 0, m_client ? m_client->getItemDefManager() : nullptr);
		drawItemStack(driver, font, stack,
			core::rect<s32>(icon_x, icon_y, icon_x + icon_size, icon_y + icon_size),
			nullptr, m_client, IT_ROT_NONE);
	}

	const s32 text_x = draw_bounds.UpperLeftCorner.X + padding + icon_size + gap;
	const s32 text_y = draw_bounds.UpperLeftCorner.Y + (draw_bounds.getHeight() - dim.Height) / 2;
	font->draw(text.c_str(), core::rect<s32>(text_x, text_y, text_x + dim.Width, text_y + dim.Height), readModuleTextColor());
}
