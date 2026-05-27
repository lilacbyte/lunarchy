// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "camera.h"
#include "debug.h"
#include "client.h"
#include "config.h"
#include "map.h"
#include "clientmap.h"     // MapDrawControl
#include "player.h"
#include <cmath>
#include "client/renderingengine.h"
#include "client/content_cao.h"
#include "client/playerstats.h"
#include "client/item_visuals_manager.h"
#include "gui/moduleColor.h"
#include "settings.h"
#include "wieldmesh.h"
#include "noise.h"         // easeCurve
#include "mtevent.h"
#include "nodedef.h"
#include "util/numeric.h"
#include "util/string.h"
#include "constants.h"
#include "fontengine.h"
#include "script/scripting_client.h"
#include "gettext.h"
#include "guiscalingfilter.h"
#include <SViewFrustum.h>
#include <IGUIFont.h>
#include <IVideoDriver.h>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>

static constexpr f32 CAMERA_OFFSET_STEP = 200;


// TODO: fix the left arm
float WIELDMESH_OFFSET_X = 55.0f;
#define WIELDMESH_OFFSET_Y -35.0f
#define WIELDMESH_AMPLITUDE_X 7.0f
#define WIELDMESH_AMPLITUDE_Y 10.0f

static const char *setting_names[] = {
	"view_bobbing_amount", "fov", "arm_inertia",
	"show_nametag_backgrounds",
	"hand_view",
	"hand_view.x",
	"hand_view.y",
	"hand_view.z",
	"hand_view.scale",
	"hand_view.color",
	"nametags.distance",
};

namespace {

struct NametagItemToken {
	std::string label;
	ItemStack item;
};

static bool lagOptimizerNoHandAnimation()
{
	return g_settings->getBool("lag_optimizer") &&
		g_settings->getBool("lag_optimizer.no_hand_animation");
}

static std::string get_item_display_name(const ItemStack &item, Client *client)
{
	if (item.empty())
		return std::string();

	std::string desc;
	if (client && client->idef())
		desc = item.getShortDescription(client->idef());

	if (desc.empty())
		desc = item.name;

	std::string clean;
	bool pending_space = false;
	for (unsigned char ch : desc) {
		if (std::isspace(ch)) {
			pending_space = !clean.empty();
			continue;
		}
		if (pending_space)
			clean.push_back(' ');
		clean.push_back(static_cast<char>(ch));
		pending_space = false;
	}
	return clean;
}

static bool texture_mentions_item(const std::string &texture, const std::string &itemstring)
{
	std::string needle = itemstring;
	std::replace(needle.begin(), needle.end(), ':', '_');
	return texture.find(needle) != std::string::npos;
}

static void collect_visible_armor_tokens(std::vector<NametagItemToken> &tokens,
		Client *client, GenericCAO *obj, bool allow_self)
{
	if (!client || !obj || !obj->isPlayer() || (obj->isLocalPlayer() && !allow_self))
		return;

	const ObjectProperties &props = obj->getProperties();
	if (props.textures.size() < 2)
		return;

	const std::string &armor_texture = props.textures[1];
	if (armor_texture.empty() || armor_texture == "blank.png")
		return;

	static const char *const head_items[] = {
		"mcl_armor:helmet_leather",
		"mcl_armor:helmet_gold",
		"mcl_armor:helmet_chain",
		"mcl_armor:helmet_copper",
		"mcl_armor:helmet_iron",
		"mcl_armor:helmet_diamond",
		"mcl_armor:helmet_netherite",
	};
	static const char *const torso_items[] = {
		"mcl_armor:chestplate_leather",
		"mcl_armor:chestplate_gold",
		"mcl_armor:chestplate_chain",
		"mcl_armor:chestplate_copper",
		"mcl_armor:chestplate_iron",
		"mcl_armor:chestplate_diamond",
		"mcl_armor:chestplate_netherite",
		"mcl_armor:elytra",
	};
	static const char *const leg_items[] = {
		"mcl_armor:leggings_leather",
		"mcl_armor:leggings_gold",
		"mcl_armor:leggings_chain",
		"mcl_armor:leggings_copper",
		"mcl_armor:leggings_iron",
		"mcl_armor:leggings_diamond",
		"mcl_armor:leggings_netherite",
	};
	static const char *const foot_items[] = {
		"mcl_armor:boots_leather",
		"mcl_armor:boots_gold",
		"mcl_armor:boots_chain",
		"mcl_armor:boots_copper",
		"mcl_armor:boots_iron",
		"mcl_armor:boots_diamond",
		"mcl_armor:boots_netherite",
	};

	auto push_if_visible = [&](const char *itemstring) {
		ItemStack item;
		item.deSerialize(itemstring, client->idef());
		if (item.empty())
			return;
		if (texture_mentions_item(armor_texture, itemstring)) {
			std::string label = get_item_display_name(item, client);
			if (!label.empty())
				tokens.push_back({label, item});
		}
	};

	for (const char *itemstring : head_items)
		push_if_visible(itemstring);
	for (const char *itemstring : torso_items)
		push_if_visible(itemstring);
	for (const char *itemstring : leg_items)
		push_if_visible(itemstring);
	for (const char *itemstring : foot_items)
		push_if_visible(itemstring);
}

static void collect_visible_wield_tokens(std::vector<NametagItemToken> &tokens,
		Client *client, GenericCAO *obj, bool allow_self)
{
	if (!client || !obj || !obj->isPlayer() || (obj->isLocalPlayer() && !allow_self))
		return;

	ClientEnvironment &env = client->getEnv();
	for (ActiveObject::object_t child_id : obj->getAttachmentChildIds()) {
		ClientActiveObject *child = env.getActiveObject(child_id);
		auto *gcao = dynamic_cast<GenericCAO *>(child);
		if (!gcao || gcao->isLocalPlayer())
			continue;

		ActiveObject::object_t parent_id = 0;
		std::string bone;
		v3f position;
		v3f rotation;
		bool force_visible = false;
		gcao->getAttachment(&parent_id, &bone, &position, &rotation, &force_visible);
		if (parent_id != obj->getId())
			continue;

		const ObjectProperties &props = gcao->getProperties();
		if (props.wield_item.empty())
			continue;

		ItemStack item;
		item.deSerialize(props.wield_item, client->idef());
		std::string label = get_item_display_name(item, client);
		if (!label.empty())
			tokens.push_back({label, item});
	}
}

static s32 measure_icon_row_width(const std::vector<NametagItemToken> &tokens, s32 icon_size, s32 gap)
{
	if (tokens.empty())
		return 0;
	return static_cast<s32>(tokens.size()) * icon_size
		+ static_cast<s32>(tokens.size() - 1) * gap;
}

static s32 measure_item_name_width(gui::IGUIFont *font,
		const std::vector<NametagItemToken> &tokens)
{
	s32 width = 0;
	for (const auto &token : tokens)
		width = std::max<s32>(width,
			static_cast<s32>(font->getDimension(utf8_to_wide(token.label).c_str()).Width));
	return width;
}

static void draw_icon_row(video::IVideoDriver *driver, gui::IGUIFont *font, Client *client,
		const std::vector<NametagItemToken> &tokens, const v2s32 &center, s32 icon_size,
		s32 gap)
{
	if (!driver || !font || tokens.empty())
		return;

	s32 row_width = measure_icon_row_width(tokens, icon_size, gap);
	s32 x = center.X - row_width / 2;

	for (size_t i = 0; i < tokens.size(); ++i) {
		if (i > 0)
			x += gap;

		core::rect<s32> icon_rect(x, center.Y, x + icon_size, center.Y + icon_size);
		video::ITexture *texture = client->getItemVisualsManager()->getInventoryTexture(tokens[i].item, client);
		if (!texture)
			texture = client->getTextureSource()->getTexture("no_texture.png");
		if (texture) {
			const video::SColor color = client->getItemVisualsManager()->getItemstackColor(tokens[i].item, client);
			const video::SColor colors[] = { color, color, color, color };
			draw2DImageFilterScaled(driver, texture, icon_rect,
				core::rect<s32>({0, 0}, core::dimension2di(texture->getOriginalSize())),
				nullptr, colors, true);
		}
		x += icon_size;
	}
}

static void draw_item_name_rows(gui::IGUIFont *font, const std::vector<NametagItemToken> &tokens,
		const v2s32 &center, s32 start_y, s32 line_height)
{
	if (!font || tokens.empty())
		return;

	for (size_t i = 0; i < tokens.size(); ++i) {
		const auto &token = tokens[i];
		const core::dimension2d<u32> dim = font->getDimension(utf8_to_wide(token.label).c_str());
		const s32 text_x = center.X - static_cast<s32>(dim.Width) / 2;
		core::rect<s32> rect(text_x, start_y + static_cast<s32>(i) * line_height,
				text_x + static_cast<s32>(dim.Width), start_y + static_cast<s32>(i + 1) * line_height);
		font->draw(utf8_to_wide(token.label).c_str(), rect,
			video::SColor(255, 255, 255, 255), true, false);
	}
}

}

