#include "scenes.h"
#include "aarect.h"
#include "box.h"
#include "bvh.h"
#include "constant_medium.h"
#include "directional_light.h"
#include "environmental_light.h"
#include "hittable_list.h"
#include "material.h"
#include "moving_sphere.h"
#include "quad_light.h"
#include "sphere.h"
#include "spot_light.h"

shared_ptr<hittable> random_scene() {
    hittable_list world;

    auto checker = make_shared<checker_texture>(color(0.2, 0.3, 0.1),
                                                color(0.9, 0.9, 0.9));
    world.add(make_shared<sphere>(point3(0, -1000, 0), 1000,
                                  make_shared<lambertian>(checker)));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            auto choose_mat = random_double();
            point3 center(a + 0.9 * random_double(), 0.2,
                          b + 0.9 * random_double());

            if ((center - point3(4, 0.2, 0)).length() > 0.9) {
                shared_ptr<material> sphere_material;

                if (choose_mat < 0.8) {
                    auto albedo = color::random() * color::random();
                    sphere_material = make_shared<lambertian>(albedo);
                    auto center2 = center + vec3(0, random_double(0, .5), 0);
                    world.add(make_shared<moving_sphere>(
                        center, center2, 0.0, 1.0, 0.2, sphere_material));
                } else if (choose_mat < 0.95) {
                    auto albedo = color::random(0.5, 1);
                    auto fuzz = random_double(0, 0.5);
                    sphere_material = make_shared<metal>(albedo, fuzz);
                    world.add(
                        make_shared<sphere>(center, 0.2, sphere_material));
                }
            }
        }
    }

    auto material1 = make_shared<dielectric>(1.5);
    world.add(make_shared<sphere>(point3(0, 1, 0), 1.0, material1));

    auto material2 = make_shared<lambertian>(color(0.4, 0.2, 0.1));
    world.add(make_shared<sphere>(point3(-4, 1, 0), 1.0, material2));

    auto material3 = make_shared<metal>(color(0.7, 0.6, 0.5), 0.0);
    world.add(make_shared<sphere>(point3(4, 1, 0), 1.0, material3));

    return make_shared<bvh_node>(world, 0, 1);
}

shared_ptr<hittable> example_light_scene() {
    hittable_list world;

    auto checker = make_shared<checker_texture>(color(0.2, 0.3, 0.1),
                                                color(0.9, 0.9, 0.9));
    world.add(make_shared<sphere>(point3(0, -1000, 0), 1000,
                                  make_shared<lambertian>(checker)));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            auto choose_mat = random_double();
            point3 center(a + 0.9 * random_double(), 0.2,
                          b + 0.9 * random_double());

            if ((center - point3(4, 0.2, 0)).length() > 0.9) {
                shared_ptr<material> sphere_material;

                if (choose_mat < 0.3) {
                    auto albedo = color::random() * color::random();
                    sphere_material = make_shared<lambertian>(albedo);
                    auto center2 = center + vec3(0, random_double(0, .5), 0);
                    world.add(
                        make_shared<sphere>(center, 0.2, sphere_material));
                } else if (choose_mat < 0.6) {
                    auto albedo = color::random(0.5, 1);
                    auto fuzz = random_double(0, 0.5);
                    sphere_material = make_shared<metal>(albedo, fuzz);
                    world.add(
                        make_shared<sphere>(center, 0.2, sphere_material));
                } else if (choose_mat < 0.95) {
                    auto difflight =
                        make_shared<diffuse_light>(color::random() * 2);
                    world.add(make_shared<sphere>(center, 0.2, difflight));
                }
            }
        }
    }

    auto material1 = make_shared<dielectric>(1.5);
    world.add(make_shared<sphere>(point3(0, 1, 0), 1.0, material1));

    auto material2 = make_shared<diffuse_light>(color(0.4, 0.2, 0.1) * 5);
    world.add(make_shared<sphere>(point3(-4, 1, 0), 1.0, material2));

    auto material3 = make_shared<metal>(color(0.7, 0.6, 0.5), 0.0);
    world.add(make_shared<sphere>(point3(4, 1, 0), 1.0, material3));

    return make_shared<bvh_node>(world, 0, 1);
}

shared_ptr<hittable> two_spheres() {
    hittable_list objects;

    auto checker = make_shared<checker_texture>(color(0.2, 0.3, 0.1),
                                                color(0.9, 0.9, 0.9));

    objects.add(make_shared<sphere>(point3(0, -10, 0), 10,
                                    make_shared<lambertian>(checker)));
    objects.add(make_shared<sphere>(point3(0, 10, 0), 10,
                                    make_shared<lambertian>(checker)));

    return make_shared<bvh_node>(objects, 0, 1);
}

shared_ptr<hittable> two_perlin_spheres() {
    hittable_list objects;

    auto pertext = make_shared<noise_texture>(4);
    objects.add(make_shared<sphere>(point3(0, -1000, 0), 1000,
                                    make_shared<lambertian>(pertext)));
    objects.add(make_shared<sphere>(point3(0, 2, 0), 2,
                                    make_shared<lambertian>(pertext)));

    return make_shared<bvh_node>(objects, 0, 1);
}

shared_ptr<hittable> earth() {
    auto earth_texture = make_shared<image_texture>("earthmap.jpg");
    auto earth_surface = make_shared<lambertian>(earth_texture);
    auto globe = make_shared<sphere>(point3(0, 0, 0), 2, earth_surface);

    return make_shared<bvh_node>(hittable_list(globe), 0, 1);
}

shared_ptr<hittable> simple_light() {
    hittable_list objects;

    auto pertext = make_shared<lambertian>(color(0.4, 0.6, 0.3));
    objects.add(make_shared<sphere>(point3(0, -1000, 0), 1000, pertext));
    objects.add(make_shared<sphere>(point3(0, 2, 0), 2, pertext));

    auto difflight = make_shared<diffuse_light>(color(4, 4, 4));
    objects.add(make_shared<xy_rect>(3, 5, 1, 3, -2, difflight));
    objects.add(make_shared<sphere>(vec3(0, 7, 0), 2, difflight));

    return make_shared<bvh_node>(objects, 0, 1);
}

