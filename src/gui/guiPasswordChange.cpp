/*
Part of Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013 Ciaran Gultnieks <ciaran@ciarang.com>

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "guiPasswordChange.h"
#include "client/client.h"
#include "guiButton.h"
#include <IGUICheckBox.h>
#include <IGUIEditBox.h>
#include <IGUIButton.h>
#include <IGUIStaticText.h>
#include <IGUIFont.h>
#include <IVideoDriver.h>

#include "porting.h"
#include "gettext.h"

const int ID_oldPassword = 256;
const int ID_newPassword1 = 257;
const int ID_newPassword2 = 258;
const int ID_change = 259;
const int ID_message = 260;
const int ID_cancel = 261;
const int ID_random = 262;

GUIPasswordChange::GUIPasswordChange(gui::IGUIEnvironment* env,
		gui::IGUIElement* parent, s32 id,
		IMenuManager *menumgr,
		Client* client,
		ISimpleTextureSource *tsrc
):
	GUIModalMenu(env, parent, id, menumgr),
	m_client(client),
	m_oldpass(utf8_to_wide(client->getPassword())),
	m_tsrc(tsrc)
{
}

void GUIPasswordChange::regenerateGui(v2u32 screensize)
{
	/*
		save current input
	*/
	acceptInput();

	/*
		Remove stuff
	*/
	removeAllChildren();

	/*
		Calculate new sizes and positions
	*/
	ScalingInfo info = getScalingInfo(screensize, v2u32(700, 420));
	const float s = info.scale;
	DesiredRect = info.rect;
	recalculateAbsolutePosition(false);

	v2s32 size = DesiredRect.getSize();
	v2s32 topleft_client(35 * s, 0);

	/*
		Add stuff
	*/
	s32 ypos = 45 * s;
	{
		core::rect<s32> rect(0, 0, 150 * s, 20 * s);
		rect += topleft_client + v2s32(25 * s, ypos + 6 * s);
		gui::StaticText::add(Environment, wstrgettext("Old Password"), rect,
				false, true, this, -1);
	}
	{
		core::rect<s32> rect(0, 0, 380 * s, 30 * s);
		rect += topleft_client + v2s32(175 * s, ypos);
		gui::IGUIEditBox *e = Environment->addEditBox(
				m_oldpass.c_str(), rect, true, this, ID_oldPassword);
		Environment->setFocus(e);
		e->setPasswordBox(true);
	}
	ypos += 55 * s;
	{
		core::rect<s32> rect(0, 0, 150 * s, 20 * s);
		rect += topleft_client + v2s32(25 * s, ypos + 6 * s);
		gui::StaticText::add(Environment, wstrgettext("New Password"), rect,
				false, true, this, -1);
	}
	{
		core::rect<s32> rect(0, 0, 265 * s, 30 * s);
		rect += topleft_client + v2s32(175 * s, ypos);
		gui::IGUIEditBox *e = Environment->addEditBox(
				m_newpass.c_str(), rect, true, this, ID_newPassword1);
		e->setPasswordBox(true);
	}
	{
		core::rect<s32> rect(0, 0, 105 * s, 30 * s);
		rect += topleft_client + v2s32(450 * s, ypos);
		GUIButton::addButton(Environment, rect, m_tsrc, this, ID_random,
				wstrgettext("Random").c_str());
	}
	ypos += 55 * s;
	{
		core::rect<s32> rect(0, 0, 150 * s, 20 * s);
		rect += topleft_client + v2s32(25 * s, ypos + 6 * s);
		gui::StaticText::add(Environment, wstrgettext("Confirm Password"), rect,
				false, true, this, -1);
	}
	{
		core::rect<s32> rect(0, 0, 380 * s, 30 * s);
		rect += topleft_client + v2s32(175 * s, ypos);
		gui::IGUIEditBox *e = Environment->addEditBox(
				m_newpass_confirm.c_str(), rect, true, this, ID_newPassword2);
		e->setPasswordBox(true);
	}

	ypos += 60 * s;
	{
		core::rect<s32> rect(0, 0, 120 * s, 32 * s);
		rect = rect + v2s32(size.X / 2 - 135 * s, ypos);
		GUIButton::addButton(Environment, rect, m_tsrc, this, ID_change,
				wstrgettext("Change").c_str());
	}
	{
		core::rect<s32> rect(0, 0, 120 * s, 32 * s);
		rect = rect + v2s32(size.X / 2 + 15 * s, ypos);
		GUIButton::addButton(Environment, rect, m_tsrc, this, ID_cancel,
				wstrgettext("Cancel").c_str());
	}

	ypos += 45 * s;
	{
		core::rect<s32> rect(0, 0, 470 * s, 36 * s);
		rect += topleft_client + v2s32(110 * s, ypos);
		IGUIElement *e = gui::StaticText::add(
				Environment, wstrgettext("Passwords do not match!"), rect,
				false, true, this, ID_message);
		e->setVisible(false);
	}
}