Camera::Camera(MapDrawControl &draw_control, Client *client, RenderingEngine *rendering_engine):
	m_draw_control(draw_control),
	m_client(client),
	m_player_light_color(0xFFFFFFFF)
{
	auto smgr = rendering_engine->get_scene_manager();
	// note: making the camera node a child of the player node
	// would lead to unexpected behavior, so we don't do that.
	m_playernode = smgr->addEmptySceneNode(smgr->getRootSceneNode());
	m_headnode = smgr->addEmptySceneNode(m_playernode);
	m_cameranode = smgr->addCameraSceneNode(smgr->getRootSceneNode());
	m_cameranode->bindTargetAndRotation(true);

	// This needs to be in its own scene manager. It is drawn after
	// all other 3D scene nodes and before the GUI.
	m_wieldmgr = smgr->createNewSceneManager();
	m_wieldmgr->addCameraSceneNode();
	m_wieldnode = new WieldMeshSceneNode(m_wieldmgr, -1);
	m_wieldnode->setItem(ItemStack(), m_client);
	m_wieldnode->drop(); // m_wieldmgr grabbed it

	m_nametags.clear();

	readSettings();
	for (auto name : setting_names)
		g_settings->registerChangedCallback(name, settingChangedCallback, this);
}

void Camera::settingChangedCallback(const std::string &name, void *data)
{
	Camera *camera = static_cast<Camera *>(data);
	camera->readSettings();
	if (name.rfind("hand_view", 0) == 0 && camera->m_wieldnode) {
		camera->m_wieldnode->setItem(camera->m_wield_item_next, camera->m_client);
	}
}

void Camera::readSettings()
{
	/* TODO: Local caching of settings is not optimal and should at some stage
	 *       be updated to use a global settings object for getting thse values
	 *       (as opposed to the this local caching). This can be addressed in
	 *       a later release.
	 */
	m_cache_view_bobbing_amount = g_settings->getFloat("view_bobbing_amount", 0.0f, 7.9f);
	// 45 degrees is the lowest FOV that doesn't cause the server to treat this
	// as a zoom FOV and load world beyond the set server limits.
	m_cache_fov                 = g_settings->getFloat("fov", 45.0f, 160.0f);
	m_arm_inertia               = g_settings->getBool("arm_inertia");
	m_show_nametag_backgrounds  = g_settings->getBool("show_nametag_backgrounds");
}

Camera::~Camera()
{
	g_settings->deregisterAllChangedCallbacks(this);
	m_wieldmgr->drop();
}

void Camera::notifyFovChange()
{
	LocalPlayer *player = m_client->getEnv().getLocalPlayer();
	assert(player);

	PlayerFovSpec spec = player->getFov();

	// Remember old FOV in case a transition is wanted
	f32 m_old_fov_degrees = m_fov_transition_active
		? m_curr_fov_degrees // FOV is overridden with transition
		: m_server_sent_fov
			? m_target_fov_degrees // FOV is overridden without transition
			: m_cache_fov; // FOV is not overridden

	m_server_sent_fov = spec.fov > 0.0f;
	m_target_fov_degrees = m_server_sent_fov
		? spec.is_multiplier
			? m_cache_fov * spec.fov // apply multiplier to client-set FOV
			: spec.fov // absolute override
		: m_cache_fov; // reset to client-set FOV

	m_fov_transition_active = spec.transition_time > 0.0f;
	if (m_fov_transition_active) {
		m_transition_time = spec.transition_time;
		m_fov_diff = m_target_fov_degrees - m_old_fov_degrees;
	}
}

// Returns the fractional part of x
inline f32 my_modf(f32 x)
{
	float dummy;
	return std::modf(x, &dummy);
}

void Camera::step(f32 dtime)
{
	bool was_under_zero = m_wield_change_timer < 0;
	m_wield_change_timer = MYMIN(m_wield_change_timer + dtime, 0.125);

	if (m_wield_change_timer >= 0 && was_under_zero) {
		m_wieldnode->setItem(m_wield_item_next, m_client);
		m_wieldnode->setLightColorAndAnimation(m_player_light_color,
				m_client->getAnimationTime());
	}

	if (m_view_bobbing_state != 0)
	{
		//f32 offset = dtime * m_view_bobbing_speed * 0.035;
		f32 offset = dtime * m_view_bobbing_speed * 0.030;
		if (m_view_bobbing_state == 2) {
			// Animation is getting turned off
			if (m_view_bobbing_anim < 0.25) {
				m_view_bobbing_anim -= offset;
			} else if (m_view_bobbing_anim > 0.75) {
				m_view_bobbing_anim += offset;
			} else if (m_view_bobbing_anim < 0.5) {
				m_view_bobbing_anim += offset;
				if (m_view_bobbing_anim > 0.5)
					m_view_bobbing_anim = 0.5;
			} else {
				m_view_bobbing_anim -= offset;
				if (m_view_bobbing_anim < 0.5)
					m_view_bobbing_anim = 0.5;
			}

			if (m_view_bobbing_anim <= 0 || m_view_bobbing_anim >= 1 ||
					fabs(m_view_bobbing_anim - 0.5) < 0.01) {
				m_view_bobbing_anim = 0;
				m_view_bobbing_state = 0;
			}
		}
		else {
			float was = m_view_bobbing_anim;
			m_view_bobbing_anim = my_modf(m_view_bobbing_anim + offset);
			bool step = (was == 0 ||
					(was < 0.5f && m_view_bobbing_anim >= 0.5f) ||
					(was > 0.5f && m_view_bobbing_anim <= 0.5f));
			if(step) {
				m_client->getEventManager()->put(new SimpleTriggerEvent(MtEvent::VIEW_BOBBING_STEP));
			}
		}
	}

	if (m_digging_button != -1) {
		f32 offset = dtime * 3.5f;
		float m_digging_anim_was = m_digging_anim;
		m_digging_anim += offset;
		if (m_digging_anim >= 1)
		{
			m_digging_anim = 0;
			m_digging_button = -1;
		}
		float lim = 0.15;
		if(m_digging_anim_was < lim && m_digging_anim >= lim)
		{
			if (m_digging_button == 0) {
				m_client->getEventManager()->put(new SimpleTriggerEvent(MtEvent::CAMERA_PUNCH_LEFT));
			} else if(m_digging_button == 1) {
				m_client->getEventManager()->put(new SimpleTriggerEvent(MtEvent::CAMERA_PUNCH_RIGHT));
			}
		}
	}
}

