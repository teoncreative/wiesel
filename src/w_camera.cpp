//
// Created by Metehan Gezer on 22.03.2023.
//
#include "w_camera.h"

WieselCamera::WieselCamera(const glm::vec3& position, const glm::quat& orientation, float aspectRatio, float fieldOfView, float nearPlane, float farPlane) : WieselObject(position, orientation), aspectRatio(aspectRatio), fieldOfView(fieldOfView), nearPlane(nearPlane), farPlane(farPlane) {
	updateProjection();
}

WieselCamera::~WieselCamera() {

}

const glm::mat4& WieselCamera::getProjection() {
	return projection;
}

float WieselCamera::getFieldOfView() const {
	return fieldOfView;
}

void WieselCamera::move(float x, float y, float z) {
	WieselObject::move(x, y, z);
	updateProjection();
}

void WieselCamera::move(const glm::vec3& move) {
	WieselObject::move(move);
	updateProjection();
}

void WieselCamera::rotate(float radians, float ax, float ay, float az) {
	WieselObject::rotate(radians, ax, ay, az);
	updateProjection();
}

void WieselCamera::updateProjection() {
	projection = glm::perspective(glm::radians(fieldOfView), aspectRatio, nearPlane, farPlane);
	projection[1][1] *= -1; // glm is originally designed for OpenGL, which Y coords where flipped
}



