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
        range1u       index_range;
        PrimitiveType geo_type;
        uint32_t      material_id;
        xf3           obj_to_world;
        range3        obj_bounds;
    };

    enum struct IndexBase {
        Start,
        End,
    };

    using Offset = std::variant<IndexBase, uint32_t>;

    std::vector<Vert>     verts;
    std::vector<uint32_t> indices;
    std::vector<Prim>     prims;

    uint32_t add_verts(std::initializer_list<Vert> vs);
    range1u  add_indices(std::initializer_list<uint32_t> is, Offset offset=IndexBase::Start);
    uint32_t add_prim(
        PrimitiveType geo_type,
        std::initializer_list<uint32_t> indices,
        Offset     offset       = IndexBase::Start,
        const xf3& obj_to_world = xf3{},
        uint32_t   material_id  = 0
    );
};

using ModelRef = std::shared_ptr<Model>;

}  // namespace stereo