shared_ptr<hittable> cornell_box() {
    hittable_list objects;

    auto red = make_shared<lambertian>(color(.65, .05, .05));
    auto white = make_shared<lambertian>(color(.73, .73, .73));
    auto green = make_shared<lambertian>(color(.12, .45, .15));
    auto light = make_shared<diffuse_light>(color(15, 15, 15));

    objects.add(make_shared<yz_rect>(0, 555, 0, 555, 555, green));
    objects.add(make_shared<yz_rect>(0, 555, 0, 555, 0, red));
    objects.add(make_shared<xz_rect>(213, 343, 227, 332, 554, light));
    objects.add(make_shared<xz_rect>(0, 555, 0, 555, 0, white));
    objects.add(make_shared<xz_rect>(0, 555, 0, 555, 555, white));
    objects.add(make_shared<xy_rect>(0, 555, 0, 555, 555, white));

    shared_ptr<hittable> box1 =
        make_shared<box>(point3(0, 0, 0), point3(165, 330, 165), white);
    box1 = make_shared<rotate_y>(box1, 15);
    box1 = make_shared<translate>(box1, vec3(265, 0, 295));
    objects.add(box1);

    shared_ptr<hittable> box2 =
        make_shared<box>(point3(0, 0, 0), point3(165, 165, 165), white);
    box2 = make_shared<rotate_y>(box2, -18);
    box2 = make_shared<translate>(box2, vec3(130, 0, 65));
    objects.add(box2);

    return make_shared<bvh_node>(objects, 0, 1);
}

shared_ptr<hittable> cornell_smoke() {
    hittable_list objects;

    auto red = make_shared<lambertian>(color(.65, .05, .05));
    auto white = make_shared<lambertian>(color(.73, .73, .73));
    auto green = make_shared<lambertian>(color(.12, .45, .15));
    auto light = make_shared<diffuse_light>(color(7, 7, 7));

    objects.add(make_shared<yz_rect>(0, 555, 0, 555, 555, green));
    objects.add(make_shared<yz_rect>(0, 555, 0, 555, 0, red));
    objects.add(make_shared<xz_rect>(113, 443, 127, 432, 554, light));
    objects.add(make_shared<xz_rect>(0, 555, 0, 555, 555, white));
    objects.add(make_shared<xz_rect>(0, 555, 0, 555, 0, white));
    objects.add(make_shared<xy_rect>(0, 555, 0, 555, 555, white));

    shared_ptr<hittable> box1 =
        make_shared<box>(point3(0, 0, 0), point3(165, 330, 165), white);
    box1 = make_shared<rotate_y>(box1, 15);
    box1 = make_shared<translate>(box1, vec3(265, 0, 295));
    box1 = make_shared<constant_medium>(box1, 0.01, color(0, 0, 0));
    objects.add(box1);

    shared_ptr<hittable> box2 =
        make_shared<box>(point3(0, 0, 0), point3(165, 165, 165), white);
    box2 = make_shared<rotate_y>(box2, -18);
    box2 = make_shared<translate>(box2, vec3(130, 0, 65));
    box2 = make_shared<constant_medium>(box2, 0.01, color(1, 1, 1));
    objects.add(box2);

    return make_shared<bvh_node>(objects, 0, 1);
}

shared_ptr<hittable> final_scene() {
    hittable_list boxes1;
    auto ground = make_shared<lambertian>(color(0.48, 0.83, 0.53));

    const int boxes_per_side = 20;
    for (int i = 0; i < boxes_per_side; i++) {
        for (int j = 0; j < boxes_per_side; j++) {
            auto w = 100.0;
            auto x0 = -1000.0 + i * w;
            auto z0 = -1000.0 + j * w;
            auto y0 = 0.0;
            auto x1 = x0 + w;
            auto y1 = random_double(1, 101);
            auto z1 = z0 + w;

            boxes1.add(make_shared<box>(point3(x0, y0, z0), point3(x1, y1, z1),
                                        ground));
        }
    }

    hittable_list objects;

    objects.add(make_shared<bvh_node>(boxes1, 0, 1));

    auto light = make_shared<diffuse_light>(color(7, 7, 7));
    objects.add(make_shared<xz_rect>(123, 423, 147, 412, 554, light));

    auto center1 = point3(400, 400, 200);
    auto center2 = center1 + vec3(30, 0, 0);
    auto moving_sphere_material = make_shared<lambertian>(color(0.7, 0.3, 0.1));
    objects.add(make_shared<moving_sphere>(center1, center2, 0, 1, 50,
                                           moving_sphere_material));

    objects.add(make_shared<sphere>(point3(260, 150, 45), 50,
                                    make_shared<dielectric>(1.5)));
    objects.add(
        make_shared<sphere>(point3(0, 150, 145), 50,
                            make_shared<metal>(color(0.8, 0.8, 0.9), 1.0)));

    auto boundary = make_shared<sphere>(point3(360, 150, 145), 70,
                                        make_shared<dielectric>(1.5));
    objects.add(boundary);
    objects.add(
        make_shared<constant_medium>(boundary, 0.2, color(0.2, 0.4, 0.9)));

    boundary = make_shared<sphere>(point3(0, 0, 0), 5000,
                                   make_shared<dielectric>(1.5));
    objects.add(make_shared<constant_medium>(boundary, .0001, color(1, 1, 1)));

    auto emat =
        make_shared<lambertian>(make_shared<image_texture>("earthmap.jpg"));
    objects.add(make_shared<sphere>(point3(400, 200, 400), 100, emat));

    auto pertext = make_shared<noise_texture>(0.1);
    objects.add(make_shared<sphere>(point3(220, 280, 300), 80,
                                    make_shared<lambertian>(pertext)));

    hittable_list boxes2;
    auto white = make_shared<lambertian>(color(.73, .73, .73));
    int ns = 1000;
    for (int j = 0; j < ns; j++) {
        boxes2.add(make_shared<sphere>(point3::random(0, 165), 10, white));
    }

    objects.add(make_shared<translate>(
        make_shared<rotate_y>(make_shared<bvh_node>(boxes2, 0.0, 1.0), 15),
        vec3(-100, 270, 395)));

    return make_shared<bvh_node>(objects, 0, 1);
}

