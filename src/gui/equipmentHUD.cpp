#include "gui/equipmentHUD.h"

#include "gui/drawItemStack.h"
#include "gui/moduleColor.h"
#include "inventory.h"
#include "itemdef.h"
#include "itemgroup.h"
#include "client/localplayer.h"
#include "tool.h"
#include "util/string.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
static std::wstring toWString(const std::string &str)
{
	return utf8_to_wide(str);
}

static std::string getDisplayName(const ItemStack &item, Client *client)
{
	if (!client)
		return item.name;

	if (item.name.find("mcl_meshhand") != std::string::npos ||
			item.name.find("filled_map_hand") != std::string::npos) {
		return "Hand";
	}

	std::string desc = item.getShortDescription(client->idef());
	if (!desc.empty())
		return desc;

	desc = item.getDescription(client->idef());
	if (!desc.empty())
		return desc;

	const ItemDefinition &def = item.getDefinition(client->idef());
	if (!def.short_description.empty())
		return def.short_description;
	if (!def.description.empty())
		return def.description;

	return item.name;
}

static std::string getDurabilityMode()
{
	return g_settings->get("equipment_hud.durability_mode");
}
} // namespace

EquipmentHUD::EquipmentHUD(Client *client, const core::rect<s32> &rect) :
	CheatUIElement(rect), m_client(client)
{
}

int EquipmentHUD::getDurabilityUses(const ItemStack &item, Client *client)
{
	if (!client || item.empty())
		return 0;

	const ItemDefinition &def = item.getDefinition(client->idef());
	const int armor_uses = itemgroup_get(def.groups, "mcl_armor_uses");
	if (armor_uses > 0)
		return armor_uses;

	if (def.type == ITEM_TOOL) {
		const ToolCapabilities &caps = item.getToolCapabilities(client->idef());
		for (const auto &groupcap : caps.groupcaps) {
			if (groupcap.second.uses > 0)
				return groupcap.second.uses;
		}
		if (caps.punch_attack_uses > 0)
			return caps.punch_attack_uses;
	}

	return 0;
}

int EquipmentHUD::getRemainingDurability(const ItemStack &item, int max_uses)
{
	if (max_uses <= 0)
		return 0;

	const float remaining = (65535.0f - static_cast<float>(item.wear)) *
		static_cast<float>(max_uses) / 65535.0f;
	return std::max(0, static_cast<int>(std::floor(remaining + 0.5f)));
}

video::SColor EquipmentHUD::getDurabilityBarColor(const ItemStack &item, Client *client)
{
	const int max_uses = getDurabilityUses(item, client);
	if (max_uses <= 0)
		return video::SColor(255, 0, 255, 0);

	const int remaining = getRemainingDurability(item, max_uses);
	const float ratio = static_cast<float>(remaining) / static_cast<float>(max_uses);
	if (ratio > 0.5f)
		return video::SColor(255, 0, 255, 0);
	if (ratio > 0.25f)
		return video::SColor(255, 255, 165, 0);
	return video::SColor(255, 255, 0, 0);
}

std::wstring EquipmentHUD::getItemNameLabel(const ItemStack &item, Client *client)
{
	const std::string display_name = getDisplayName(item, client);
	return toWString(display_name.empty() ? item.name : display_name);
}

std::wstring EquipmentHUD::getDurabilityLabel(const ItemStack &item, Client *client)
{
	const int max_uses = getDurabilityUses(item, client);
	if (max_uses > 0) {
		const int remaining = getRemainingDurability(item, max_uses);
		const float percent = max_uses > 0 ? (static_cast<float>(remaining) / max_uses) * 100.f : 0.f;
		const std::string mode = getDurabilityMode();
		if (mode == "Percent") {
			return L" (" + toWString(itos(static_cast<int>(std::floor(percent + 0.5f)))) + L"%)";
		} else if (mode == "Dur/Max") {
			return L" (" + toWString(itos(remaining)) + L"/" + toWString(itos(max_uses)) + L")";
		} else {
			return L" (" + toWString(itos(remaining)) + L"/" + toWString(itos(max_uses)) +
				L", " + toWString(itos(static_cast<int>(std::floor(percent + 0.5f)))) + L"%)";
		}
	}

	return L"";
}