static inline v2f dir(const v2f &pos_dist)
{
	f32 x = pos_dist.X - WIELDMESH_OFFSET_X;
	f32 y = pos_dist.Y - WIELDMESH_OFFSET_Y;

	f32 x_abs = std::fabs(x);
	f32 y_abs = std::fabs(y);

	if (x_abs >= y_abs) {
		y *= (1.0f / x_abs);
		x /= x_abs;
	}

	if (y_abs >= x_abs) {
		x *= (1.0f / y_abs);
		y /= y_abs;
	}

	return v2f(std::fabs(x), std::fabs(y));
}

void Camera::addArmInertia(f32 player_yaw)
{
	m_cam_vel.X = std::fabs(rangelim(m_last_cam_pos.X - player_yaw,
		-100.0f, 100.0f) / 0.016f) * 0.01f;
	m_cam_vel.Y = std::fabs((m_last_cam_pos.Y - m_camera_direction.Y) / 0.016f);
	f32 gap_X = std::fabs(WIELDMESH_OFFSET_X - m_wieldmesh_offset.X);
	f32 gap_Y = std::fabs(WIELDMESH_OFFSET_Y - m_wieldmesh_offset.Y);

	if (m_cam_vel.X > 1.0f || m_cam_vel.Y > 1.0f) {
		/*
		    The arm moves relative to the camera speed,
		    with an acceleration factor.
		*/

		if (m_cam_vel.X > 1.0f) {
			if (m_cam_vel.X > m_cam_vel_old.X)
				m_cam_vel_old.X = m_cam_vel.X;

			f32 acc_X = 0.12f * (m_cam_vel.X - (gap_X * 0.1f));
			m_wieldmesh_offset.X += m_last_cam_pos.X < player_yaw ? acc_X : -acc_X;

			if (m_last_cam_pos.X != player_yaw)
				m_last_cam_pos.X = player_yaw;

			m_wieldmesh_offset.X = rangelim(m_wieldmesh_offset.X,
				WIELDMESH_OFFSET_X - (WIELDMESH_AMPLITUDE_X * 0.5f),
				WIELDMESH_OFFSET_X + (WIELDMESH_AMPLITUDE_X * 0.5f));
		}

		if (m_cam_vel.Y > 1.0f) {
			if (m_cam_vel.Y > m_cam_vel_old.Y)
				m_cam_vel_old.Y = m_cam_vel.Y;

			f32 acc_Y = 0.12f * (m_cam_vel.Y - (gap_Y * 0.1f));
			m_wieldmesh_offset.Y +=
				m_last_cam_pos.Y > m_camera_direction.Y ? acc_Y : -acc_Y;

			if (m_last_cam_pos.Y != m_camera_direction.Y)
				m_last_cam_pos.Y = m_camera_direction.Y;

			m_wieldmesh_offset.Y = rangelim(m_wieldmesh_offset.Y,
				WIELDMESH_OFFSET_Y - (WIELDMESH_AMPLITUDE_Y * 0.5f),
				WIELDMESH_OFFSET_Y + (WIELDMESH_AMPLITUDE_Y * 0.5f));
		}

		m_arm_dir = dir(m_wieldmesh_offset);
	} else {
		/*
		    Now the arm gets back to its default position when the camera stops,
		    following a vector, with a smooth deceleration factor.
		*/

		f32 dec_X = 0.35f * (std::min(15.0f, m_cam_vel_old.X) * (1.0f +
			(1.0f - m_arm_dir.X))) * (gap_X / 20.0f);

		f32 dec_Y = 0.25f * (std::min(15.0f, m_cam_vel_old.Y) * (1.0f +
			(1.0f - m_arm_dir.Y))) * (gap_Y / 15.0f);

		if (gap_X < 0.1f)
			m_cam_vel_old.X = 0.0f;

		m_wieldmesh_offset.X -=
			m_wieldmesh_offset.X > WIELDMESH_OFFSET_X ? dec_X : -dec_X;

		if (gap_Y < 0.1f)
			m_cam_vel_old.Y = 0.0f;

		m_wieldmesh_offset.Y -=
			m_wieldmesh_offset.Y > WIELDMESH_OFFSET_Y ? dec_Y : -dec_Y;
	}
}

void Camera::updateOffset()
{
	v3f cp = m_camera_position / BS;

	// Update offset if too far away from the center of the map
	m_camera_offset = v3s16(
		floorf(cp.X / CAMERA_OFFSET_STEP) * CAMERA_OFFSET_STEP,
		floorf(cp.Y / CAMERA_OFFSET_STEP) * CAMERA_OFFSET_STEP,
		floorf(cp.Z / CAMERA_OFFSET_STEP) * CAMERA_OFFSET_STEP
	);

	// No need to update m_cameranode as that will be done before the next render.
}

