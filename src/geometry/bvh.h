#ifndef BVH_H
#define BVH_H

#include <algorithm>

#include "ray.h"
#include "rtweekend.h"
#include "vec3.h"

#include "hittable.h"
#include "hittable_list.h"

class bvh_node : public hittable {
  public:

    bvh_node(const hittable_list &list, double time0, double time1)
        : bvh_node(list.objects, 0, list.objects.size(), time0, time1) {
    }

    bvh_node(const std::vector<shared_ptr<hittable>> &src_objects, size_t start,
             size_t end, double time0, double time1);

    virtual bool hit(const ray &r, double t_min, double t_max,
                     hit_record &rec) const override;
    virtual bool bounding_box(double time0, double time1,
                              aabb &output_box) const override;

  public:
    shared_ptr<hittable> left;
    shared_ptr<hittable> right;
    aabb box;
};

bool bvh_node::bounding_box(double time0, double time1,
                            aabb &output_box) const {
    output_box = box;
    return true;
}

bool bvh_node::hit(const ray &r, double t_min, double t_max,
                   hit_record &rec) const {
    if (!box.hit(r, t_min, t_max)) {
        return false;
    }

    bool hit_left = left->hit(r, t_min, t_max, rec);
    bool hit_right = right->hit(r, t_min, hit_left ? rec.t : t_max, rec);

    return hit_left || hit_right;
}

bvh_node::bvh_node(const std::vector<shared_ptr<hittable>> &src_objects,
                   size_t start, size_t end, double time0, double time1) {
    auto objects = src_objects;

    aabb centroid_bounds;
    bool first_centroid = true;
    for (size_t i = start; i < end; ++i) {
        aabb box;
        if (!objects[i]->bounding_box(0, 0, box)) {
            std::cerr << "No bounding box in bvh_node constructor.\n";
            continue;
        }
        point3 centroid = 0.5 * (box.min() + box.max());
        aabb centroid_box(centroid, centroid);
        centroid_bounds = first_centroid
                              ? centroid_box
                              : surrounding_box(centroid_bounds, centroid_box);
        first_centroid = false;
    }

    vec3 extent = centroid_bounds.max() - centroid_bounds.min();
    int axis = 0;
    if (extent.y() > extent.x() && extent.y() > extent.z()) {
        axis = 1;
    } else if (extent.z() > extent.x()) {
        axis = 2;
    }

    auto comparator = [axis](const shared_ptr<hittable> &a,
                             const shared_ptr<hittable> &b) {
        aabb box_a, box_b;
        if (!a->bounding_box(0, 0, box_a) || !b->bounding_box(0, 0, box_b)) {
            std::cerr << "No bounding box in bvh_node constructor.\n";
        }
        point3 centroid_a = 0.5 * (box_a.min() + box_a.max());
        point3 centroid_b = 0.5 * (box_b.min() + box_b.max());
        return centroid_a[axis] < centroid_b[axis];
    };

    size_t object_span = end - start;

    if (object_span == 1) {
        left = right = objects[start];
    } else if (object_span == 2) {
        if (comparator(objects[start], objects[start + 1])) {
            left = objects[start];
            right = objects[start + 1];
        } else {
            left = objects[start + 1];
            right = objects[start];
        }
    } else {
        std::sort(objects.begin() + start, objects.begin() + end, comparator);

        auto mid = start + object_span / 2;
        left = make_shared<bvh_node>(objects, start, mid, time0, time1);
        right = make_shared<bvh_node>(objects, mid, end, time0, time1);
    }

    aabb box_left, box_right;

    if (!left->bounding_box(time0, time1, box_left) ||
        !right->bounding_box(time0, time1, box_right)) {
        std::cerr << "No bounding box in bvh_node constructor.\n";
    }

    box = surrounding_box(box_left, box_right);
}

#endif