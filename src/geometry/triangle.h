#ifndef TRIANGLE_H
#define TRIANGLE_H

#include <cmath>

#include "hittable.h"
#include "material.h"
#include "vec3.h"

class triangle : public hittable {
  public:
    triangle(const point3 &p0, const point3 &p1, const point3 &p2,
             shared_ptr<material> m)
        : v0(p0), v1(p1), v2(p2), mat_ptr(std::move(m)) {
        edge1 = v1 - v0;
        edge2 = v2 - v0;
        face_normal = unit_vector(cross(edge1, edge2));
    }

    triangle(const point3 &p0, const point3 &p1, const point3 &p2,
             const vec3 &n0, const vec3 &n1, const vec3 &n2,
             shared_ptr<material> m)
        : v0(p0),
          v1(p1),
          v2(p2),
          n0(n0),
          n1(n1),
          n2(n2),
          mat_ptr(std::move(m)),
          has_vertex_normals(true) {
        edge1 = v1 - v0;
        edge2 = v2 - v0;
        face_normal = unit_vector(cross(edge1, edge2));
    }

    bool hit(const ray &r, double t_min, double t_max,
             hit_record &rec) const override {
        const double eps = 1e-8;
        vec3 pvec = cross(r.direction(), edge2);
        double det = dot(edge1, pvec);

        if (fabs(det) < eps) {
            return false;
        }

        double inv_det = 1.0 / det;
        vec3 tvec = r.origin() - v0;
        double u = dot(tvec, pvec) * inv_det;
        if (u < 0.0 || u > 1.0) {
            return false;
        }

        vec3 qvec = cross(tvec, edge1);
        double v = dot(r.direction(), qvec) * inv_det;
        if (v < 0.0 || u + v > 1.0) {
            return false;
        }

        double t = dot(edge2, qvec) * inv_det;
        if (t < t_min || t > t_max) {
            return false;
        }

        rec.t = t;
        rec.p = r.at(t);
        rec.u = u;
        rec.v = v;
        rec.mat_ptr = mat_ptr.get();

        vec3 shading_normal = face_normal;
        if (has_vertex_normals) {
            shading_normal = unit_vector((1.0 - u - v) * n0 + u * n1 + v * n2);
        }

        rec.set_face_normal(r, shading_normal);
        return true;
    }

    bool bounding_box(double /*time0*/, double /*time1*/,
                      aabb &output_box) const override {
        double min_x = fmin(v0.x(), fmin(v1.x(), v2.x()));
        double min_y = fmin(v0.y(), fmin(v1.y(), v2.y()));
        double min_z = fmin(v0.z(), fmin(v1.z(), v2.z()));
        double max_x = fmax(v0.x(), fmax(v1.x(), v2.x()));
        double max_y = fmax(v0.y(), fmax(v1.y(), v2.y()));
        double max_z = fmax(v0.z(), fmax(v1.z(), v2.z()));

        const double padding = 1e-4;
        output_box = aabb(point3(min_x - padding, min_y - padding,
                                 min_z - padding),
                          point3(max_x + padding, max_y + padding,
                                 max_z + padding));
        return true;
    }

  private:
    point3 v0;
    point3 v1;
    point3 v2;
    vec3 edge1;
    vec3 edge2;
    vec3 face_normal;

    vec3 n0{0, 0, 0};
    vec3 n1{0, 0, 0};
    vec3 n2{0, 0, 0};
    bool has_vertex_normals = false;

    shared_ptr<material> mat_ptr;
};

#endif