shared_ptr<hittable> pbr_test_scene() {
    hittable_list world;

    auto checker = make_shared<checker_texture>(color(0.2, 0.3, 0.1),
                                                color(0.9, 0.9, 0.9));
    world.add(make_shared<sphere>(point3(0, -1000, 0), 1000,
                                  make_shared<lambertian>(checker)));

    // Left: Gold (Metallic)
    auto gold_albedo = make_shared<solid_color>(0.8, 0.6, 0.2);
    auto gold_rough = make_shared<solid_color>(0.1, 0.1, 0.1);
    auto gold_metal = make_shared<solid_color>(1.0, 1.0, 1.0);
    auto gold_mat =
        make_shared<PBRMaterial>(gold_albedo, gold_rough, gold_metal);
    world.add(make_shared<sphere>(point3(-4, 1, 0), 1.0, gold_mat));

    // Middle: Silver/Textured (Metallic with Noise)
    auto noise = make_shared<noise_texture>(4.0);
    auto silver_rough = make_shared<solid_color>(0.2, 0.2, 0.2);
    auto silver_metal = make_shared<solid_color>(1.0, 1.0, 1.0);
    auto mid_mat = make_shared<PBRMaterial>(noise, silver_rough, silver_metal);
    world.add(make_shared<sphere>(point3(0, 1, 0), 1.0, mid_mat));

    // Right: Blue Plastic (Dielectric-like PBR)
    auto blue_albedo = make_shared<solid_color>(0.1, 0.2, 0.5);
    auto blue_rough = make_shared<solid_color>(0.05, 0.05, 0.05);
    auto blue_metal = make_shared<solid_color>(0.0, 0.0, 0.0);
    auto blue_mat =
        make_shared<PBRMaterial>(blue_albedo, blue_rough, blue_metal);
    world.add(make_shared<sphere>(point3(4, 1, 0), 1.0, blue_mat));

    return make_shared<bvh_node>(world, 0, 1);
}

shared_ptr<hittable> pbr_spheres_grid() {
    hittable_list world;

    auto checker = make_shared<checker_texture>(color(0.2, 0.3, 0.1),
                                                color(0.9, 0.9, 0.9));
    world.add(make_shared<sphere>(point3(0, -1000, 0), 1000,
                                  make_shared<lambertian>(checker)));

    int rows = 7;
    int cols = 7;
    double spacing = 2.5;

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            double metallic_val = (double)row / (rows - 1);
            double roughness_val = clamp((double)col / (cols - 1), 0.05, 1.0);

            auto albedo = make_shared<solid_color>(0.5, 0.0, 0.0);
            auto roughness = make_shared<solid_color>(
                roughness_val, roughness_val, roughness_val);
            auto metallic = make_shared<solid_color>(metallic_val, metallic_val,
                                                     metallic_val);

            auto mat = make_shared<PBRMaterial>(albedo, roughness, metallic);

            double x = (col - (cols - 1) / 2.0) * spacing;
            double z = (row - (rows - 1) / 2.0) * spacing;

            world.add(make_shared<sphere>(point3(x, 1, z), 1.0, mat));
        }
    }

    auto light_mat = make_shared<diffuse_light>(color(10, 10, 10));
    // 将主光源移到相机上方 (y=60)，避免遮挡视线
    world.add(make_shared<sphere>(point3(0, 60, 0), 10, light_mat));
    // 调整侧面辅助光源的位置
    world.add(make_shared<sphere>(point3(-20, 10, 20), 2, light_mat));
    world.add(make_shared<sphere>(point3(20, 10, 20), 2, light_mat));

    return make_shared<bvh_node>(world, 0, 1);
}

shared_ptr<hittable> pbr_materials_gallery() {
    hittable_list world;

    auto ground_mat = make_shared<lambertian>(color(0.5, 0.5, 0.5));
    world.add(make_shared<sphere>(point3(0, -1000, 0), 1000, ground_mat));

    // Non-Metals (Metallic = 0.0)
    struct MaterialInfo {
        color albedo;
        double metallic;
        double roughness;
    };

    std::vector<MaterialInfo> non_metals = {
        {color(0.02, 0.02, 0.02), 0.0, 0.5}, // Charcoal
        {color(0.21, 0.28, 0.08), 0.0, 0.5}, // Grass
        {color(0.51, 0.51, 0.51), 0.0, 0.5}, // Concrete
        {color(0.7, 0.7, 0.7), 0.0, 0.5},    // White Paint
        {color(0.81, 0.81, 0.81), 0.0, 0.5}  // Fresh Snow
    };

    std::vector<MaterialInfo> metals = {
        {color(0.54, 0.49, 0.42), 1.0, 0.2}, // Titanium
        {color(0.56, 0.57, 0.58), 1.0, 0.2}, // Iron
        {color(0.95, 0.64, 0.54), 1.0, 0.2}, // Copper
        {color(1.00, 0.71, 0.29), 1.0, 0.2}, // Gold
        {color(0.91, 0.92, 0.92), 1.0, 0.2}, // Aluminium
        {color(0.97, 0.96, 0.91), 1.0, 0.2}  // Silver
    };

    double spacing = 2.5;
    double start_x_nm = -((non_metals.size() - 1) * spacing) / 2.0;

    for (size_t i = 0; i < non_metals.size(); ++i) {
        auto &info = non_metals[i];
        auto mat = make_shared<PBRMaterial>(
            make_shared<solid_color>(info.albedo),
            make_shared<solid_color>(info.roughness, info.roughness,
                                     info.roughness),
            make_shared<solid_color>(info.metallic, info.metallic,
                                     info.metallic));
        world.add(make_shared<sphere>(point3(start_x_nm + i * spacing, 1, -2),
                                      1.0, mat));
    }

    double start_x_m = -((metals.size() - 1) * spacing) / 2.0;
    for (size_t i = 0; i < metals.size(); ++i) {
        auto &info = metals[i];
        auto mat = make_shared<PBRMaterial>(
            make_shared<solid_color>(info.albedo),
            make_shared<solid_color>(info.roughness, info.roughness,
                                     info.roughness),
            make_shared<solid_color>(info.metallic, info.metallic,
                                     info.metallic));
        world.add(make_shared<sphere>(point3(start_x_m + i * spacing, 1, 2),
                                      1.0, mat));
    }

    // Lights
    auto light_mat = make_shared<diffuse_light>(color(10, 10, 10));
    world.add(make_shared<sphere>(point3(0, 20, 10), 5, light_mat));

    return make_shared<bvh_node>(world, 0, 1);
}

