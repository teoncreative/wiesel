//
// Created by Metehan Gezer on 22.03.2023.
//

#include "w_baseobject.h"

WieselObject::WieselObject(const glm::vec3& position, const glm::quat& orientation) : position(position), orientation(orientation) {

}

WieselObject::~WieselObject() {

}

const glm::vec3& WieselObject::getPosition() {
	return position;
}

const glm::quat& WieselObject::getOrientation() {
	return orientation;
}

void WieselObject::rotate(float radians, float ax, float ay, float az) {
	orientation *= glm::angleAxis(radians, glm::vec3(ax, ay, az));
}

void WieselObject::move(float x, float y, float z) {
	this->move(glm::vec3(x, y, z));
}

void WieselObject::move(const glm::vec3& move) {
	position += move;
}



