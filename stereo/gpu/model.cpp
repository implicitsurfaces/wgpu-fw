#include <geomc/shape/Transformed.h>
#include <stereo/gpu/model.h>

namespace stereo {

uint32_t Model::add_verts(std::initializer_list<Model::Vert> vs) {
    uint32_t base = verts.size();
    verts.insert(verts.end(), vs.begin(), vs.end());
    return base;
}

range1ui Model::add_indices(std::initializer_list<uint32_t> is, uint32_t offset) {
    uint32_t range_begin = indices.size();
    indices.reserve(indices.size() + is.size());
    for (uint32_t i : is) {
        indices.push_back(i + offset);
    }
    return {range_begin, (uint32_t) is.size() - 1};
}

uint32_t Model::add_prim(
    PrimitiveType geo_type,
    std::initializer_list<uint32_t> indices,
    uint32_t   offset,
    const xf3& obj_to_world,
    uint32_t   material_id)
{
    range1ui index_range = add_indices(indices, offset);

    range3 bbox = range3::empty;
    for (uint32_t i = index_range.lo; i <= index_range.hi; ++i) {
        bbox |= verts[this->indices[i]].p;
    }

    prims.push_back({
        index_range,
        geo_type,
        material_id,
        obj_to_world,
        bbox,
    });

    return prims.size() - 1;
}

range3 Model::compute_coarse_bounds() const {
    range3 b = range3::empty;
    for (const Prim& p : prims) {
        b |= affinebox3(p.obj_bounds, p.obj_to_world).bounds();
    }
    return b;
}

range3 Model::compute_vertex_bounds() const {
    range3 b = range3::empty;
    for (const Vert& v : verts) {
        b |= v.p;
    }
    return b;
}

}  // namespace stereo
