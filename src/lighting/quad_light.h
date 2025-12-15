#ifndef QUAD_LIGHT_H
#define QUAD_LIGHT_H

#include "light.h"
#include "rtweekend.h"

class QuadLight : public Light {
  public:
    QuadLight(const point3 &_Q, const vec3 &_u, const vec3 &_v, const color &_c)
        : Q(_Q), u(_u), v(_v), intensity(_c) {
        vec3 n = cross(u, v);
        area = n.length();
        normal = unit_vector(n);
        // Note: for downward-facing light, normal should be (0, -1, 0)
        // If cross(u, v) gives (0, 1, 0), negate it
    }

    virtual LightSample sample(const point3 &p,
                               const vec2 &random_u) const override {
        LightSample s;
        // Sample point on the rectangle
        point3 light_point = Q + random_u.x() * u + random_u.y() * v;

        vec3 d = light_point - p;
        double dist_sq = d.length_squared();
        s.dist = sqrt(dist_sq);
        s.wi = d / s.dist;
        s.is_delta = false;

        // Assuming single-sided light (emits in direction of normal)
        double cos_theta = dot(-s.wi, normal);

        if (cos_theta <= 0) {
            s.Li = color(0, 0, 0);
            s.pdf = 0;
            return s;
        }

        s.Li = intensity;

        // Convert area PDF to solid angle PDF
        // pdf_area = 1.0 / area
        // pdf_solid = pdf_area * dist^2 / cos_theta
        s.pdf = dist_sq / (area * cos_theta);

        return s;
    }

    virtual double pdf(const point3 &origin,
                       const vec3 &direction) const override {
        // Ray-plane intersection
        double denom = dot(direction, normal);

        // Only allow hitting from front (denom < 0)
        if (denom >= -1e-6)
            return 0;

        double t = dot(Q - origin, normal) / denom;
        if (t < 0.001 || t > infinity)
            return 0;

        point3 intersection = origin + t * direction;
        vec3 planar_hit_pt_vector = intersection - Q;

        // Assuming orthogonal u, v
        double alpha = dot(planar_hit_pt_vector, u) / u.length_squared();
        double beta = dot(planar_hit_pt_vector, v) / v.length_squared();

        if (alpha < 0 || alpha > 1 || beta < 0 || beta > 1) {
            return 0;
        }

        double dist_sq = t * t * direction.length_squared();
        double cos_theta = -denom / direction.length();

        return dist_sq / (area * cos_theta);
    }

    virtual bool is_delta() const override {
        return false;
    }
    virtual bool is_infinite() const override {
        return false;
    }

  private:
    point3 Q;
    vec3 u, v;
    color intensity;
    vec3 normal;
    double area;
};

#endif
