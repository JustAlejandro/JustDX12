#include "KeyboardWrapper.h"
#include <WinUser.h>

void KeyboardWrapper::registerKey(int val) {
	if (keys.find(val) != keys.end()) {
		return;
	}
	keys.insert(std::make_pair(val, Key(0,0)));
}

void KeyboardWrapper::update() {
	for (auto& key : keys) {
		key.second.prevStatus = key.second.curStatus;
		key.second.curStatus = GetAsyncKeyState(key.first);
	}
}

KEY_STATUS KeyboardWrapper::getKeyStatus(int val) {
	auto keyFind = keys.find(val);
	if (keyFind == keys.end()) {
		registerKey(val);
		keyFind = keys.find(val);
	}
	Key& key = keyFind->second;
	KEY_STATUS status = KEY_STATUS_NONE;
	// 0x8000 = pressed
	if (key.curStatus & 0x8000) {
		status |= KEY_STATUS_PRESSED;
		if ((key.prevStatus & 0x8000) == 0) {
			status |= KEY_STATUS_JUST_PRESSED;
		}
	}
	else {
		status |= KEY_STATUS_RELEASED;
		if ((key.prevStatus & 0x8000) != 0) {
			status |= KEY_STATUS_JUST_RELEASED;
		}
	}
	return status;
}