void Camera::update(LocalPlayer* player, f32 frametime, f32 tool_reload_ratio)
{
	WIELDMESH_OFFSET_X = 55.0f;

	// Get player position
	// Smooth the movement when walking up stairs
	v3f old_player_position = m_playernode->getPosition();
	v3f player_position = player->getPosition();

	f32 yaw = player->getYaw();
	f32 pitch = player->getPitch();

	// This is worse than `LocalPlayer::getPosition()` but
	// mods expect the player head to be at the parent's position
	// plus eye height.
	if (player->getParent())
		player_position = player->getParent()->getPosition() + v3f(0,  g_settings->getBool("float_above_parent") ? BS : 0, 0);

	// Smooth the camera movement after the player instantly moves upward due to stepheight.
	// The smoothing usually continues until the camera position reaches the player position.
	float player_stepheight = player->getCAO() ? player->getCAO()->getStepHeight() : HUGE_VALF;
	float upward_movement = player_position.Y - old_player_position.Y;
	if (upward_movement < 0.01f || upward_movement > player_stepheight) {
		m_stepheight_smooth_active = false;
	} else if (player->touching_ground && !g_settings->getBool("freecam")) {
		m_stepheight_smooth_active = true;
	}
	if (m_stepheight_smooth_active) {
		f32 oldy = old_player_position.Y;
		f32 newy = player_position.Y;
		f32 t = std::exp(-23 * frametime);
		player_position.Y = oldy * t + newy * (1-t);
	}

	// Set player node transformation
	m_playernode->setPosition(player_position);
	m_playernode->setRotation(v3f(0, -1 * yaw, 0));
	m_playernode->updateAbsolutePosition();

	// Get camera tilt timer (hurt animation)
	float cameratilt = fabs(fabs(player->hurt_tilt_timer-0.75)-0.75);

	// Calculate and translate the head SceneNode offsets
	{
		v3f eye_offset = player->getEyeOffset();
		switch(m_camera_mode) {
		case CAMERA_MODE_ANY:
			assert(false);
			break;
		case CAMERA_MODE_FIRST:
			eye_offset += player->eye_offset_first;
			break;
		case CAMERA_MODE_THIRD:
			eye_offset += player->eye_offset_third;
			break;
		case CAMERA_MODE_THIRD_FRONT:
			eye_offset.X += player->eye_offset_third_front.X;
			eye_offset.Y += player->eye_offset_third_front.Y;
			eye_offset.Z -= player->eye_offset_third_front.Z;
			break;
		}

		// Set head node transformation
		eye_offset.Y += cameratilt * -player->hurt_tilt_strength;
		m_headnode->setPosition(eye_offset);
		m_headnode->setRotation(v3f(pitch, 0,
			cameratilt * player->hurt_tilt_strength));
		m_headnode->updateAbsolutePosition();
	}


	// Compute relative camera position and target
	v3f rel_cam_pos = v3f(0,0,0);
	v3f rel_cam_target = v3f(0,0,1);
	v3f rel_cam_up = v3f(0,1,0);

	if (m_cache_view_bobbing_amount != 0.0f && m_view_bobbing_anim != 0.0f &&
		m_camera_mode < CAMERA_MODE_THIRD) {
		f32 bobfrac = my_modf(m_view_bobbing_anim * 2);
		f32 bobdir = (m_view_bobbing_anim < 0.5) ? 1.0 : -1.0;

		f32 bobknob = 1.2;
		f32 bobtmp = std::sin(std::pow(bobfrac, bobknob) * M_PI);

		v3f bobvec = v3f(
			0.3 * bobdir * std::sin(bobfrac * M_PI),
			-0.28 * bobtmp * bobtmp,
			0.);

		rel_cam_pos += bobvec * m_cache_view_bobbing_amount;
		rel_cam_target += bobvec * m_cache_view_bobbing_amount;
		rel_cam_up.rotateXYBy(-0.03 * bobdir * bobtmp * M_PI * m_cache_view_bobbing_amount);
	}

	// Compute absolute camera position and target
	m_headnode->getAbsoluteTransformation().transformVect(m_camera_position, rel_cam_pos);
	m_camera_direction = m_headnode->getAbsoluteTransformation()
			.rotateAndScaleVect(rel_cam_target - rel_cam_pos);

	v3f abs_cam_up = m_headnode->getAbsoluteTransformation()
			.rotateAndScaleVect(rel_cam_up);

	// Reposition the camera for third person view
	if (m_camera_mode > CAMERA_MODE_FIRST)
	{
		v3f my_cp = m_camera_position;

		if (m_camera_mode == CAMERA_MODE_THIRD_FRONT)
			m_camera_direction *= -1;

		my_cp.Y += 2;

		// Calculate new position
		bool abort = false;
		for (int i = BS; i <= BS * 2.75; i++) {
			my_cp.X = m_camera_position.X + m_camera_direction.X * -i;
			my_cp.Z = m_camera_position.Z + m_camera_direction.Z * -i;
			if (i > 12)
				my_cp.Y = m_camera_position.Y + (m_camera_direction.Y * -i);

			// Prevent camera positioned inside nodes
			const NodeDefManager *nodemgr = m_client->ndef();
			MapNode n = m_client->getEnv().getClientMap()
				.getNode(floatToInt(my_cp, BS));

			const ContentFeatures& features = nodemgr->get(n);
			if (features.walkable) {
				my_cp.X += m_camera_direction.X*-1*-BS/2;
				my_cp.Z += m_camera_direction.Z*-1*-BS/2;
				my_cp.Y += m_camera_direction.Y*-1*-BS/2;
				abort = true;
				break;
			}
		}

		// If node blocks camera position don't move y to heigh
		if (abort && my_cp.Y > player_position.Y+BS*2)
			my_cp.Y = player_position.Y+BS*2;

		// update the camera position in third-person mode to render blocks behind player
		// and correctly apply liquid post FX.
		m_camera_position = my_cp;
	}

	// Set camera node transformation
	m_cameranode->setPosition(m_camera_position - intToFloat(m_camera_offset, BS));
	m_cameranode->setUpVector(abs_cam_up);
	m_cameranode->updateAbsolutePosition();
	// *100 helps in large map coordinates
	m_cameranode->setTarget(m_camera_position - intToFloat(m_camera_offset, BS)
		+ 100 * m_camera_direction);

	/*
	 * Apply server-sent FOV, instantaneous or smooth transition.
	 * If not, check for zoom and set to zoom FOV.
	 * Otherwise, default to m_cache_fov.
	 */
	if (m_fov_transition_active) {
		// Smooth FOV transition
		// Dynamically calculate FOV delta based on frametimes
		f32 delta = (frametime / m_transition_time) * m_fov_diff;
		m_curr_fov_degrees += delta;

		// Mark transition as complete if target FOV has been reached
		if ((m_fov_diff > 0.0f && m_curr_fov_degrees >= m_target_fov_degrees) ||
				(m_fov_diff < 0.0f && m_curr_fov_degrees <= m_target_fov_degrees)) {
			m_fov_transition_active = false;
			m_curr_fov_degrees = m_target_fov_degrees;
		}
	} else if (m_server_sent_fov) {
		// Instantaneous FOV change
		m_curr_fov_degrees = m_target_fov_degrees;
	} else if (player->getPlayerControl().zoom && player->getZoomFOV() > 0.001f) {
		// Player requests zoom, apply zoom FOV
		m_curr_fov_degrees = player->getZoomFOV();
	} else {
		// Set to client's selected FOV
		m_curr_fov_degrees = m_cache_fov;
	}
	if (g_settings->getBool("fov_setting")) {
		m_curr_fov_degrees = rangelim(g_settings->getFloat("fov.step"), 1.0f, 160.0f);
	} else {
		m_curr_fov_degrees = rangelim(m_curr_fov_degrees, 1.0f, 160.0f);
	}

	// FOV and aspect ratio
	const v2u32 &window_size = RenderingEngine::getWindowSize();
	m_aspect = (f32) window_size.X / (f32) window_size.Y;
	m_fov_y = m_curr_fov_degrees * M_PI / 180.0;
	// Increase vertical FOV on lower aspect ratios (<16:10)
	m_fov_y *= core::clamp(sqrt(16./10. / m_aspect), 1.0, 1.4);
	m_fov_x = 2 * atan(m_aspect * tan(0.5 * m_fov_y));
	m_cameranode->setAspectRatio(m_aspect);
	m_cameranode->setFOV(m_fov_y);

	// Make new matrices and frustum
	m_cameranode->updateMatrices();

	if (m_arm_inertia && !lagOptimizerNoHandAnimation())
		addArmInertia(yaw);

	// Position the wielded item
	v3f wield_position = v3f(m_wieldmesh_offset.X, m_wieldmesh_offset.Y, 65);
	v3f wield_rotation = v3f(-100, 120, -100);
	const bool no_hand_animation = lagOptimizerNoHandAnimation();
	if (!no_hand_animation) {
		wield_position.Y += std::abs(m_wield_change_timer) * 320 - 40;
		if (m_digging_anim < 0.05 || m_digging_anim > 0.5) {
			f32 frac = 1.0;
			if (m_digging_anim > 0.5)
				frac = 2.0 * (m_digging_anim - 0.5);
			// This value starts from 1 and settles to 0
			f32 ratiothing = std::pow((1.0f - tool_reload_ratio), 0.5f);
			f32 ratiothing2 = (easeCurve(ratiothing*0.5))*2.0;
			wield_position.Y -= frac * 25.0f * std::pow(ratiothing2, 1.7f);
			wield_position.X -= frac * 35.0f * std::pow(ratiothing2, 1.1f);
			wield_rotation.Y += frac * 70.0f * std::pow(ratiothing2, 1.4f);
		}
		if (m_digging_button != -1) {
			f32 digfrac = m_digging_anim;
			wield_position.X -= 50 * std::sin(std::pow(digfrac, 0.8f) * M_PI);
			wield_position.Y += 24 * std::sin(digfrac * 1.8 * M_PI);
			wield_position.Z += 25 * 0.5;

			// Euler angles are PURE EVIL, so why not use quaternions?
			core::quaternion quat_begin(wield_rotation * core::DEGTORAD);
			core::quaternion quat_end(v3f(80, 30, 100) * core::DEGTORAD);
			core::quaternion quat_slerp;
			quat_slerp.slerp(quat_begin, quat_end, std::sin(digfrac * M_PI));
			quat_slerp.toEuler(wield_rotation);
			wield_rotation *= core::RADTODEG;
		} else {
			f32 bobfrac = my_modf(m_view_bobbing_anim);
			wield_position.X -= std::sin(bobfrac*M_PI*2.0) * 3.0;
			wield_position.Y += std::sin(my_modf(bobfrac*2.0)*M_PI) * 3.0;
		}
	} else if (m_digging_button != -1) {
		// Keep the weapon in place, but preserve the swing rotation.
		f32 digfrac = m_digging_anim;
		if (m_digging_anim < 0.05 || m_digging_anim > 0.5) {
			f32 frac = 1.0;
			if (m_digging_anim > 0.5)
				frac = 2.0 * (m_digging_anim - 0.5);
			f32 ratiothing = std::pow((1.0f - tool_reload_ratio), 0.5f);
			f32 ratiothing2 = (easeCurve(ratiothing * 0.5f)) * 2.0f;
			wield_rotation.Y += frac * 70.0f * std::pow(ratiothing2, 1.4f);
		}
		core::quaternion quat_begin(wield_rotation * core::DEGTORAD);
		core::quaternion quat_end(v3f(80, 30, 100) * core::DEGTORAD);
		core::quaternion quat_slerp;
		quat_slerp.slerp(quat_begin, quat_end, std::sin(digfrac * M_PI));
		quat_slerp.toEuler(wield_rotation);
		wield_rotation *= core::RADTODEG;
	}
	if (g_settings->getBool("hand_view")) {
		const v3f hand_view_offset(
			core::clamp(g_settings->getFloat("hand_view.x", -100.0f, 100.0f), -12.0f, 12.0f),
			core::clamp(g_settings->getFloat("hand_view.y", -100.0f, 100.0f), -12.0f, 12.0f),
			core::clamp(g_settings->getFloat("hand_view.z", -100.0f, 100.0f), -24.0f, 24.0f));
		wield_position += hand_view_offset;
	}
	// Apply left-hand mirroring using quaternions
	if (g_settings->getBool("left_hand")) {
		// Mirror position across X-axis
		wield_position.X = -wield_position.X;

		// Convert to quaternion
		core::quaternion quat(wield_rotation * core::DEGTORAD);

		// Mirror across X-axis (flip X and W)
		quat.X = -quat.X;
		quat.W = -quat.W;

		// Apply +90 degrees pitch correction
		core::quaternion pitchFix(v3f(180, 0, 180) * core::DEGTORAD);
		quat = pitchFix * quat;

		// Convert back to Euler
		quat.toEuler(wield_rotation);
		wield_rotation *= core::RADTODEG;
	}
	m_wieldnode->setPosition(wield_position);
	m_wieldnode->setRotation(wield_rotation);

	m_player_light_color = player->light_color;
	m_wieldnode->setLightColorAndAnimation(m_player_light_color,
			m_client->getAnimationTime());

	// Set render distance
	updateViewingRange();

	// If the player is walking, swimming, or climbing,
	// view bobbing is enabled and free_move is off,
	// start (or continue) the view bobbing animation.
	const v3f &speed = player->getSpeed();
	const bool movement_XZ = std::hypot(speed.X, speed.Z) > BS;
	const bool movement_Y = std::abs(speed.Y) > BS;

	const bool walking = movement_XZ && player->touching_ground;
	const bool swimming = (movement_XZ || player->swimming_vertical) && player->in_liquid;
	const bool climbing = movement_Y && player->is_climbing;
	const bool flying = g_settings->getBool("free_move")
		&& m_client->checkLocalPrivilege("fly");
	if ((walking || swimming || climbing) && !flying && !g_settings->getBool("nobob")) {
		// Start animation
		m_view_bobbing_state = 1;
		m_view_bobbing_speed = MYMIN(speed.getLength(), 70);
	} else if (m_view_bobbing_state == 1) {
		// Stop animation
		m_view_bobbing_state = 2;
		m_view_bobbing_speed = 60;
	}
}

