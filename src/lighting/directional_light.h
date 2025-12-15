// lighting/directional_light.h
#ifndef DIRECTIONAL_LIGHT_H
#define DIRECTIONAL_LIGHT_H

#include "light.h"

class DirectionalLight : public Light {
public:
    DirectionalLight(const vec3& dir, const color& c)
        : direction(unit_vector(dir)), L(c) {
    }

    virtual LightSample sample(const point3& p, const vec2& u) const override {
        LightSample s;
        s.wi = -direction;
        s.dist = infinity;
        s.Li = L;
        s.is_delta = true;
        s.pdf = 1.0;
        return s;
    }

    virtual bool is_delta() const override {
        return true;
    }

private:
    vec3 direction;
    color L;
};

#endif