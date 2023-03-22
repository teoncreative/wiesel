//
// Created by Metehan Gezer on 22.03.2023.
//

#ifndef WIESEL_W_CAMERA_H
#define WIESEL_W_CAMERA_H

#include "w_baseobject.h"

class WieselCamera : public WieselObject {
public:
	WieselCamera(const glm::vec3& position, const glm::quat& orientation, float aspectRatio, float fieldOfView = 45, float nearPlane = 0.1f, float farPlane = 1000.0f);
	~WieselCamera();

	const glm::mat4& getView();
	const glm::mat4& getProjection();

	float getFieldOfView() const;

	void move(float x, float y, float z);
	void move(const glm::vec3& move);

	void rotate(float radians, float ax, float ay, float az);

private:
	void updateProjection();

	float fieldOfView;
	float nearPlane;
	float farPlane;
	float aspectRatio;
	glm::mat4 projection;
	glm::mat4 view;


};

#endif //WIESEL_W_CAMERA_H
