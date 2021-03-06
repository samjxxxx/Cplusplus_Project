#pragma once
#include "ray.h"

class material;

struct hit_record {
	float t;
	vec3 p;
	vec3 normal;
	material* mat_ptr;
	bool font_face;

	inline void set_face_normal(const ray& r, const vec3& outward_normal)
	{
		font_face = dot(r.direction(), outward_normal);
		normal = font_face ? outward_normal : -outward_normal;
	}
};

class hitable {
public:
	virtual bool hit(const ray& r, float t_min, float t_max, hit_record& rec) const = 0;
};