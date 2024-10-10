#pragma once

#include <geomc/linalg/AffineTransform.h>

#include <stereo/defs.h>

namespace stereo {

enum struct PrimitiveType {
    Triangles,
    Lines,
    Points,
    TriangleStrip,
};

struct Model {
    struct Vert {
        vec3 p;
        vec3 n;
        vec2 uv;
    };

    struct Prim {
        range1i       index_range;
        PrimitiveType geo_type;
        uint32_t      material_id;
        xf3           obj_to_world;
        range3        obj_bounds;
    };

    std::vector<Vert>     verts;
    std::vector<uint32_t> indices;
    std::vector<Prim>     prims;
};

using SharedModel = std::shared_ptr<Model>;

}  // namespace stereo
