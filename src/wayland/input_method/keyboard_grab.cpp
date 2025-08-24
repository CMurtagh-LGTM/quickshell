#include "keyboard_grab.hpp"

#include <qdebug.h>
#include <qtmetamacros.h>
#include <sys/mman.h>
#include <wayland-input-method-unstable-v2-client-protocol.h>
#include <xkbcommon/xkbcommon.h>

#include "manager.hpp"
#include "virtual_keyboard.hpp"

namespace qs::wayland::input_method::impl {

InputMethodKeyboardGrab::InputMethodKeyboardGrab(
    QObject* parent,
    ::zwp_input_method_keyboard_grab_v2* keyboard
)
    : QObject(parent)
    , zwp_input_method_keyboard_grab_v2(keyboard) {}

InputMethodKeyboardGrab::~InputMethodKeyboardGrab() {
	this->release();

	// Release forward the pressed keys to the text input
	for (xkb_keycode_t key = 0; key < this->mKeyState.size(); ++key) {
		if (this->mKeyState[key] == KeyState::PRESSED) {
			this->mVirturalKeyboard->sendKey(
			    key + this->mKeyMapState.minKeycode(),
			    WL_KEYBOARD_KEY_STATE_PRESSED
			);
		}
	}
}

void InputMethodKeyboardGrab::zwp_input_method_keyboard_grab_v2_keymap(
    uint32_t format [[maybe_unused]],
    int32_t fd,
    uint32_t size
) {
	// https://wayland-book.com/seat/example.html
	assert(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);

	char* mapShm = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
	assert(mapShm != MAP_FAILED);

	this->mKeyMapState = KeyMapState(mapShm);

	munmap(mapShm, size);
	close(fd);

	this->mVirturalKeyboard =
	    VirtualKeyboardManager::instance()->createVirtualKeyboard(this->mKeyMapState);
	this->mKeyState = std::vector<KeyState>(
	    this->mKeyMapState.maxKeycode() - this->mKeyMapState.minKeycode() - 8,
	    KeyState::RELEASED
	);

	// Tell the text input to release all the keys
	for (xkb_keycode_t key = 0; key < this->mKeyState.size(); ++key) {
		this->mVirturalKeyboard->sendKey(
		    key + this->mKeyMapState.minKeycode(),
		    WL_KEYBOARD_KEY_STATE_RELEASED
		);
	}
}

void InputMethodKeyboardGrab::zwp_input_method_keyboard_grab_v2_key(
    uint32_t serial,
    uint32_t /*time*/, // TODO key repeat
    uint32_t key,
    uint32_t state
) {
	if (serial <= this->mSerial) return;
	this->mSerial = serial;

	key += 8;

#if 0
	qDebug() << KeyMapState::keyStateName(static_cast<wl_keyboard_key_state>(state))
	        << this->mKeyMapState.keyName(key) << "[" << key << "]"
	        << this->mKeyMapState.getChar(key);
#endif

	xkb_keysym_t sym = this->mKeyMapState.getOneSym(key);

	if (sym == XKB_KEY_Escape) {
		if (state == WL_KEYBOARD_KEY_STATE_PRESSED) emit escapePress();
		return;
	}
	if (sym == XKB_KEY_Return) {
		if (state == WL_KEYBOARD_KEY_STATE_PRESSED) emit returnPress();
		return;
	}

	// Skip adding the control keys because we've consumed them
	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		this->mKeyState[key - this->mKeyMapState.minKeycode()] = KeyState::PRESSED;
	} else if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
		this->mKeyState[key - this->mKeyMapState.minKeycode()] = KeyState::RELEASED;
	}

	if (sym == XKB_KEY_Up) {
		if (state == WL_KEYBOARD_KEY_STATE_PRESSED) emit directionPress(DirectionKey::UP);
		return;
	}
	if (sym == XKB_KEY_Down) {
		if (state == WL_KEYBOARD_KEY_STATE_PRESSED) emit directionPress(DirectionKey::DOWN);
		return;
	}
	if (sym == XKB_KEY_Left) {
		if (state == WL_KEYBOARD_KEY_STATE_PRESSED) emit directionPress(DirectionKey::LEFT);
		return;
	}
	if (sym == XKB_KEY_Right) {
		if (state == WL_KEYBOARD_KEY_STATE_PRESSED) emit directionPress(DirectionKey::RIGHT);
		return;
	}
	if (sym == XKB_KEY_BackSpace) {
		if (state == WL_KEYBOARD_KEY_STATE_PRESSED) emit backspacePress();
		return;
	}
	if (sym == XKB_KEY_Delete) {
		if (state == WL_KEYBOARD_KEY_STATE_PRESSED) emit deletePress();
		return;
	}

	QChar character = this->mKeyMapState.getChar(key);
	if (character != '\0') {
		if (state == WL_KEYBOARD_KEY_STATE_PRESSED) emit keyPress(character);
		return;
	} else {
		this->mVirturalKeyboard->sendKey(key, static_cast<wl_keyboard_key_state>(state));
	}
}

void InputMethodKeyboardGrab::zwp_input_method_keyboard_grab_v2_modifiers(
    uint32_t serial,
    uint32_t modsDepressed,
    uint32_t modsLatched,
    uint32_t modsLocked,
    uint32_t group
) {
	if (serial <= this->mSerial) return;
	this->mSerial = serial;
	this->mKeyMapState.setModifiers(
	    KeyMapState::ModifierState(modsDepressed, modsLatched, modsLocked, group, group, group)
	);
	this->mVirturalKeyboard->sendModifiers();
}

void InputMethodKeyboardGrab::zwp_input_method_keyboard_grab_v2_repeat_info(
    int32_t rate,
    int32_t delay
) {
	this->mRate = rate;
	this->mDelay = delay;
}

} // namespace qs::wayland::input_method::impl
