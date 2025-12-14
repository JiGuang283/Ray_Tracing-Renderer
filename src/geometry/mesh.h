#ifndef MESH_H
#define MESH_H

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "bvh.h"
#include "hittable.h"
#include "material.h"
#include "tiny_obj_loader.h"
#include "triangle.h"

class mesh : public hittable {
  public:
    mesh() = default;

    mesh(std::vector<shared_ptr<hittable>> faces, double time0 = 0.0,
         double time1 = 1.0)
        : triangles(std::move(faces)),
          accelerator(make_shared<bvh_node>(triangles, 0, triangles.size(),
                                             time0, time1)) {
    }

    static shared_ptr<mesh>
    load_from_obj(const std::string &filename, shared_ptr<material> mat,
                  const vec3 &translation = vec3(0, 0, 0),
                  const vec3 &scale = vec3(1, 1, 1));

    bool hit(const ray &r, double t_min, double t_max,
             hit_record &rec) const override {
        return accelerator && accelerator->hit(r, t_min, t_max, rec);
    }

    bool bounding_box(double time0, double time1,
                      aabb &output_box) const override {
        if (!accelerator) {
            return false;
        }
        return accelerator->bounding_box(time0, time1, output_box);
    }

  private:
    std::vector<shared_ptr<hittable>> triangles;
    shared_ptr<hittable> accelerator;
};

inline shared_ptr<mesh>
mesh::load_from_obj(const std::string &filename, shared_ptr<material> mat,
                    const vec3 &translation, const vec3 &scale) {
    tinyobj::ObjReaderConfig reader_config;
    reader_config.triangulate = true;

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(filename, reader_config)) {
        std::cerr << "[TinyObjLoader] " << reader.Error() << std::endl;
        return nullptr;
    }

    const auto &attrib = reader.GetAttrib();
    const auto &shapes = reader.GetShapes();

    std::vector<shared_ptr<hittable>> faces;
    faces.reserve(shapes.size() * 3);

    for (const auto &shape : shapes) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            int fv = shape.mesh.num_face_vertices[f];
            if (fv != 3) {
                index_offset += fv;
                continue;
            }

            tinyobj::index_t idx0 = shape.mesh.indices[index_offset + 0];
            tinyobj::index_t idx1 = shape.mesh.indices[index_offset + 1];
            tinyobj::index_t idx2 = shape.mesh.indices[index_offset + 2];
            index_offset += 3;

            if (idx0.vertex_index < 0 || idx1.vertex_index < 0 ||
                idx2.vertex_index < 0) {
                continue;
            }

            auto fetch_vertex = [&](const tinyobj::index_t &idx) {
                if (idx.vertex_index < 0 ||
                    attrib.vertices.size() <=
                        static_cast<size_t>(3 * idx.vertex_index + 2)) {
                    return point3(0, 0, 0);
                }
                size_t v_base = static_cast<size_t>(3 * idx.vertex_index);
                point3 p(attrib.vertices[v_base + 0], attrib.vertices[v_base + 1],
                         attrib.vertices[v_base + 2]);
                p[0] = p.x() * scale.x() + translation.x();
                p[1] = p.y() * scale.y() + translation.y();
                p[2] = p.z() * scale.z() + translation.z();
                return p;
            };

            auto fetch_normal = [&](const tinyobj::index_t &idx) {
                if (idx.normal_index < 0 ||
                    attrib.normals.size() <=
                        static_cast<size_t>(3 * idx.normal_index + 2)) {
                    return vec3(0, 0, 0);
                }
                size_t n_base = static_cast<size_t>(3 * idx.normal_index);
                return vec3(attrib.normals[n_base + 0], attrib.normals[n_base + 1],
                            attrib.normals[n_base + 2]);
            };

            point3 p0 = fetch_vertex(idx0);
            point3 p1 = fetch_vertex(idx1);
            point3 p2 = fetch_vertex(idx2);

            vec3 n0 = fetch_normal(idx0);
            vec3 n1 = fetch_normal(idx1);
            vec3 n2 = fetch_normal(idx2);

            if (n0.length_squared() > 0 && n1.length_squared() > 0 &&
                n2.length_squared() > 0) {
                faces.push_back(
                    make_shared<triangle>(p0, p1, p2, unit_vector(n0),
                                          unit_vector(n1), unit_vector(n2), mat));
            } else {
                faces.push_back(make_shared<triangle>(p0, p1, p2, mat));
            }
        }
    }

    if (faces.empty()) {
        std::cerr << "[TinyObjLoader] No valid triangles parsed from "
                  << filename << std::endl;
        return nullptr;
    }

    return make_shared<mesh>(faces, 0.0, 1.0);
}

#endif
