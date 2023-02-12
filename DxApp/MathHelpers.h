#pragma once

#include <cmath>
#include <algorithm>


struct Vector3
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	Vector3() = default;

	Vector3(float x, float y, float z) : x(x), y(y), z(z) { }

	Vector3(const Vector3& other)
	{
		x = other.x;
		y = other.y;
		z = other.z;
	}

	Vector3& operator=(const Vector3& other) = default;

	Vector3 operator+(const Vector3& other) const
	{
		return Vector3(x + other.x, y + other.y, z + other.z);
	}

	Vector3& operator+=(const Vector3& other)
	{
		x += other.x;
		y += other.y;
		z += other.z;
		return *this;
	}

	void Clamp01()
	{
		x = std::clamp(x, 0.0f, 1.0f);
		y = std::clamp(y, 0.0f, 1.0f);
		z = std::clamp(z, 0.0f, 1.0f);
	}
};

