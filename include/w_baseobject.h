//
// Created by Metehan Gezer on 22.03.2023.
//

#ifndef WIESEL_W_BASEOBJECT_H
#define WIESEL_W_BASEOBJECT_H

#include "w_utils.h"
#include "w_logger.h"

class WieselObject {
public:
	WieselObject(const glm::vec3& position, const glm::quat& orientation);
	virtual ~WieselObject();

	const glm::vec3& getPosition();
	const glm::quat& getOrientation();
	const glm::mat4& getLocalView();

	virtual void move(float x, float y, float z);
	virtual void move(const glm::vec3& move);

	virtual void rotate(float radians, float ax, float ay, float az);
protected:

	virtual void updateView();

	glm::vec3 position;
	glm::quat orientation;
	glm::mat4 localView;
};

#endif //WIESEL_W_BASEOBJECT_H