shared_ptr<hittable> pbr_reference_scene() {
    hittable_list world;

    auto ground_mat = make_shared<lambertian>(color(0.2, 0.2, 0.2));
    world.add(make_shared<sphere>(point3(0, -1000, 0), 1000, ground_mat));

    struct MaterialInfo {
        color albedo;
        double metallic;
        double roughness;
        const char *name;
    };

    // 1. Metals
    std::vector<MaterialInfo> metals = {
        {color(1.000, 0.766, 0.336), 1.0, 0.2, "Gold"},
        {color(0.955, 0.638, 0.538), 1.0, 0.2, "Copper"},
        {color(0.560, 0.570, 0.580), 1.0, 0.3, "Iron"},
        {color(0.913, 0.922, 0.924), 1.0, 0.1, "Aluminum"}};

    // 2. Non-Metals
    std::vector<MaterialInfo> non_metals = {
        {color(1.0, 0.1, 0.1), 0.0, 0.1, "Red Plastic"},
        {color(0.1, 0.1, 1.0), 0.0, 0.8, "Blue Rubber"},
        {color(1.0, 1.0, 1.0), 0.0, 0.02, "Water/Ice"},
        {color(0.02, 0.02, 0.02), 0.0, 0.9, "Charcoal"},
        {color(0.81, 0.81, 0.81), 0.0, 0.9, "Fresh Snow"}};

    // 3. Roughness Gradient (Gold)
    std::vector<double> roughness_gradient = {0.0, 0.2, 0.4, 0.6, 0.8, 1.0};

    double spacing = 2.5;
    double row_z = 0.0;

    // Row 1: Metals
    row_z = -5.0;
    double start_x = -((metals.size() - 1) * spacing) / 2.0;
    for (size_t i = 0; i < metals.size(); ++i) {
        auto &info = metals[i];
        auto mat = make_shared<PBRMaterial>(
            make_shared<solid_color>(info.albedo),
            make_shared<solid_color>(info.roughness, info.roughness,
                                     info.roughness),
            make_shared<solid_color>(info.metallic, info.metallic,
                                     info.metallic));
        world.add(make_shared<sphere>(point3(start_x + i * spacing, 1, row_z),
                                      1.0, mat));
    }

    // Row 2: Non-Metals
    row_z = 0.0;
    start_x = -((non_metals.size() - 1) * spacing) / 2.0;
    for (size_t i = 0; i < non_metals.size(); ++i) {
        auto &info = non_metals[i];
        auto mat = make_shared<PBRMaterial>(
            make_shared<solid_color>(info.albedo),
            make_shared<solid_color>(info.roughness, info.roughness,
                                     info.roughness),
            make_shared<solid_color>(info.metallic, info.metallic,
                                     info.metallic));
        world.add(make_shared<sphere>(point3(start_x + i * spacing, 1, row_z),
                                      1.0, mat));
    }

    // Row 3: Roughness Gradient (Gold)
    row_z = 5.0;
    start_x = -((roughness_gradient.size() - 1) * spacing) / 2.0;
    color gold_albedo(1.000, 0.766, 0.336);
    for (size_t i = 0; i < roughness_gradient.size(); ++i) {
        double r = roughness_gradient[i];
        auto mat = make_shared<PBRMaterial>(
            make_shared<solid_color>(gold_albedo),
            make_shared<solid_color>(r, r, r),
            make_shared<solid_color>(1.0, 1.0, 1.0)); // Metallic = 1.0
        world.add(make_shared<sphere>(point3(start_x + i * spacing, 1, row_z),
                                      1.0, mat));
    }

    // Lights
    auto light_mat = make_shared<diffuse_light>(color(10, 10, 10));
    world.add(make_shared<sphere>(point3(0, 30, 10), 8, light_mat));
    world.add(make_shared<sphere>(point3(-20, 10, 20), 2, light_mat));
    world.add(make_shared<sphere>(point3(20, 10, 20), 2, light_mat));

    return make_shared<bvh_node>(world, 0, 1);
}

shared_ptr<hittable> point_light_scene() {
    hittable_list world;

    auto ground_mat = make_shared<lambertian>(color(0.5, 0.5, 0.5));
    world.add(make_shared<sphere>(point3(0, -1000, 0), 1000, ground_mat));

    // Diffuse Sphere
    auto lambert = make_shared<lambertian>(color(0.8, 0.2, 0.2));
    world.add(make_shared<sphere>(point3(0, 1, 0), 1.0, lambert));

    // PBR Metal Sphere
    auto albedo = make_shared<solid_color>(0.9, 0.9, 0.9);
    auto roughness = make_shared<solid_color>(0.05, 0.05, 0.05); // Smooth
    auto metallic = make_shared<solid_color>(1.0, 1.0, 1.0);
    auto metal_mat = make_shared<PBRMaterial>(albedo, roughness, metallic);
    world.add(make_shared<sphere>(point3(-3, 1, 0), 1.0, metal_mat));

    // PBR Dielectric-like Sphere
    auto d_albedo = make_shared<solid_color>(0.2, 0.2, 0.8);
    auto d_roughness = make_shared<solid_color>(0.1, 0.1, 0.1);
    auto d_metallic = make_shared<solid_color>(0.0, 0.0, 0.0);
    auto plastic_mat =
        make_shared<PBRMaterial>(d_albedo, d_roughness, d_metallic);
    world.add(make_shared<sphere>(point3(3, 1, 0), 1.0, plastic_mat));

    return make_shared<bvh_node>(world, 0, 1);
}

shared_ptr<hittable> mis_demo() {
    hittable_list world;

    auto ground_mat = make_shared<lambertian>(color(0.5, 0.5, 0.5));
    world.add(make_shared<sphere>(point3(0, -1000, 0), 1000, ground_mat));

    // Smooth Metal (Roughness 0.05) - BSDF sampling dominates for specular
    // reflection
    auto smooth_metal =
        make_shared<PBRMaterial>(make_shared<solid_color>(0.9, 0.9, 0.9),
                                 make_shared<solid_color>(0.05, 0.05, 0.05),
                                 make_shared<solid_color>(1.0, 1.0, 1.0));
    world.add(make_shared<sphere>(point3(-4, 1, 0), 1.0, smooth_metal));

    // Rough Metal (Roughness 0.5) - NEE dominates
    auto rough_metal =
        make_shared<PBRMaterial>(make_shared<solid_color>(0.9, 0.9, 0.9),
                                 make_shared<solid_color>(0.5, 0.5, 0.5),
                                 make_shared<solid_color>(1.0, 1.0, 1.0));
    world.add(make_shared<sphere>(point3(0, 1, 0), 1.0, rough_metal));

    // Diffuse - NEE dominates
    auto diffuse = make_shared<lambertian>(color(0.2, 0.2, 0.8));
    world.add(make_shared<sphere>(point3(4, 1, 0), 1.0, diffuse));

    // Emissive Sphere (BSDF sampling only, as it's not in lights list)
    auto light_mat = make_shared<diffuse_light>(color(10, 5, 5));
    world.add(make_shared<sphere>(point3(0, 1, -3), 1.0, light_mat));

    return make_shared<bvh_node>(world, 0, 1);
}