void Camera::updateViewingRange()
{
	f32 viewing_range = g_settings->getFloat("viewing_range");

	m_cameranode->setNearValue(0.1f * BS);

	m_draw_control.wanted_range = std::fmin(adjustDist(viewing_range, getFovMax()), 4000);
	if (m_draw_control.range_all) {
		m_cameranode->setFarValue(100000.0);
		return;
	}
	m_cameranode->setFarValue((viewing_range < 2000) ? 2000 * BS : viewing_range * BS);
}

void Camera::setDigging(s32 button)
{
	if (m_digging_button == -1)
		m_digging_button = button;
}

void Camera::wield(const ItemStack &item)
{
	if (lagOptimizerNoHandAnimation()) {
		if (item.name != m_wield_item_next.name || item.metadata != m_wield_item_next.metadata) {
			m_wield_item_next = item;
			m_wield_change_timer = 0;
			if (m_wieldnode) {
				m_wieldnode->setItem(m_wield_item_next, m_client);
				m_wieldnode->setLightColorAndAnimation(m_player_light_color,
					m_client->getAnimationTime());
			}
		}
		return;
	}
	if (item.name != m_wield_item_next.name ||
			item.metadata != m_wield_item_next.metadata) {
		m_wield_item_next = item;
		if (m_wield_change_timer > 0)
			m_wield_change_timer = -m_wield_change_timer;
		else if (m_wield_change_timer == 0)
			m_wield_change_timer = -0.001;
	}
}

