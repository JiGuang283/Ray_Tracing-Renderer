#ifndef POINT_LIGHT_H
#define POINT_LIGHT_H

#include "light.h"

class PointLight : public Light {
  public:
    PointLight(const point3 &pos, const color &intensity)
        : m_position(pos), m_intensity(intensity) {
    }

    virtual LightSample sample(const point3 &p) const override {
        LightSample ls;

        vec3 direction = m_position - p;
        double dist_squared = direction.length_squared();

        ls.dist = sqrt(dist_squared);
        ls.wi = direction / ls.dist;
        ls.Li = m_intensity / dist_squared;
        ls.pdf = 1.0;

        return ls;
    }

  private:
    point3 m_position;
    color m_intensity;
};

#endif