shared_ptr<hittable> mis_comparison_scene() {
    hittable_list world;

    auto ground_mat = make_shared<lambertian>(color(0.5, 0.5, 0.5));
    world.add(make_shared<sphere>(point3(0, -1000, 0), 1000, ground_mat));

    // 1. Smooth Metal (Roughness 0.001) - NEE struggles with Large Light, BSDF
    // wins
    auto smooth_metal = make_shared<PBRMaterial>(
        make_shared<solid_color>(0.9, 0.6, 0.2), // Gold
        make_shared<solid_color>(0.001, 0.001, 0.001),
        make_shared<solid_color>(1.0, 1.0, 1.0));
    world.add(make_shared<sphere>(point3(-2.5, 1, 0), 1.0, smooth_metal));

    // 2. Rough Metal (Roughness 0.4) - NEE is fine
    auto rough_metal = make_shared<PBRMaterial>(
        make_shared<solid_color>(0.8, 0.8, 0.8), // Silver
        make_shared<solid_color>(0.4, 0.4, 0.4),
        make_shared<solid_color>(1.0, 1.0, 1.0));
    world.add(make_shared<sphere>(point3(0, 1, 0), 1.0, rough_metal));

    // 3. Glass (Transmission)
    auto glass = make_shared<dielectric>(1.5);
    world.add(make_shared<sphere>(point3(2.5, 1, 0), 1.0, glass));

    // Lights are added in select_scene via config.lights
    // But we need visual representation here

    // Large Area Light (Top)
    auto light_mat = make_shared<diffuse_light>(color(5, 5, 5));
    // Centered at (0, 10, 0), size 20x20
    // Using xz_rect for top light (y is constant)
    // xz_rect normal is (0, 1, 0) (up), so we need to flip it to face down
    world.add(make_shared<flip_face>(
        make_shared<xz_rect>(-10, 10, -10, 10, 10, light_mat)));

    // Small Intense Light (Right Side)
    auto small_light_mat = make_shared<diffuse_light>(color(50, 50, 50));
    // Centered at (6, 4, 2), size 0.5x0.5 facing -X
    // Using yz_rect for side light (x is constant)
    // yz_rect normal is (1, 0, 0) (right), so we need to flip it to face left
    // (-X)
    world.add(make_shared<flip_face>(
        make_shared<yz_rect>(3.75, 4.25, 1.75, 2.25, 6, small_light_mat)));

    return make_shared<bvh_node>(world, 0, 1);
}

shared_ptr<hittable> hdr_demo_scene() {
    hittable_list world;

    // 悬空球体展示
    // 1. 完美镜面 (Chrome)
    auto chrome = make_shared<metal>(color(0.9, 0.9, 0.9), 0.0);
    world.add(make_shared<sphere>(point3(-4, 1, 0), 1.0, chrome));

    // 2. 粗糙金属 (Rough Gold)
    auto rough_gold = make_shared<PBRMaterial>(
        make_shared<solid_color>(1.0, 0.71, 0.29), // Gold
        make_shared<solid_color>(0.2, 0.2, 0.2),   // Roughness
        make_shared<solid_color>(1.0, 1.0, 1.0));  // Metallic
    world.add(make_shared<sphere>(point3(0, 1, 0), 1.0, rough_gold));

    // 3. 玻璃 (Glass)
    auto glass = make_shared<dielectric>(1.5);
    world.add(make_shared<sphere>(point3(4, 1, 0), 1.0, glass));

    // 4. 漫反射 (Matte White)
    // auto matte = make_shared<lambertian>(color(0.8, 0.8, 0.8));
    // world.add(make_shared<sphere>(point3(0, -1000, 0), 1000, matte)); // 地面

    return make_shared<bvh_node>(world, 0, 1);
}

shared_ptr<hittable> directional_light_scene() {
    hittable_list objects;

    // Ground
    auto ground_material = make_shared<lambertian>(color(0.8, 0.8, 0.8));
    objects.add(
        make_shared<sphere>(point3(0, -1000, 0), 1000, ground_material));

    // Identical boxes to demonstrate parallel shadows
    auto mat_red = make_shared<lambertian>(color(0.8, 0.1, 0.1));
    auto mat_green = make_shared<lambertian>(color(0.1, 0.8, 0.1));
    auto mat_blue = make_shared<lambertian>(color(0.1, 0.1, 0.8));

    // Three tall boxes
    objects.add(
        make_shared<box>(point3(-4, 0, -2), point3(-3, 3, -1), mat_red));
    objects.add(
        make_shared<box>(point3(-0.5, 0, -2), point3(0.5, 3, -1), mat_green));
    objects.add(make_shared<box>(point3(3, 0, -2), point3(4, 3, -1), mat_blue));

    // Ray Tracing features: Mirror and Glass
    auto material_metal = make_shared<metal>(color(0.8, 0.8, 0.8), 0.0);
    objects.add(make_shared<sphere>(point3(-2, 1, 2), 1.0, material_metal));

    auto material_glass = make_shared<dielectric>(1.5);
    objects.add(make_shared<sphere>(point3(2, 1, 2), 1.0, material_glass));

    // Floating sphere to show shadow on ground clearly
    auto material_diffuse = make_shared<lambertian>(color(0.8, 0.5, 0.2));
    objects.add(make_shared<sphere>(point3(0, 5, 0), 1.0, material_diffuse));

    return make_shared<bvh_node>(objects, 0, 1);
}

shared_ptr<hittable> spot_light_scene() {
    hittable_list objects;

    auto ground = make_shared<lambertian>(color(0.5, 0.5, 0.5));
    objects.add(make_shared<sphere>(point3(0, -1000, 0), 1000, ground));

    auto white = make_shared<lambertian>(color(0.9, 0.9, 0.9));
    objects.add(make_shared<sphere>(point3(0, 1, 0), 1, white));

    auto red = make_shared<lambertian>(color(0.8, 0.1, 0.1));
    objects.add(make_shared<box>(point3(-2, 0, -1), point3(-1, 2, 0), red));

    auto blue = make_shared<lambertian>(color(0.1, 0.1, 0.8));
    objects.add(make_shared<box>(point3(1, 0, -1), point3(2, 2, 0), blue));

    return make_shared<bvh_node>(objects, 0, 1);
}

