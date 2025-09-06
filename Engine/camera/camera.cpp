#include "Engine/camera/camera.hpp"

Vec3d Camera::forward() const {
	float cy = NMATH::cos(radians(yaw));
	float sy = NMATH::sin(radians(yaw));
	float cp = NMATH::cos(radians(pitch));
	float sp = NMATH::sin(radians(pitch));

	Vec3d f;
	f.x = cp * cy;
	f.y = sp;
	f.z = cp * sy;
	return f.normalized();
}

Mat4 Camera::getViewMatrix() const {
	Vec3d eye = position;
	Vec3d f = forward();
	Vec3d up = Vec3d(0.f, 1.f, 0.f);

	Vec3d s = f.cross(up).normalized();
	Vec3d u = s.cross(f);

	Mat4 result(0.f);

	result.m[0][0] = s.x; result.m[0][1] = u.x; result.m[0][2] = -f.x; result.m[0][3] = 0.0f;
	result.m[1][0] = s.y; result.m[1][1] = u.y; result.m[1][2] = -f.y; result.m[1][3] = 0.0f;
	result.m[2][0] = s.z; result.m[2][1] = u.z; result.m[2][2] = -f.z; result.m[2][3] = 0.0f;
	result.m[3][0] = -s.dot(eye); result.m[3][1] = -u.dot(eye); result.m[3][2] = f.dot(eye); result.m[3][3] = 1.0f;

	return result;
}

Mat4 Camera::getProjectionMatrix(float aspect) const {
	if (aspect <= 0.0f) aspect = 1.0f;
	return perspective(radians(fovDeg), aspect, nearP, farP);
}