#include "gui/inventoryHUD.h"

#include "client/localplayer.h"
#include "gui/drawItemStack.h"
#include "gui/moduleColor.h"
#include "inventory.h"
#include "settings.h"
#include "util/string.h"
#include <algorithm>
#include <vector>

namespace {
static video::SColor readInventoryColor(const char *setting,
	const video::SColor &fallback, const char *alpha_setting = nullptr)
{
	video::SColor color = fallback;
	parseColorString(g_settings->get(setting), color, true, fallback.getAlpha());
	if (alpha_setting) {
		color.setAlpha(static_cast<u8>(std::clamp(
			g_settings->getS32(alpha_setting), 0, 255)));
	}
	return color;
}

static video::SColor readBackgroundColor()
{
	if (g_settings->getBool("hud.enabled"))
		return readModuleBackgroundColor();
	return readInventoryColor("inventory_hud.background_color",
		video::SColor(180, 25, 25, 25), "inventory_hud.background_alpha");
}

static video::SColor readBorderColor()
{
	if (g_settings->getBool("hud.enabled"))
		return readModuleBorderColor();
	return readInventoryColor("inventory_hud.border_color",
		video::SColor(255, 0, 0, 0), "inventory_hud.border_alpha");
}

static s32 readPadding()
{
	if (g_settings->getBool("hud.enabled"))
		return moduleHudPadding();
	return std::clamp(g_settings->getS32("inventory_hud.padding"), 0, 20);
}

static std::vector<ItemStack> collectMainInventory(LocalPlayer *player, bool editing)
{
	std::vector<ItemStack> items;
	InventoryList *main = player ? player->inventory.getList("main") : nullptr;
	if (!main) {
		if (editing)
			items.resize(36);
		return items;
	}

	items.reserve(main->getSize());

	// Mineclonia stores the hotbar in slots 0-8. Match its inventory screen by
	// showing the backpack first and the hotbar as the final row.
	const u32 hotbar_size = std::min<u32>(9, main->getSize());
	for (u32 i = hotbar_size; i < main->getSize(); ++i)
		items.push_back(main->getItem(i));
	for (u32 i = 0; i < hotbar_size; ++i)
		items.push_back(main->getItem(i));

	return items;
}
} // namespace

InventoryHUD::InventoryHUD(Client *client, const core::rect<s32> &rect) :
	CheatUIElement(rect), m_client(client)
{
}

void InventoryHUD::draw(video::IVideoDriver *driver, gui::IGUIFont *font, float dtime,
	ClientEnvironment &env, bool editing)
{
	(void)dtime;
	if (!hudShouldRender(editing))
		return;
	if (!g_settings->getBool("inventory_hud") && !editing)
		return;
	if (!driver || !font || !m_client)
		return;

	std::vector<ItemStack> items = collectMainInventory(env.getLocalPlayer(), editing);
	if (items.empty() && !editing)
		return;

	const s32 columns = std::clamp(g_settings->getS32("inventory_hud.columns"), 1, 12);
	const s32 configured_slot_size =
		std::clamp(g_settings->getS32("inventory_hud.slot_size"), 16, 64);
	const s32 slot_gap = std::clamp(g_settings->getS32("inventory_hud.slot_gap"), 0, 8);
	const s32 padding = readPadding();
	const s32 rows = std::max<s32>(1,
		(static_cast<s32>(items.size()) + columns - 1) / columns);
	const s32 available_width = std::max<s32>(1,
		bounds.getWidth() - padding * 2 - std::max(0, columns - 1) * slot_gap);
	const s32 available_height = std::max<s32>(1,
		bounds.getHeight() - padding * 2 - std::max(0, rows - 1) * slot_gap);
	const s32 fitted_slot_size = std::max<s32>(8,
		std::min(available_width / columns, available_height / rows));
	const s32 slot_size = std::min(configured_slot_size, fitted_slot_size);
	const s32 content_width = columns * slot_size + std::max(0, columns - 1) * slot_gap;
	const s32 content_height = rows * slot_size + std::max(0, rows - 1) * slot_gap;
	const core::rect<s32> draw_bounds = bounds;
	resizeBounds = core::rect<s32>(
		bounds.LowerRightCorner.X - 5, bounds.LowerRightCorner.Y - 5,
		bounds.LowerRightCorner.X + 5, bounds.LowerRightCorner.Y + 5);
	const s32 content_x = draw_bounds.UpperLeftCorner.X +
		std::max<s32>(padding, (draw_bounds.getWidth() - content_width) / 2);
	const s32 content_y = draw_bounds.UpperLeftCorner.Y +
		std::max<s32>(padding, (draw_bounds.getHeight() - content_height) / 2);

	if (editing || g_settings->getBool("inventory_hud.background"))
		driver->draw2DRectangle(readBackgroundColor(), draw_bounds);
	if (editing || g_settings->getBool("inventory_hud.border"))
		driver->draw2DRectangleOutline(draw_bounds, readBorderColor(), 2);

	const video::SColor slot_color = readInventoryColor(
		"inventory_hud.slot_color", video::SColor(100, 45, 45, 45),
		"inventory_hud.slot_alpha");
	const video::SColor slot_border = readInventoryColor(
		"inventory_hud.slot_border_color", video::SColor(150, 120, 120, 120));
	const video::SColor text_color = g_settings->getBool("hud.enabled") ?
		readModuleTextColor() : readInventoryColor(
			"inventory_hud.text_color", video::SColor(255, 255, 255, 255));

	for (size_t i = 0; i < items.size(); ++i) {
		const s32 column = static_cast<s32>(i) % columns;
		const s32 row = static_cast<s32>(i) / columns;
		const s32 x = content_x + column * (slot_size + slot_gap);
		const s32 y = content_y + row * (slot_size + slot_gap);
		const core::rect<s32> slot_rect(x, y, x + slot_size, y + slot_size);

		if (g_settings->getBool("inventory_hud.slot_background") || editing) {
			driver->draw2DRectangle(slot_color, slot_rect);
			driver->draw2DRectangleOutline(slot_rect, slot_border, 1);
		}
		if (!items[i].empty()) {
			drawItemStackWithTextColor(driver, font, items[i], slot_rect, nullptr,
				m_client, IT_ROT_NONE, text_color, video::SColor(255, 0, 255, 0));
		}
	}
}