void Camera::drawWieldedTool(irr::core::matrix4* translation)
{
	// Clear Z buffer so that the wielded tool stays in front of world geometry
	m_wieldmgr->getVideoDriver()->clearBuffers(video::ECBF_DEPTH);

	// Draw the wielded node (in a separate scene manager)
	scene::ICameraSceneNode* cam = m_wieldmgr->getActiveCamera();
	cam->setAspectRatio(m_cameranode->getAspectRatio());
	cam->setFOV(72.0*M_PI/180.0);
	cam->setNearValue(10);
	cam->setFarValue(1000);
	if (translation != NULL)
	{
		irr::core::matrix4 startMatrix = cam->getAbsoluteTransformation();
		irr::core::vector3df focusPoint = (cam->getTarget()
				- cam->getAbsolutePosition()).setLength(1)
				+ cam->getAbsolutePosition();

		irr::core::vector3df camera_pos =
				(startMatrix * *translation).getTranslation();
		cam->setPosition(camera_pos);
		cam->updateAbsolutePosition();
		cam->setTarget(focusPoint);
	}
	m_wieldmgr->drawAll();
}

void Camera::drawNametags()
{
	core::matrix4 trans = m_cameranode->getProjectionMatrix();
	trans *= m_cameranode->getViewMatrix();

	gui::IGUIFont *font = g_fontengine->getFont();
	video::IVideoDriver *driver = RenderingEngine::get_video_driver();
	v2u32 screensize = driver->getScreenSize();

	for (const Nametag *nametag : m_nametags) {
		// Nametags are hidden in GenericCAO::updateNametag()

		v3f pos = nametag->parent_node->getAbsolutePosition() + nametag->pos * BS;
		f32 transformed_pos[4] = { pos.X, pos.Y, pos.Z, 1.0f };
		trans.multiplyWith1x4Matrix(transformed_pos);
		if (transformed_pos[3] > 0) {
			std::wstring nametag_colorless =
				unescape_translate(utf8_to_wide(nametag->text));
			core::dimension2d<u32> textsize = font->getDimension(
				nametag_colorless.c_str());
			f32 zDiv = transformed_pos[3] == 0.0f ? 1.0f :
				core::reciprocal(transformed_pos[3]);
			v2s32 screen_pos;
			screen_pos.X = screensize.X *
				(0.5 * transformed_pos[0] * zDiv + 0.5) - textsize.Width / 2;
			screen_pos.Y = screensize.Y *
				(0.5 - transformed_pos[1] * zDiv * 0.5) - textsize.Height / 2;
			core::rect<s32> size(0, 0, std::max(textsize.Width, (u32) nametag->images_dim.Width), textsize.Height + nametag->images_dim.Height);

			auto bgcolor = g_settings->getBool("hud.enabled") && m_show_nametag_backgrounds ?
				readModuleBackgroundColor() : nametag->getBgColor(m_show_nametag_backgrounds);
			if (bgcolor.getAlpha() != 0) {
				const s32 padding = 2;
				core::rect<s32> bg_size(-padding, -padding,
					textsize.Width + padding, textsize.Height + padding);
				const core::rect<s32> bg_rect = bg_size + screen_pos;
				driver->draw2DRectangle(bgcolor, bg_rect);
				if (g_settings->getBool("hud.enabled"))
					driver->draw2DRectangleOutline(bg_rect, readModuleBorderColor(), 1);
			}

			font->draw(
				translate_string(utf8_to_wide(nametag->text)).c_str(),
				size + screen_pos, nametag->textcolor);

				v2s32 image_pos(screen_pos);
			image_pos.Y += textsize.Height;

			const video::SColor color(255, 255, 255, 255);
			const video::SColor colors[] = {color, color, color, color};

			for (video::ITexture *texture : nametag->images) {
				core::dimension2di imgsize(texture->getOriginalSize());
				core::rect<s32> rect(core::position2d<s32>(0, 0), imgsize);
				draw2DImageFilterScaled(driver, texture, rect + image_pos, rect, NULL, colors, true);
				image_pos += core::dimension2di(imgsize.Width, 0);
			}
		}
	}
}

