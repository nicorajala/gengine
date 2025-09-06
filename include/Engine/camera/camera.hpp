#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "math/math.hpp"
using namespace NMATH;

class Camera {
public:
	Vec3d position;
	float yaw;
	float pitch;
	float roll;

	float fovDeg;
	float nearP;
	float farP;

	Camera()
		: position(0.f, 0.f, 0.f), yaw(0.f), pitch(0.f), roll(0.f),
		fovDeg(60.f), nearP(0.1f), farP(1000.f) {}

	// Get the forward direction vector from yaw and pitch
	Vec3d forward() const;

	Mat4 getViewMatrix() const;

	// Get the perspective projection matrix
	Mat4 getProjectionMatrix(float aspect) const;
};

#endif