void EquipmentHUD::drawEntry(video::IVideoDriver *driver, gui::IGUIFont *font,
	Client *client, const ItemStack &item, const std::wstring &fallback_label,
	const core::rect<s32> &entry_rect, s32 icon_size, bool editing)
{
	if (!font)
		return;

	const video::SColor text_color(255, 255, 255, 255);
	const video::SColor stack_count_color(255, 255, 255, 255);
	const video::SColor durability_bar_color = getDurabilityBarColor(item, client);

	if (!editing && item.empty())
		return;

	const s32 icon_x = entry_rect.UpperLeftCorner.X;
	const s32 icon_y = entry_rect.UpperLeftCorner.Y + (entry_rect.getHeight() - icon_size) / 2;
	const core::rect<s32> icon_rect(icon_x, icon_y, icon_x + icon_size, icon_y + icon_size);

	if (!item.empty())
		drawItemStackWithTextColor(driver, font, item, icon_rect, nullptr, client,
			IT_ROT_NONE, stack_count_color, durability_bar_color);

	const std::wstring label = item.empty() ? fallback_label : getItemNameLabel(item, client);
	const std::wstring durability_label = item.empty() ? L"" : getDurabilityLabel(item, client);
	const std::wstring full_label = label + durability_label;
	if (full_label.empty())
		return;

	const std::wstring visible_label = unescape_enriched(full_label);
	core::dimension2d<u32> text_size_u32 = font->getDimension(visible_label.c_str());
	core::dimension2d<s32> text_size(text_size_u32.Width, text_size_u32.Height);

	const s32 text_x = icon_rect.LowerRightCorner.X + 5;
	const s32 text_y = entry_rect.UpperLeftCorner.Y + (entry_rect.getHeight() - text_size.Height) / 2;
	font->draw(full_label.c_str(),
		core::rect<s32>(text_x, text_y, text_x + text_size.Width, text_y + text_size.Height),
		text_color, false, false);
}

void EquipmentHUD::draw(video::IVideoDriver *driver, gui::IGUIFont *font, float dtime,
	ClientEnvironment &env, bool editing)
{
	(void)dtime;
	if (!hudShouldRender(editing))
		return;

	const bool enabled = g_settings->getBool("equipment_hud");
	if (!enabled && !editing)
		return;
	if (!font)
		return;

	LocalPlayer *player = env.getLocalPlayer();
	if (!player)
		return;

	InventoryList *armor = player->inventory.getList("armor");
	ItemStack wielded;
	ItemStack hand;
	ItemStack &effective = player->getWieldedItem(&wielded, &hand);

	std::vector<std::pair<ItemStack, std::wstring>> entries;
	entries.emplace_back(effective, L"Held item");

	if (armor) {
		const std::pair<u32, std::wstring> armor_slots[] = {
			// Mineclonia keeps the quick-equip slot at index 0; worn armor starts at 1.
			{1, L"Helmet"},
			{2, L"Chestplate"},
			{3, L"Leggings"},
			{4, L"Boots"},
		};

		for (const auto &slot : armor_slots) {
			if (slot.first < armor->getSize())
				entries.emplace_back(armor->getItem(slot.first), slot.second);
		}
	}

	if (!editing) {
		entries.erase(std::remove_if(entries.begin(), entries.end(),
			[](const auto &entry) { return entry.first.empty(); }), entries.end());
		if (entries.empty())
			return;
	}

	const s32 icon_size = std::max<s32>(16, std::min<s32>(
		static_cast<s32>(font->getDimension(L"M").Height) + 10, 28));
	const s32 row_gap = 3;
	s32 max_row_width = 0;
	s32 row_height = icon_size;
	for (const auto &entry : entries) {
		const std::wstring label = entry.first.empty() ? entry.second :
			getItemNameLabel(entry.first, m_client) + getDurabilityLabel(entry.first, m_client);
		const auto text_size = font->getDimension(unescape_enriched(label).c_str());
		max_row_width = std::max<s32>(max_row_width,
			icon_size + 5 + static_cast<s32>(text_size.Width));
		row_height = std::max<s32>(row_height, static_cast<s32>(text_size.Height));
	}
	const s32 content_height = static_cast<s32>(entries.size()) * row_height +
		std::max<s32>(0, static_cast<s32>(entries.size()) - 1) * row_gap;
	const core::rect<s32> draw_bounds = fitModuleHudBounds(*this, max_row_width, content_height);

	const video::SColor outline_color = readModuleBorderColor();
	const video::SColor background_color = readModuleBackgroundColor();
	const bool draw_background = editing || g_settings->getBool("equipment_hud.background");
	if (draw_background) {
		driver->draw2DRectangle(background_color, draw_bounds);
		driver->draw2DRectangleOutline(draw_bounds, outline_color, 2);
	}

	const s32 padding = moduleHudPadding();
	const s32 start_y = draw_bounds.UpperLeftCorner.Y + padding;
	const s32 row_width = max_row_width;
	for (size_t i = 0; i < entries.size(); ++i) {
		const ItemStack &item = entries[i].first;
		const std::wstring &fallback_label = entries[i].second;
		const s32 top = start_y + static_cast<s32>(i) * (row_height + row_gap);
		const core::rect<s32> row(
			draw_bounds.UpperLeftCorner.X + padding,
			top,
			draw_bounds.UpperLeftCorner.X + padding + row_width,
			top + row_height
		);
		drawEntry(driver, font, m_client, item, fallback_label, row, icon_size, editing);
	}
}