void Camera::drawDiffNametag(float dtime)
{
    ClientEnvironment &env = m_client->getEnv();
    gui::IGUIFont *font = g_fontengine->getFont();
    if (!font)
        return;

    v3f origin = getPosition();

    std::vector<DistanceSortedActiveObject> sortedObjects;
    env.getAllActiveObjects(origin, sortedObjects);

    video::IVideoDriver *driver = RenderingEngine::get_video_driver();
    if (!driver)
        return;

    core::matrix4 trans =
        m_cameranode->getProjectionMatrix() * m_cameranode->getViewMatrix();

    v2u32 screensize = driver->getScreenSize();

    for (auto &sortedObj : sortedObjects) {
        ClientActiveObject *cao = sortedObj.obj;
        GenericCAO *obj = dynamic_cast<GenericCAO *>(cao);

        if (!obj)
            continue;
        const bool allowSelf = g_settings->getBool("nametags.self") &&
            (g_settings->getBool("freecam") || g_settings->getBool("detached_camera"));
        const bool showSelf = allowSelf;
        if (obj->isLocalPlayer() && !showSelf)
            continue;
        if (!obj->isPlayer())
            continue;

        scene::ISceneNode *node = obj->getSceneNode();
        if (!node)
            continue;

        int h = stoi(g_settings->get("nametags.height"));
        if (h < 1 || h > 9)
            h = 5;
        float heightdiff = (float)h;

        aabb3f box(v3f(0,0,0), v3f(0,0,0));
        obj->getCollisionBox(&box);
        v3f textPos = node->getAbsolutePosition();
        core::aabbox3d<f32> nodeBox = node->getBoundingBox();
        textPos.Y += (nodeBox.MaxEdge.Y - nodeBox.MinEdge.Y) + heightdiff;

        f32 pos4[4] = { textPos.X, textPos.Y, textPos.Z, 1.0f };
        trans.multiplyWith1x4Matrix(pos4);

        if (pos4[3] <= 0.0f)
            continue;

        f32 zDiv = 1.0f / pos4[3];

        v2s32 screen;
        screen.X = screensize.X * (0.5f * pos4[0] * zDiv + 0.5f);
        screen.Y = screensize.Y * (0.5f - 0.5f * pos4[1] * zDiv);

        std::string name = obj->getName();
        int hp = obj->getHp();

        const bool showHp = g_settings->getBool("nametags.hp");
        const bool showStatus = g_settings->getBool("nametags.status");
        const bool showDistance = g_settings->getBool("nametags.distance");
        const bool showItemNames = g_settings->getBool("nametags.item_names");
        const bool showWieldedItems = g_settings->getBool("nametags.wielded_items");
        const bool showEquipment = g_settings->getBool("nametags.armor");
        const bool showLunaStats = g_settings->getBool("luna_stats.enabled") &&
            g_settings->getBool("luna_stats.nametags");

        LocalPlayer *lp = m_client->getEnv().getLocalPlayer();
        if (!lp)
            continue;
        int relation_int = lp->getEntityRelationship(obj);

        std::string relationStr;
        video::SColor relationColor(255, 128, 128, 128);

        switch (relation_int) {
            case 0:  // FRIEND
                relationStr = "[Friend]";
                relationColor = video::SColor(255, 255, 0, 255); // pink
                break;

            case 1:  // ENEMY
                relationStr = "[Enemy]";
                relationColor = video::SColor(255, 255, 0, 0); // red
                break;

            case 2:  // ALLY
                relationStr = "[Ally]";
                relationColor = video::SColor(255, 0, 255, 0); // green
                break;

            case 3:  // NEUTRAL
                relationStr = "[Neutral]";
                relationColor = video::SColor(255, 160, 160, 160); // grey
                break;

            default: // STAFF
                relationStr = "[Staff]";
                relationColor = video::SColor(255, 0, 0, 255); // blue
                break;
        }

        std::wstring wname = utf8_to_wide(name);
        std::wstring whp = utf8_to_wide(std::to_string(hp));
        std::wstring wrelation = utf8_to_wide(relationStr);
        std::wstring wdistance;
        if (showDistance) {
            const f32 distance_blocks_f = lp->getPosition().getDistanceFrom(obj->getPosition()) / BS;
            const s32 distance_blocks = std::max(0, static_cast<s32>(std::floor(distance_blocks_f + 0.5f)));
            wdistance = utf8_to_wide(" [" + itos(distance_blocks) + "m away]");
        }

        auto dim_name = font->getDimension(wname.c_str());
        auto dim_hp = font->getDimension(whp.c_str());
        auto dim_rel = font->getDimension(wrelation.c_str());
        auto dim_distance = showDistance ? font->getDimension(wdistance.c_str()) : core::dimension2d<u32>(0, 0);
        auto luna_stats = showLunaStats ? player_stats::getLine(name) : std::nullopt;
        if (luna_stats)
            *luna_stats = L"[" + *luna_stats + L"]";
        const auto dim_luna_stats = luna_stats ? font->getDimension(luna_stats->c_str()) :
            core::dimension2d<u32>(0, 0);

        std::vector<NametagItemToken> item_tokens;
        if (showWieldedItems)
            collect_visible_wield_tokens(item_tokens, m_client, obj, allowSelf);
        if (showEquipment)
            collect_visible_armor_tokens(item_tokens, m_client, obj, allowSelf);

        const f32 icon_scale = core::clamp(g_settings->getFloat("nametags.icon_scale", 0.5f, 4.0f), 0.5f, 4.0f);
        const s32 icon_size = std::max<s32>(20, std::min<s32>(72,
            static_cast<s32>(std::round((dim_name.Height + 8) * icon_scale))));
        const s32 row_gap = 1;
        const s32 icon_gap = 2;
        const s32 item_line_height = std::max<s32>(dim_name.Height, 10) + 1;
        int lineWidth = dim_name.Width;
        if (showDistance)
            lineWidth += 8 + dim_distance.Width;
        if (luna_stats)
            lineWidth += 8 + dim_luna_stats.Width;
        if (showHp)
            lineWidth += 8 + dim_hp.Width;
        if (showStatus)
            lineWidth += 8 + dim_rel.Width;

        const s32 icon_row_width = measure_icon_row_width(item_tokens, icon_size, icon_gap);
        int textWidth = lineWidth;
        int nameHeight = std::max({dim_name.Height,
                               showDistance ? dim_distance.Height : 0,
                               showHp ? dim_hp.Height : 0,
                               showStatus ? dim_rel.Height : 0});
        int itemNamesHeight = 0;
        if (!item_tokens.empty()) {
            if (showItemNames) {
                itemNamesHeight = static_cast<s32>(item_tokens.size()) * item_line_height;
                textWidth = std::max<s32>(textWidth, measure_item_name_width(font, item_tokens));
            }
        }
        int totalHeight = (item_tokens.empty() ? 0 : icon_size + row_gap) +
            itemNamesHeight + nameHeight;

        int text_x = screen.X - textWidth / 2;
        const int name_x = screen.X - lineWidth / 2;
        int y = screen.Y - totalHeight / 2;
        bool drew_text_bg = false;
        const s32 nametag_padding = 2;
        const video::SColor nametag_background = g_settings->getBool("hud.enabled") ?
            readModuleBackgroundColor() : video::SColor(140, 0, 0, 0);
        if (!item_tokens.empty()) {
            draw_icon_row(driver, font, m_client, item_tokens, v2s32(screen.X, y), icon_size, icon_gap);
            y += icon_size + row_gap;

            if (showItemNames) {
                if (m_show_nametag_backgrounds) {
                    core::rect<s32> bg_rect(
                        text_x - nametag_padding,
                        y - nametag_padding,
                        text_x + std::max(textWidth, icon_row_width) + nametag_padding,
                        y + itemNamesHeight + nameHeight + nametag_padding);
                    driver->draw2DRectangle(nametag_background, bg_rect);
                    if (g_settings->getBool("hud.enabled"))
                        driver->draw2DRectangleOutline(bg_rect, readModuleBorderColor(), 1);
                    drew_text_bg = true;
                }
                draw_item_name_rows(font, item_tokens, v2s32(screen.X, y),
                        y, item_line_height);
                y += itemNamesHeight;
            }
        }

        if (m_show_nametag_backgrounds && !drew_text_bg) {
            core::rect<s32> bg_rect(
                text_x - nametag_padding,
                y - nametag_padding,
                text_x + textWidth + nametag_padding,
                y + nameHeight + nametag_padding);
            driver->draw2DRectangle(nametag_background, bg_rect);
            if (g_settings->getBool("hud.enabled"))
                driver->draw2DRectangleOutline(bg_rect, readModuleBorderColor(), 1);
        }

        core::rect<s32> rectName(name_x, y, name_x + dim_name.Width, y + dim_name.Height);

        font->draw(wname.c_str(), rectName,
                   video::SColor(255, 255, 255, 255),
                   false, false);

        int offsetX = name_x + dim_name.Width + 8;

        if (showDistance) {
            core::rect<s32> rectDistance(offsetX, y,
                                         offsetX + dim_distance.Width,
                                         y + dim_distance.Height);

            font->draw(wdistance.c_str(), rectDistance,
                       video::SColor(255, 180, 180, 180),
                       false, false);

            offsetX += dim_distance.Width + 8;
        }

        if (luna_stats) {
            core::rect<s32> rectStats(offsetX, y,
                                      offsetX + dim_luna_stats.Width,
                                      y + dim_luna_stats.Height);

            font->draw(luna_stats->c_str(), rectStats,
                       video::SColor(255, 160, 160, 160),
                       false, false);

            offsetX += dim_luna_stats.Width + 8;
        }

        if (showHp) {
            core::rect<s32> rectHp(offsetX, y,
                                   offsetX + dim_hp.Width,
                                   y + dim_hp.Height);

            font->draw(whp.c_str(), rectHp,
                       video::SColor(255, 0, 255, 0),
                       false, false);

            offsetX += dim_hp.Width + 8;
        }

        if (showStatus) {
            core::rect<s32> rectRel(offsetX, y,
                                    offsetX + dim_rel.Width,
                                    y + dim_rel.Height);

            font->draw(wrelation.c_str(), rectRel,
                       relationColor,
                       false, false);
        }

    }
}