shared_ptr<hittable> environment_light_scene() {
    hittable_list objects;

    // Metal sphere to show reflection
    auto material_metal = make_shared<metal>(color(0.8, 0.8, 0.8), 0.0);
    objects.add(make_shared<sphere>(point3(-2, 1, 0), 1.0, material_metal));

    // Glass sphere to show refraction
    auto material_glass = make_shared<dielectric>(1.5);
    objects.add(make_shared<sphere>(point3(0, 1, 0), 1.0, material_glass));

    // Diffuse sphere to show lighting
    auto material_diffuse = make_shared<lambertian>(color(0.8, 0.5, 0.2));
    objects.add(make_shared<sphere>(point3(2, 1, 0), 1.0, material_diffuse));

    // Ground
    auto ground_material = make_shared<lambertian>(color(0.5, 0.5, 0.5));
    objects.add(
        make_shared<sphere>(point3(0, -1000, 0), 1000, ground_material));

    return make_shared<bvh_node>(objects, 0, 1);
}

shared_ptr<hittable> quad_light_scene() {
    hittable_list objects;

    auto ground = make_shared<lambertian>(color(0.5, 0.5, 0.5));
    objects.add(make_shared<sphere>(point3(0, -1000, 0), 1000, ground));

    auto sphere_mat = make_shared<lambertian>(color(0.1, 0.2, 0.5));
    objects.add(make_shared<sphere>(point3(0, 2, 0), 2, sphere_mat));

    // Visible light geometry (must match QuadLight parameters)
    // x: -2 to 2, z: -2 to 2, y: 7
    // Use flip_face to make the normal point downward (-Y)
    auto light_mat = make_shared<diffuse_light>(color(15, 15, 15));
    auto light_rect = make_shared<xz_rect>(-2, 2, -2, 2, 7, light_mat);
    objects.add(make_shared<flip_face>(light_rect));

    return make_shared<bvh_node>(objects, 0, 1);
}

shared_ptr<hittable> cornell_box_nee() {
    hittable_list objects;

    auto red = make_shared<lambertian>(color(.65, .05, .05));
    auto white = make_shared<lambertian>(color(.73, .73, .73));
    auto green = make_shared<lambertian>(color(.12, .45, .15));
    auto light = make_shared<diffuse_light>(color(15, 15, 15));

    objects.add(make_shared<yz_rect>(0, 555, 0, 555, 555, green));
    objects.add(make_shared<yz_rect>(0, 555, 0, 555, 0, red));
    // Flip face so light emits downward
    objects.add(make_shared<flip_face>(
        make_shared<xz_rect>(213, 343, 227, 332, 554, light)));
    objects.add(make_shared<xz_rect>(0, 555, 0, 555, 0, white));
    objects.add(make_shared<xz_rect>(0, 555, 0, 555, 555, white));
    objects.add(make_shared<xy_rect>(0, 555, 0, 555, 555, white));

    shared_ptr<hittable> box1 =
        make_shared<box>(point3(0, 0, 0), point3(165, 330, 165), white);
    box1 = make_shared<rotate_y>(box1, 15);
    box1 = make_shared<translate>(box1, vec3(265, 0, 295));
    objects.add(box1);

    shared_ptr<hittable> box2 =
        make_shared<box>(point3(0, 0, 0), point3(165, 165, 165), white);
    box2 = make_shared<rotate_y>(box2, -18);
    box2 = make_shared<translate>(box2, vec3(130, 0, 65));
    objects.add(box2);

    return make_shared<bvh_node>(objects, 0, 1);
}

shared_ptr<hittable> final_scene_nee() {
    hittable_list boxes1;
    auto ground = make_shared<lambertian>(color(0.48, 0.83, 0.53));

    const int boxes_per_side = 20;
    for (int i = 0; i < boxes_per_side; i++) {
        for (int j = 0; j < boxes_per_side; j++) {
            auto w = 100.0;
            auto x0 = -1000.0 + i * w;
            auto z0 = -1000.0 + j * w;
            auto y0 = 0.0;
            auto x1 = x0 + w;
            auto y1 = random_double(1, 101);
            auto z1 = z0 + w;

            boxes1.add(make_shared<box>(point3(x0, y0, z0), point3(x1, y1, z1),
                                        ground));
        }
    }

    hittable_list objects;

    objects.add(make_shared<bvh_node>(boxes1, 0, 1));

    auto light = make_shared<diffuse_light>(color(7, 7, 7));
    // Flip face so light emits downward
    objects.add(make_shared<flip_face>(
        make_shared<xz_rect>(123, 423, 147, 412, 554, light)));

    auto center1 = point3(400, 400, 200);
    auto center2 = center1 + vec3(30, 0, 0);
    auto moving_sphere_material = make_shared<lambertian>(color(0.7, 0.3, 0.1));
    objects.add(make_shared<moving_sphere>(center1, center2, 0, 1, 50,
                                           moving_sphere_material));

    objects.add(make_shared<sphere>(point3(260, 150, 45), 50,
                                    make_shared<dielectric>(1.5)));
    objects.add(
        make_shared<sphere>(point3(0, 150, 145), 50,
                            make_shared<metal>(color(0.8, 0.8, 0.9), 1.0)));

    auto boundary = make_shared<sphere>(point3(360, 150, 145), 70,
                                        make_shared<dielectric>(1.5));
    objects.add(boundary);
    objects.add(
        make_shared<constant_medium>(boundary, 0.2, color(0.2, 0.4, 0.9)));

    boundary = make_shared<sphere>(point3(0, 0, 0), 5000,
                                   make_shared<dielectric>(1.5));
    objects.add(make_shared<constant_medium>(boundary, .0001, color(1, 1, 1)));

    auto emat =
        make_shared<lambertian>(make_shared<image_texture>("earthmap.jpg"));
    objects.add(make_shared<sphere>(point3(400, 200, 400), 100, emat));

    auto pertext = make_shared<noise_texture>(0.1);
    objects.add(make_shared<sphere>(point3(220, 280, 300), 80,
                                    make_shared<lambertian>(pertext)));

    hittable_list boxes2;
    auto white = make_shared<lambertian>(color(.73, .73, .73));
    int ns = 1000;
    for (int j = 0; j < ns; j++) {
        boxes2.add(make_shared<sphere>(point3::random(0, 165), 10, white));
    }

    objects.add(make_shared<translate>(
        make_shared<rotate_y>(make_shared<bvh_node>(boxes2, 0.0, 1.0), 15),
        vec3(-100, 270, 395)));

    return make_shared<bvh_node>(objects, 0, 1);
}

