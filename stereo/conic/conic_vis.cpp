#include <stereo/conic/conic_vis.h>
#include <stereo/util/prims.h>

namespace stereo {

size_t ConicVisualizer::add_conic_section(
    ModelRef model,
    const Conic<float>& conic,
    size_t n_samples,
    float thickness,
    range angle_range)
{   
    std::vector<vec3> path(n_samples);
    for (size_t i = 0; i < n_samples; ++i) {
        float s = i / (float) (n_samples - 1);
        float theta = angle_range.remap(s);
        
        // on the unit circle in “unit cone” space:
        vec2 r = {std::cos(theta), std::sin(theta)};
        
        // map to actual conic
        path[i] = {conic.from_unit_cone(r), 1};
    }
    
    return add_prim(
        *model,
        add_path<float>(
            *model,
            path.data(),
            path.size(),
            16,
            thickness,
            DotCap<float>(thickness * 2),
            DotCap<float>(thickness * 2)
        )
    );
}


size_t ConicVisualizer::build_cone(
    ModelRef model,
    const Conic<float>& conic,
    size_t n_samples,
    range  height,
    range  angle_range)
{
    size_t n_verts   = model->verts.size();
    size_t n_indices = model->indices.size();
    model->verts.reserve(n_verts + 2 * n_samples);
    // two triangles per segment, n - 1 segments
    model->indices.reserve(n_indices + 6 * (n_samples - 1));
    
    bool ok = true;
    SimpleMatrix<float,3,3> N_mtx = transpose(inv(conic.M_fwd, &ok));
    if (not ok) {
        std::cerr << "singular conic transform" << std::endl;
        return -1;
    }
    
    // generate pts on the cone surface
    range3 bounds = range3::empty;
    model->verts.reserve(n_verts + 2 * n_samples);
    for (size_t i = 0; i < n_samples; ++i) {
        float s = i / (float) (n_samples - 1);
        float angle = angle_range.remap(s);
        
        // On the unit circle in “unit cone” space:
        vec2 r = {std::cos(angle), std::sin(angle)};
        
        // compute points in unit-cone space
        vec3 p0 = vec3{r, 1} * height.lo;
        vec3 p1 = vec3{r, 1} * height.hi;
        vec3 v  = p1 - p0;
        vec3 r0 = vec3{r, 0};
        vec3 n  = r0 - r0.project_on(v);
        
        // transform to the final conic
        vec3 p0c = conic.M_inv * p0;
        vec3 p1c = conic.M_inv * p1;
        vec3 nc  = (N_mtx * n).unit();
        
        model->verts.push_back({
            .p  = p0c,
            .n  = nc,
            .uv = {0.},
        });
        model->verts.push_back({
            .p  = p1c,
            .n  = nc,
            .uv = {0.},
        });
        bounds |= p0c;
        bounds |= p1c;
    }
    
    // Step 3: generate indices for the strip
    model->indices.reserve(n_indices + 6 * (n_samples - 1));
    for (uint32_t i = 0; i < n_samples - 1; ++i) {
        uint32_t j = n_verts + 2 * i;
        
        // 2 triangles per segment
        model->indices.insert(model->indices.end(), {
            j, j + 2, j + 3,
            j, j + 3, j + 1,
        });
    }
    
    // Add a primitive for this curve
    Model::Prim prim {
        .index_range = {
            static_cast<uint32_t>(n_indices),
            static_cast<uint32_t>(model->indices.size() - 1)
        },
        .geo_type     = PrimitiveType::Triangles,
        .material_id  = 0,       // default material
        .obj_to_world = xf3{},   // identity
        // bounding box in object space
        .obj_bounds  = bounds,
    };
    
    model->prims.push_back(prim);
    return model->prims.size() - 1;
}

} // namespace stereo