/// @brief

double Camera::getInterpolatedHealth(const GenericCAO *obj, float dtime) {
	ActiveObject::object_t id = obj->getId();

	auto it = m_interpolated_entity_health.find(id);

	double currentHp = static_cast<double>(obj->getHp());

	if (it != m_interpolated_entity_health.end()) {
		double interpolatedHealth = it->second;

		double healthDifference = currentHp - interpolatedHealth;

		double change = healthDifference * (dtime * 8);

		interpolatedHealth += change;

		m_interpolated_entity_health[id] = interpolatedHealth;

		return interpolatedHealth;
	} else {
		m_interpolated_entity_health[id] = currentHp;
		return currentHp;
	}
}

void Camera::drawHealthESP(float dtime)
{
    ClientEnvironment &env = m_client->getEnv();
    gui::IGUIFont *font = g_fontengine->getFont();

    v3f origin = getPosition();

    std::vector<DistanceSortedActiveObject> sortedObjects;
    env.getAllActiveObjects(origin, sortedObjects);

    f32 fovScale = 72 / m_curr_fov_degrees;

    video::IVideoDriver *driver = RenderingEngine::get_video_driver();
    core::matrix4 trans = m_cameranode->getProjectionMatrix() * m_cameranode->getViewMatrix();
    v2u32 screensize = driver->getScreenSize();

    for (auto &sortedObj : sortedObjects) {
        ClientActiveObject *cao = sortedObj.obj;
        GenericCAO *obj = dynamic_cast<GenericCAO *>(cao);
        if (!obj)
            continue;

        if (obj->isLocalPlayer() || !obj->canAttack(1))
            continue;
		
        if (!obj->isPlayer() && g_settings->getBool("enable_health_esp.players_only"))
            continue;

        scene::ISceneNode* node = obj->getSceneNode();
		if (!node) {
		    continue;
		}

		v3f textPos = node->getAbsolutePosition();


        textPos.Y += 9.0f;
        f32 transformed_pos[4] = { textPos.X, textPos.Y, textPos.Z, 1.0f };

        trans.multiplyWith1x4Matrix(transformed_pos);
        if (transformed_pos[3] > 0) {
            if (g_settings->exists("enable_health_esp.type") && g_settings->get("enable_health_esp.type") == "Health Bar") {
                double health_percentage = obj->getProperties().hp_max > 0 ? static_cast<double>(getInterpolatedHealth(obj, dtime)) / obj->getProperties().hp_max : 0.0;
                health_percentage = std::max(0.0, std::min(1.0, health_percentage));

                video::SColor backgroundColor(255, 5, 10, 15);
                video::SColor borderColor(255, 0, 0, 0);
                u8 red = static_cast<u8>(255 * (1.0f - health_percentage));
                u8 green = static_cast<u8>(255 * health_percentage);
                video::SColor filledColor = video::SColor(255, red, green, 0);

                f32 zDiv = transformed_pos[3] == 0.0f ? 1.0f : core::reciprocal(transformed_pos[3]);
                f32 scale = zDiv;
                scale = std::min(scale, 3.0f);

                s32 barWidth = static_cast<s32>((1000 * scale) * fovScale);
                s32 barHeight = static_cast<s32>((15000 * scale) * fovScale);
                s32 baseBarOffset = static_cast<s32>((6000 * scale) * fovScale);

                v2s32 screen_pos;
                screen_pos.X = screensize.X * (0.5 * transformed_pos[0] * zDiv + 0.5) - barWidth / 2;
                screen_pos.Y = screensize.Y * (0.5 - transformed_pos[1] * zDiv * 0.5) - barHeight / 2;

                core::rect<s32> barRect(baseBarOffset, 0, baseBarOffset + barWidth, barHeight);
                s32 fillHeight = static_cast<s32>(barRect.getHeight() * health_percentage);
                core::rect<s32> filledRect(
                    barRect.UpperLeftCorner.X,
                    barRect.LowerRightCorner.Y - fillHeight,
                    barRect.LowerRightCorner.X,
                    barRect.LowerRightCorner.Y
                );

                driver->draw2DRectangle(backgroundColor, barRect + screen_pos);
                driver->draw2DRectangle(filledColor, filledRect + screen_pos);
                driver->draw2DRectangleOutline(barRect + screen_pos, borderColor, barWidth * 0.2);
                continue;
            }

            std::string hpText = "HP: " + std::to_string(obj->getHp());
            core::dimension2d<u32> textsize = font->getDimension(utf8_to_wide(hpText).c_str());
            f32 zDiv = transformed_pos[3] == 0.0f ? 1.0f : core::reciprocal(transformed_pos[3]);
            v2s32 screen_pos;
            screen_pos.X = screensize.X * (0.5 * transformed_pos[0] * zDiv + 0.5) - textsize.Width / 2;
            screen_pos.Y = screensize.Y * (0.5 - transformed_pos[1] * zDiv * 0.5) - textsize.Height / 2;

            core::rect<s32> size(0, 0, textsize.Width, textsize.Height);
            if (font) {
                font->draw(
                    utf8_to_wide(hpText).c_str(),
                    size + screen_pos,
                    video::SColor(255, 255, 255, 255),
                    true,
                    true
                );
            }
        }
    }
}

Nametag *Camera::addNametag(scene::ISceneNode *parent_node,
		const std::string &text, video::SColor textcolor,
		std::optional<video::SColor> bgcolor, const v3f &pos, const std::vector<std::string> &images)
{
	Nametag *nametag = new Nametag(parent_node, text, textcolor, bgcolor, pos, m_client->tsrc(), images);
	m_nametags.push_back(nametag);
	return nametag;
}

void Camera::removeNametag(Nametag *nametag)
{
	m_nametags.remove(nametag);
	delete nametag;
}

std::array<core::plane3d<f32>, 4> Camera::getFrustumCullPlanes() const
{
	using irr::scene::SViewFrustum;
	const auto &frustum_planes = m_cameranode->getViewFrustum()->planes;
	return {
		frustum_planes[SViewFrustum::VF_LEFT_PLANE],
		frustum_planes[SViewFrustum::VF_RIGHT_PLANE],
		frustum_planes[SViewFrustum::VF_BOTTOM_PLANE],
		frustum_planes[SViewFrustum::VF_TOP_PLANE],
	};
}
