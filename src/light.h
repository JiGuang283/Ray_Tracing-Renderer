#ifndef LIGHT_H
#define LIGHT_H

#include "vec3.h"

struct LightSample {
    color Li;
    vec3 wi;
    double pdf;
    double dist;
};

class Light {
  public:
    virtual ~Light() = default;
    virtual LightSample sample(const point3 &p) const = 0;
};

#endif
