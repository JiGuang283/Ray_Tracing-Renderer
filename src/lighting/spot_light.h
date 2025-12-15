#ifndef SPOT_LIGHT_H
#define SPOT_LIGHT_H

#include "light.h"

class SpotLight : public Light {
public:
    SpotLight(point3 pos, vec3 dir, double cutoff, color intensity)
        : position(pos), direction(unit_vector(dir)), intensity(intensity) {
        auto r = cutoff * (pi / 180.0);
        cos_cutoff = cos(r);
    }

    virtual LightSample sample(const point3& p, const vec2& u) const override {
        LightSample s;
        vec3 d = position - p;
        double dist2 = d.length_squared();
        s.dist = sqrt(dist2);
        s.wi = d / s.dist;
        s.is_delta = true;
        s.pdf = 1.0;

        double cos_theta = dot(-s.wi, direction);

        if (cos_theta < cos_cutoff) {
            s.Li = color(0, 0, 0);
        }
        else {
            s.Li = intensity / dist2;
        }
        return s;
    }

    virtual bool is_delta() const override { return true; }

private:
    point3 position;
    vec3 direction;
    color intensity;
    double cos_cutoff;
};
#endif
