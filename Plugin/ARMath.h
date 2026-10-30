#pragma once

#define _USE_MATH_DEFINES // For MSVC
#include <math.h>

float Lerp(float A, float B, float t)
{
	float Result = (1.0f - t) * A + t * B;
	return Result;
}

inline float deg_to_rad(float deg) {
	return deg * (M_PI / 180.0f);
}

inline float rad_to_deg(float rad) {
	return rad * (180.0f / M_PI);
}

struct Vec3 {
	float x, y, z;
};

inline Vec3 get_vec3(float x, float y, float z) {
	Vec3 v;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

inline Vec3 operator+(Vec3 a, Vec3 b) {
	return get_vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

inline Vec3 operator-(Vec3 a, Vec3 b) {
	return get_vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

inline Vec3 operator-(Vec3 v) {
	return get_vec3(-v.x, -v.y, -v.z);
}

inline Vec3 operator*(Vec3 a, Vec3 b) {
	return get_vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}

inline Vec3 operator*(Vec3 v, float s) {
	return get_vec3(v.x * s, v.y * s, v.z * s);
}

inline Vec3 operator/(Vec3 a, Vec3 b) {
	return get_vec3(a.x / b.x, a.y / b.y, a.z / b.z);
}

inline Vec3 operator/(Vec3 v, float s) {
	return get_vec3(v.x / s, v.y / s, v.z / s);
}

inline Vec3 &operator+=(Vec3 &a, Vec3 b) {
	a = a + b;
	return a;
}

inline Vec3 &operator-=(Vec3 &a, Vec3 b) {
	a = a - b;
	return a;
}

inline Vec3 &operator*=(Vec3 &a, Vec3 b) {
	a = a * b;
	return a;
}

inline Vec3 normalize_vec3(Vec3 v) {
	Vec3 norm = {};

	float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
	if (len > 0.0f) {
		norm = v / len;
	}

	return norm;
}

inline Vec3 cross_vec3(Vec3 a, Vec3 b) {
	return get_vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

inline float dot_vec3(Vec3 a, Vec3 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

struct Quaternion {
	float x, y, z, w;
};

// Row major
struct Mat4 {
	float e[4][4];
};

inline Mat4 get_identity_mat4() {
	Mat4 m;
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 4; ++c) {
			if (c == r) {
				m.e[r][c] = 1.0f;
			} else {
				m.e[r][c] = 0.0f;
			}
		}
	}
	return m;
}

inline Mat4 get_look_at_mat4(Vec3 pos, Vec3 target, Vec3 up) {
	Vec3 d = normalize_vec3(target - pos);
	Vec3 r = normalize_vec3(cross_vec3(up, d));
	Vec3 u = cross_vec3(d, r);

	Mat4 m = {};

	m.e[0][0] = r.x;
	m.e[0][1] = r.y;
	m.e[0][2] = r.z;
	m.e[0][3] = -dot_vec3(pos, r);

	m.e[1][0] = u.x;
	m.e[1][1] = u.y;
	m.e[1][2] = u.z;
	m.e[1][3] = -dot_vec3(pos, u);

	m.e[2][0] = d.x;
	m.e[2][1] = d.y;
	m.e[2][2] = d.z;
	m.e[2][3] = -dot_vec3(pos, d);

	m.e[3][3] = 1.0f;

	return m;
}

inline Mat4 get_perspective_mat4(float vert_fov, float asp_ratio, float zn, float zf) {
	float tan_half_vert_fov = tanf(vert_fov * 0.5f);

	float h = zn * tan_half_vert_fov;
	float w = asp_ratio * h;

	Mat4 m = {};

	m.e[0][0] = (2.0f * zn) / w;
	m.e[1][1] = (2.0f * zn) / h;
	m.e[2][2] = zf / (zf - zn);
	m.e[2][3] = (-zf * zn) / (zf - zn);
	m.e[3][2] = 1.0f;

	return m;
}

inline Mat4 operator*(Mat4 a, Mat4 b) {
	Mat4 m;
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 4; ++c) {
			m.e[r][c] = a.e[r][0] * b.e[0][c] + a.e[r][1] * b.e[1][c] + a.e[r][2] * b.e[2][c] + a.e[r][3] * b.e[3][c];
		}
	}
	return m;
}

inline Mat4 get_translation_mat4(Vec3 v) {
	Mat4 m = get_identity_mat4();
	m.e[0][3] = v.x;
	m.e[1][3] = v.y;
	m.e[2][3] = v.z;
	return m;
}

inline Mat4 get_scale_mat4(float s) {
	Mat4 m = get_identity_mat4();
	m.e[0][0] = s;
	m.e[1][1] = s;
	m.e[2][2] = s;
	return m;
}

inline Quaternion get_quaternion(Vec3 axis, float angle) {
	float sin_half_angle = sinf(angle * 0.5f);

	Quaternion q;
	q.x = sin_half_angle * axis.x;
	q.y = sin_half_angle * axis.y;
	q.z = sin_half_angle * axis.z;
	q.w = cosf(angle * 0.5f);

	return q;
}

inline Mat4 get_rotation_mat4(Quaternion q) {
	// Conjugate of the quaternion q
	Quaternion c;
	c.x = -q.x;
	c.y = -q.y;
	c.z = -q.z;
	c.w =  q.w;

	// TODO: See if this can be done by multiplying the quaternions
	Mat4 m1 = {
		 q.w, -q.z,  q.y, 0.0f,
		 q.z,  q.w, -q.x, 0.0f,
		-q.y,  q.x,  q.w, 0.0f,
		-q.x, -q.y, -q.z, 0.0f,
	};

	Mat4 m2 = {
		 c.w,  c.z, -c.y,  c.x,
		-c.z,  c.w,  c.x,  c.y,
		 c.y, -c.x,  c.w,  c.z,
		0.0f, 0.0f, 0.0f, 0.0f,
	};

	Mat4 m = m2 * m1;
	m.e[3][3] = 1.0f;

	return m;
}

inline Quaternion operator*(Quaternion a, Quaternion b) {
	Quaternion q;
	q.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
	q.y = a.w * b.y + a.y * b.w + a.z * b.x - a.x * b.z;
	q.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
	q.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
	return q;
}

inline Mat4 get_transpose_mat4(Mat4 m) {
	Mat4 transpose;
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 4; ++c) {
			transpose.e[r][c] = m.e[c][r];
		}
	}
	return transpose;
}
