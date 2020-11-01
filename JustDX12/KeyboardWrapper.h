#pragma once
#include <unordered_map>
#include <wrl.h>

typedef enum KEY_STATUS {
	KEY_STATUS_NONE = 0,
	KEY_STATUS_PRESSED = 1,
	KEY_STATUS_RELEASED = 2,
	KEY_STATUS_JUST_PRESSED = 4,
	KEY_STATUS_JUST_RELEASED = 8
} KEY_STATUS;
DEFINE_ENUM_FLAG_OPERATORS(KEY_STATUS);

struct Key {
	Key(SHORT prev, SHORT cur) {
		prevStatus = prev;
		curStatus = cur;
	}
	SHORT prevStatus;
	SHORT curStatus;
};

class KeyboardWrapper {
public:
	void registerKey(int val);
	void update();
	KEY_STATUS getKeyStatus(int val);
private:
	std::unordered_map<int, Key> keys;
};

