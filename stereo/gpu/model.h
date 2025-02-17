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

using MaterialId = uint32_t;

struct Model {
    struct Vert {
        vec3 p;
        vec3 n;
        vec2 uv;
    };

    struct Prim {
        range1ui      index_range;
        PrimitiveType geo_type;
        MaterialId    material_id;
        xf3           obj_to_world;
        range3        obj_bounds;
    };

    std::vector<Vert>     verts;
    std::vector<uint32_t> indices;
    std::vector<Prim>     prims;
    
    uint32_t add_verts(std::initializer_list<Vert> vs);
    range1ui add_indices(std::initializer_list<uint32_t> is, uint32_t offset=0);
    uint32_t add_prim(
        PrimitiveType geo_type,
        std::initializer_list<uint32_t> indices,
        uint32_t   offset       = 0,
        const xf3& obj_to_world = xf3{},
        uint32_t   material_id  = 0
    );
    
    /// Bounds of all the prims' bboxen, including prim transforms
    range3 compute_coarse_bounds() const;
    
    /// Bounds of all the model's verts, not counting any prim transforms
    range3 compute_vertex_bounds() const;
};

using ModelRef = std::shared_ptr<Model>;

}  // namespace stereo
