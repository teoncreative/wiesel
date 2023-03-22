//
// Created by Metehan Gezer on 22.03.2023.
//

#ifndef WIESEL_W_KEYMANAGER_H
#define WIESEL_W_KEYMANAGER_H

#include <map>

struct KeyData {
	KeyData();
	KeyData(bool pressed);

	bool pressed;
};

class WieselKeyManager {
public:
	WieselKeyManager();
	~WieselKeyManager();

	void set(int key, bool pressed);
	bool isPressed(int key);
private:
	std::map<int, KeyData> keys;
};

#endif //WIESEL_W_KEYMANAGER_H