SceneConfig select_scene(int scene_id) {
    SceneConfig config;

    switch (scene_id) {
    case 1:
        config.world = random_scene();
        config.aspect_ratio = 1.0;
        config.image_width = 400;
        config.samples_per_pixel = 50;
        config.background = color(0.70, 0.80, 1.00);
        config.lookfrom = point3(13, 2, 3);
        config.lookat = point3(0, 0, 0);
        config.vfov = 20.0;
        config.aperture = 0.1;
        break;

    case 2:
        config.world = two_spheres();
        config.background = color(0.70, 0.80, 1.00);
        config.lookfrom = point3(13, 2, 3);
        config.lookat = point3(0, 0, 0);
        config.vfov = 20.0;
        break;

    case 4:
        config.world = earth();
        config.lookfrom = point3(13, 2, 3);
        config.background = color(0.70, 0.80, 1.00);
        config.lookat = point3(0, 0, 0);
        config.vfov = 20.0;
        break;

    case 5:
        config.world = simple_light();
        config.background = color(0.0, 0.0, 0.0);
        config.lookfrom = point3(26, 3, 6);
        config.lookat = point3(0, 2, 0);
        config.vfov = 20.0;
        break;

    case 6:
        config.world = example_light_scene();
        config.background = color(0.00, 0.00, 0.00);
        config.lookfrom = point3(13, 2, 3);
        config.lookat = point3(0, 0, 0);
        config.vfov = 20.0;
        config.aperture = 0.0;
        break;

    case 7:
        config.world = cornell_box();
        config.aspect_ratio = 1.0;
        config.image_width = 600;
        config.samples_per_pixel = 400;
        config.background = color(0, 0, 0);
        config.lookfrom = point3(278, 278, -800);
        config.lookat = point3(278, 278, 0);
        config.vfov = 40.0;
        config.aperture = 0.0;
        break;

    case 8:
        config.world = cornell_smoke();
        config.aspect_ratio = 1.0;
        config.image_width = 600;
        config.samples_per_pixel = 200;
        config.background = color(0, 0, 0);
        config.lookfrom = point3(278, 278, -800);
        config.lookat = point3(278, 278, 0);
        config.vfov = 40.0;
        break;

    case 9:
        config.world = final_scene();
        config.aspect_ratio = 1.0;
        config.image_width = 800;
        config.samples_per_pixel = 10000;
        config.background = color(0, 0, 0);
        config.lookfrom = point3(478, 278, -600);
        config.lookat = point3(278, 278, 0);
        config.vfov = 40.0;
        break;

    case 11:
        config.world = pbr_test_scene();
        config.aspect_ratio = 16.0 / 9.0;
        config.image_width = 800;
        config.samples_per_pixel = 100;
        config.background = color(0.70, 0.80, 1.00);
        config.lookfrom = point3(13, 2, 3);
        config.lookat = point3(0, 0, 0);
        config.vfov = 20.0;
        break;

    case 12:
        config.world = pbr_spheres_grid();
        config.aspect_ratio = 1.0;
        config.image_width = 800;
        config.samples_per_pixel = 500;
        config.background = color(0.05, 0.05, 0.05);
        config.lookfrom = point3(0, 40, 0);
        config.lookat = point3(0, 0, 0);
        config.vup = vec3(0, 0, -1);
        config.vfov = 25.0;
        break;

    case 13:
        config.world = pbr_materials_gallery();
        config.aspect_ratio = 16.0 / 9.0;
        config.image_width = 800;
        config.samples_per_pixel = 1000;
        config.background = color(0.1, 0.1, 0.1);
        config.lookfrom = point3(0, 10, 20);
        config.lookat = point3(0, 0, 0);
        config.vfov = 30.0;
        break;

    case 14:
        config.world = pbr_reference_scene();
        config.aspect_ratio = 16.0 / 9.0;
        config.image_width = 1000;
        config.samples_per_pixel = 5000;
        config.background = color(0.05, 0.05, 0.05);
        config.lookfrom = point3(0, 15, 25);
        config.lookat = point3(0, 0, 0);
        config.vfov = 30.0;
        break;

    case 15:
        config.world = point_light_scene();
        config.aspect_ratio = 16.0 / 9.0;
        config.image_width = 800;
        config.samples_per_pixel = 100;
        config.background = color(0.0, 0.0, 0.0);
        config.lookfrom = point3(0, 5, 10);
        config.lookat = point3(0, 1, 0);
        config.vfov = 30.0;
        config.lights.push_back(
            make_shared<PointLight>(point3(0, 6, 2), color(50, 50, 50)));
        break;

    case 16:
        config.world = mis_demo();
        config.aspect_ratio = 16.0 / 9.0;
        config.image_width = 800;
        config.samples_per_pixel = 500;
        config.background = color(0.1, 0.1, 0.1);
        config.lookfrom = point3(0, 5, 10);
        config.lookat = point3(0, 1, 0);
        config.vfov = 30.0;
        // Add PointLight (NEE sampling)
        config.lights.push_back(
            make_shared<PointLight>(point3(5, 10, 5), color(100, 100, 100)));
        break;

    case 17:
        config.world = directional_light_scene();
        config.aspect_ratio = 16.0 / 9.0;
        config.image_width = 800;
        config.samples_per_pixel = 100;
        config.background = color(0.0, 0.0, 0.0);
        config.lookfrom = point3(0, 6, 12);
        config.lookat = point3(0, 2, 0);
        config.vfov = 30.0;
        config.lights.push_back(
            make_shared<DirectionalLight>(vec3(-1, -1, -0.5), color(3, 3, 3)));
        break;
    case 18:
        config.world = spot_light_scene();
        config.aspect_ratio = 16.0 / 9.0;
        config.image_width = 800;
        config.samples_per_pixel = 100;
        config.background = color(0.0, 0.0, 0.0);
        config.lookfrom = point3(0, 5, 10);
        config.lookat = point3(0, 1, 0);
        config.vfov = 30.0;
        config.lights.push_back(make_shared<SpotLight>(
            point3(0, 8, 4), vec3(0, -1, -0.5), 20.0, color(2000, 2000, 2000)));
        break;
    case 19:
        config.world = environment_light_scene();
        config.aspect_ratio = 16.0 / 9.0;
        config.image_width = 800;
        config.samples_per_pixel = 100;
        config.background = color(0.0, 0.0, 0.0);
        config.lookfrom = point3(0, 2, 10);
        config.lookat = point3(0, 1, 0);
        config.vfov = 30.0;
        config.lights.push_back(make_shared<EnvironmentLight>("sky.hdr"));
        break;
    case 20:
        config.world = quad_light_scene();
        config.aspect_ratio = 16.0 / 9.0;
        config.image_width = 800;
        config.samples_per_pixel = 100;
        config.background = color(0.0, 0.0, 0.0);
        config.lookfrom = point3(0, 4, 15);
        config.lookat = point3(0, 3, 0);
        config.vfov = 50.0;
        // Add QuadLight (NEE sampling)
        // Q = (-2, 7, -2), u = (4, 0, 0), v = (0, 0, 4)
        config.lights.push_back(
            make_shared<QuadLight>(point3(-2, 7, -2), vec3(4, 0, 0),
                                   vec3(0, 0, 4), color(15, 15, 15)));
        break;

    case 21:
        config.world = cornell_box_nee();
        config.aspect_ratio = 1.0;
        config.image_width = 600;
        config.samples_per_pixel = 400;
        config.background = color(0, 0, 0);
        config.lookfrom = point3(278, 278, -800);
        config.lookat = point3(278, 278, 0);
        config.vfov = 40.0;
        config.aperture = 0.0;
        // Add QuadLight: xz_rect(213, 343, 227, 332, 554)
        // Q = (213, 554, 227), u = (130, 0, 0), v = (0, 0, 105)
        config.lights.push_back(
            make_shared<QuadLight>(point3(213, 554, 227), vec3(130, 0, 0),
                                   vec3(0, 0, 105), color(15, 15, 15)));
        break;

    case 22:
        config.world = final_scene_nee();
        config.aspect_ratio = 1.0;
        config.image_width = 800;
        config.samples_per_pixel = 500;
        config.background = color(0, 0, 0);
        config.lookfrom = point3(478, 278, -600);
        config.lookat = point3(278, 278, 0);
        config.vfov = 40.0;
        // Add QuadLight: xz_rect(123, 423, 147, 412, 554)
        // Q = (123, 554, 147), u = (300, 0, 0), v = (0, 0, 265)
        config.lights.push_back(
            make_shared<QuadLight>(point3(123, 554, 147), vec3(300, 0, 0),
                                   vec3(0, 0, 265), color(7, 7, 7)));
        break;

    case 23:
        config.world = mis_comparison_scene();
        config.aspect_ratio = 16.0 / 9.0;
        config.image_width = 800;
        config.samples_per_pixel = 64; // Low samples to highlight noise
        config.background = color(0.0, 0.0, 0.0);
        config.lookfrom = point3(0, 3, 8);
        config.lookat = point3(0, 1, 0);
        config.vfov = 35.0;

        // Large Light (Top)
        config.lights.push_back(
            make_shared<QuadLight>(point3(-10, 10, -10), vec3(20, 0, 0),
                                   vec3(0, 0, 20), color(5, 5, 5)));

        // Small Light (Right)
        config.lights.push_back(
            make_shared<QuadLight>(point3(6, 4, 2), vec3(0, 0.5, 0),
                                   vec3(0, 0, 0.5), color(50, 50, 50)));
        break;

    case 24: // brown_photostudio_02_4k.hdr
        config.world = hdr_demo_scene();
        config.aspect_ratio = 16.0 / 9.0;
        config.image_width = 800;
        config.samples_per_pixel = 200;
        config.background = color(0.0, 0.0, 0.0);
        config.lookfrom = point3(0, 3, 10);
        config.lookat = point3(0, 1, 0);
        config.vfov = 30.0;
        config.lights.push_back(
            make_shared<EnvironmentLight>("brown_photostudio_02_4k.hdr"));
        break;

    case 25: // cedar_bridge_sunset_2_4k.hdr
        config.world = hdr_demo_scene();
        config.aspect_ratio = 16.0 / 9.0;
        config.image_width = 800;
        config.samples_per_pixel = 200;
        config.background = color(0.0, 0.0, 0.0);
        config.lookfrom = point3(0, 3, 10);
        config.lookat = point3(0, 1, 0);
        config.vfov = 30.0;
        config.lights.push_back(
            make_shared<EnvironmentLight>("cedar_bridge_sunset_2_4k.hdr"));
        break;

    case 26: // rnl_probe.hdr
        config.world = hdr_demo_scene();
        config.aspect_ratio = 16.0 / 9.0;
        config.image_width = 800;
        config.samples_per_pixel = 200;
        config.background = color(0.0, 0.0, 0.0);
        config.lookfrom = point3(0, 3, 10);
        config.lookat = point3(0, 1, 0);
        config.vfov = 30.0;
        config.lights.push_back(make_shared<EnvironmentLight>("rnl_probe.hdr"));
        break;

    case 27: // stpeters_probe.hdr
        config.world = hdr_demo_scene();
        config.aspect_ratio = 16.0 / 9.0;
        config.image_width = 800;
        config.samples_per_pixel = 200;
        config.background = color(0.0, 0.0, 0.0);
        config.lookfrom = point3(0, 3, 10);
        config.lookat = point3(0, 1, 0);
        config.vfov = 30.0;
        config.lights.push_back(
            make_shared<EnvironmentLight>("stpeters_probe.hdr"));
        break;

    case 28: // uffizi_probe.hdr
        config.world = hdr_demo_scene();
        config.aspect_ratio = 16.0 / 9.0;
        config.image_width = 800;
        config.samples_per_pixel = 200;
        config.background = color(0.0, 0.0, 0.0);
        config.lookfrom = point3(0, 3, 10);
        config.lookat = point3(0, 1, 0);
        config.vfov = 30.0;
        config.lights.push_back(
            make_shared<EnvironmentLight>("uffizi_probe.hdr"));
        break;

    case 10:
    default:
        config.world = two_perlin_spheres();
        config.background = color(0.70, 0.80, 1.00);
        config.lookfrom = point3(13, 2, 3);
        config.lookat = point3(0, 0, 0);
        config.vfov = 20.0;
        break;
    }

    return config;
}