void GUIPasswordChange::drawMenu()
{
	gui::IGUISkin *skin = Environment->getSkin();
	if (!skin)
		return;
	video::IVideoDriver *driver = Environment->getVideoDriver();

	video::SColor bgcolor(140, 0, 0, 0);
	driver->draw2DRectangle(bgcolor, AbsoluteRect, &AbsoluteClippingRect);

	gui::IGUIElement::draw();
#ifdef __ANDROID__
	getAndroidUIInput();
#endif
}

void GUIPasswordChange::acceptInput()
{
	gui::IGUIElement *e;
	e = getElementFromId(ID_oldPassword);
	if (e != NULL)
		m_oldpass = e->getText();
	e = getElementFromId(ID_newPassword1);
	if (e != NULL)
		m_newpass = e->getText();
	e = getElementFromId(ID_newPassword2);
	if (e != NULL)
		m_newpass_confirm = e->getText();
}

bool GUIPasswordChange::processInput()
{
	if (m_newpass != m_newpass_confirm) {
		gui::IGUIElement *e = getElementFromId(ID_message);
		if (e != NULL)
			e->setVisible(true);
		return false;
	}
	m_client->sendChangePassword(wide_to_utf8(m_oldpass), wide_to_utf8(m_newpass));
	return true;
}

void GUIPasswordChange::generateRandomPassword()
{
	static const char chars[] =
			"abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";
	unsigned char bytes[96];
	if (!porting::secure_rand_fill_buf(bytes, sizeof(bytes)))
		return;

	std::string password;
	password.reserve(sizeof(bytes));
	for (unsigned char byte : bytes)
		password += chars[byte % (sizeof(chars) - 1)];

	m_newpass = utf8_to_wide(password);
	m_newpass_confirm = m_newpass;
	gui::IGUIElement *newpass = getElementFromId(ID_newPassword1);
	gui::IGUIElement *confirm = getElementFromId(ID_newPassword2);
	if (newpass)
		newpass->setText(m_newpass.c_str());
	if (confirm)
		confirm->setText(m_newpass_confirm.c_str());
}

bool GUIPasswordChange::OnEvent(const SEvent &event)
{
	if (event.EventType == EET_KEY_INPUT_EVENT) {
		if (event.KeyInput.Key == KEY_ESCAPE && event.KeyInput.PressedDown) {
			quitMenu();
			return true;
		}
		if (event.KeyInput.Key == KEY_RETURN && event.KeyInput.PressedDown) {
			acceptInput();
			if (processInput())
				quitMenu();
			return true;
		}
	}
	if (event.EventType == EET_GUI_EVENT) {
		if (event.GUIEvent.EventType == gui::EGET_ELEMENT_FOCUS_LOST &&
				isVisible()) {
			if (!canTakeFocus(event.GUIEvent.Element)) {
				infostream << "GUIPasswordChange: Not allowing focus change."
					<< std::endl;
				// Returning true disables focus change
				return true;
			}
		}
		if (event.GUIEvent.EventType == gui::EGET_BUTTON_CLICKED) {
			switch (event.GUIEvent.Caller->getID()) {
			case ID_change:
				acceptInput();
				if (processInput())
					quitMenu();
				return true;
			case ID_cancel:
				quitMenu();
				return true;
			case ID_random:
				generateRandomPassword();
				return true;
			}
		}
		if (event.GUIEvent.EventType == gui::EGET_EDITBOX_ENTER) {
			switch (event.GUIEvent.Caller->getID()) {
			case ID_oldPassword:
			case ID_newPassword1:
			case ID_newPassword2:
				acceptInput();
				if (processInput())
					quitMenu();
				return true;
			}
		}
	}

	return Parent ? Parent->OnEvent(event) : false;
}

std::string GUIPasswordChange::getNameByID(s32 id)
{
	switch (id) {
	case ID_oldPassword:
		return "old_password";
	case ID_newPassword1:
		return "new_password_1";
	case ID_newPassword2:
		return "new_password_2";
	}
	return "";
}

#ifdef __ANDROID__
void GUIPasswordChange::getAndroidUIInput()
{
	porting::AndroidDialogState dialogState = getAndroidUIInputState();
	if (dialogState == porting::DIALOG_SHOWN) {
		return;
	} else if (dialogState == porting::DIALOG_CANCELED) {
		m_jni_field_name.clear();
		return;
	}

	// It has to be a text input
	if (porting::getLastInputDialogType() != porting::TEXT_INPUT)
		return;

	gui::IGUIElement *e = nullptr;
	if (m_jni_field_name == "old_password")
		e = getElementFromId(ID_oldPassword);
	else if (m_jni_field_name == "new_password_1")
		e = getElementFromId(ID_newPassword1);
	else if (m_jni_field_name == "new_password_2")
		e = getElementFromId(ID_newPassword2);
	m_jni_field_name.clear();

	if (!e || e->getType() != irr::gui::EGUIET_EDIT_BOX)
		return;

	std::string text = porting::getInputDialogMessage();
	e->setText(utf8_to_wide(text).c_str());
	return;
}
#endif
