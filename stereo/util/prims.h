#pragma once

#include <variant>
#include <geomc/linalg/Rotation.h>
#include <geomc/shape/SphericalCap.h>
#include <stereo/gpu/model.h>

template <typename T>
using Thickness = std::variant<T, const T*>;

// todo: handle bounds

namespace stereo {

enum struct CapType {
    /**
     * @brief No cap (apologies zoomers).
     * 
     * For 3D caps, this leaves a hole in the end of the path.
     */
    None,
    
    /**
     * @brief Round cap, flush with the path.
     *
     * Makes a path with a rounded tip. The center of the cap is at the end of the path.
     */
    Round,
    
    /**
     * @brief Flat cap, flush with the path.
     *
     * Makes a path with a square tip.
     */
    Flat,
};

template <typename T>
struct ArrowheadCap {
    T length = 2;
    T thickness = 1;
};

template <typename T>
struct BoxCap {
    T size;
};

template <typename T>
struct DotCap {
    T radius;
};

template <typename T>
using PathCap = std::variant<
    CapType,
    ArrowheadCap<T>,
    BoxCap<T>,
    DotCap<T>
>;

template <typename T>
struct SubPrim {
    range1ui  index_range;
    Rect<T,3> bounds;
    
    SubPrim& operator|=(const SubPrim& other) {
        index_range |= other.index_range;
        bounds      |= other.bounds;
        return *this;
    }
};


template <typename T>
index_t add_prim(Model& model, const SubPrim<T>& subprim, MaterialId material_id=0) {
    model.prims.push_back(Model::Prim {
        .index_range  = subprim.index_range,
        .geo_type     = PrimitiveType::Triangles,
        .material_id  = material_id,
        .obj_to_world = {},
        .obj_bounds   = subprim.bounds,
    });
    return model.prims.size() - 1;
}


template <typename T>
SubPrim<T> add_sphere_cap(
    Model& model,
    const SphericalCap<T,3>& cap,
    Vec<T,3> center,
    Vec<T,3> pole,
    T        radius,
    size_t   lat_segments,
    size_t   lon_segments)
{
    SubPrim<T> out;
    std::vector<Vec<T,2>> circle_pts (lon_segments);
    for (int i = 0; i < lon_segments; ++i) {
        T theta = (2 * std::numbers::pi_v<T> * i) / lon_segments;
        circle_pts[i] = {std::cos(theta), std::sin(theta)};
    }
    size_t vert_offset = model.verts.size();
    size_t idx_offset  = model.indices.size();
    
    AffineTransform<T,3> rot = geom::direction_align({0,0,1}, pole.unit());
    
    // compute the vertices
    for (size_t i = 0; i < lat_segments; ++i) {
        T s = ((T) i) / (lat_segments - 1);
        T theta = s * cap.half_angle_radians;
        T r = std::sin(theta);
        T z = std::cos(theta);
        for (int j = 0; j < lon_segments; ++j) {
            float t = ((T) j) / (lon_segments - 1);
            Vec<T,3> p   = rot * Vec<T,3>{r * circle_pts[j], z};
            Vec<T,3> p_v = radius * p + center;
            model.verts.push_back({
                .p  = p_v,
                .n  = p,
                .uv = {s, j / t},
            });
            out.bounds |= p_v;
        }
    }
    
    // compute the indices
    for (size_t i = 0; i < lat_segments - 1; ++i) {
        size_t j = vert_offset + i * lon_segments;
        for (size_t k = 0; k < lon_segments; ++k) {
            // current section
            gpu_size_t i0 = j + k;
            gpu_size_t i1 = j + (k + 1) % lon_segments;
            // next section
            gpu_size_t i2 = i0 + lon_segments;
            gpu_size_t i3 = i1 + lon_segments;
            model.indices.insert(model.indices.end(), {
                i0, i3, i1,
                i0, i2, i3,
            });
        }
    }
    
    out.index_range = {
        (gpu_size_t) idx_offset,
        (gpu_size_t) model.indices.size() - 1,
    };
    return out;
}


template <typename T>
SubPrim<T> add_disk(
    Model&   model,
    Vec<T,3> center,
    Vec<T,3> normal,
    T        radius,
    size_t   segments)
{
    SubPrim<T> out;
    size_t vert_offset = model.verts.size();
    size_t idx_offset  = model.indices.size();
    
    normal = normal.unit();
    AffineTransform<T,3> rot = geom::direction_align({0,0,1}, normal);
    
    // compute the vertices
    // center point:
    model.verts.push_back({
        .p  = center,
        .n  = normal,
        .uv = {0.5, 0.5},
    });
    for (int j = 0; j < segments; ++j) {
        float t = ((T) j) / segments;
        T theta = t * 2 * std::numbers::pi_v<T> * j;
        Vec<T,2> xy  = {std::cos(theta), std::sin(theta)};
        Vec<T,3> p   = radius * (rot * Vec<T,3>{xy, 0}) + center;
        model.verts.push_back({
            .p  = p,
            .n  = normal,
            .uv = 0.5 * xy + 0.5,
        });
        out.bounds |= p;
    }
    
    // compute the indices
    for (size_t k = 0; k < segments; ++k) {
        // current section
        gpu_size_t i0 = vert_offset;
        gpu_size_t i1 = 1 + vert_offset + k;
        gpu_size_t i2 = 1 + vert_offset + (k + 1) % segments;
        model.indices.insert(model.indices.end(), {
            i0, i1, i2,
        });
    }
    
    out.index_range |= {
        (gpu_size_t) idx_offset,
        (gpu_size_t) model.indices.size() - 1,
    };
    return out;
}


template <typename T>
SubPrim<T> add_cone(
    Model&   model,
    Vec<T,3> center,
    Vec<T,3> axis,
    T        radius,
    size_t   segments)
{
    SubPrim<T> out;
    // make a cone by unfolding triangles around the tip
    size_t vert_offset = model.verts.size();
    size_t idx_offset  = model.indices.size();
    
    T h = axis.mag();
    Vec<T,3> v = axis / h;
    AffineTransform<T,3> rot = geom::direction_align({0,0,1}, v);
    
    // compute the vertices
    out.bounds |= center + axis;
    for (int j = 0; j < segments; ++j) {
        float t = ((T) j) / segments;
        T theta = t * 2 * std::numbers::pi_v<T> * j;
        Vec<T,2> xy  = {std::cos(theta), std::sin(theta)};
        Vec<T,3> p   = radius * (rot * Vec<T,3>{xy, 0}) + center;
        Vec<T,3> n   = rot * Vec<T,3>{h * xy, radius}.unit();
        // edge pt
        model.verts.push_back({
            .p  = p,
            .n  = n,
            .uv = 0.5 * xy + 0.5,
        });
        // tip pt (repeated for unique normal)
        model.verts.push_back({
            .p = center + axis,
            .n = n,
            .uv = {0.5, 0.5},
        });
        out.bounds |= p;
    }
    
    // compute the indices
    for (size_t k = 0; k < segments; ++k) {
        // current section
        gpu_size_t i0 = vert_offset + 2 * k;
        gpu_size_t i1 = vert_offset + 2 * ((k + 1) % segments);
        gpu_size_t i2 = i0 + 1;
        
        model.indices.insert(model.indices.end(), {
            i0, i1, i2,
        });
    }
    
    out.index_range |= {
        (gpu_size_t) idx_offset,
        (gpu_size_t) model.indices.size() - 1,
    };
    
    // add the base cap
    out |= add_disk(model, center, -v, radius, segments);
    
    return out;
}


template <typename T>
SubPrim<T> add_path(
        Model& model,
        const Vec<T,3>* pts,
        size_t n,
        size_t section_segments,
        Thickness<T> thickness,
        PathCap<T> start_cap = CapType::None,
        PathCap<T> end_cap   = CapType::None)
{
    SubPrim<T> out;
    // todo: cut off the end of the path if the cap is an arrow
    //   and the arrow tip is inside the path
    std::vector<Vec<T,3>> section_verts (section_segments);
    for (int i = 0; i < section_segments; ++i) {
        T theta = (2 * std::numbers::pi_v<T> * i) / section_segments;
        section_verts[i] = {std::cos(theta), std::sin(theta), 0};
    }
    size_t vert_offset = model.verts.size();
    size_t idx_offset  = model.indices.size();
    
    // reserve space for the new vertices and indices
    model.verts.reserve(vert_offset + n * section_segments);
    // n - 1 segments, times quads in a segment, times indices in a quad (6, for two tris)
    model.indices.reserve(idx_offset + (n - 1) * section_segments * 6);
    
    // compute the vertices
    for (index_t i = 0; i < n; ++i) {
        T s = ((T) i) / (n - 1);
        // tangent
        Vec<T,3> v;
        if      (i == 0)     v = pts[1]     - pts[0];
        else if (i == n - 1) v = pts[i]     - pts[i - 1];
        else                 v = pts[i + 1] - pts[i - 1];
        Vec<T,3> p = pts[i];
        
        // radius for this section
        T r = 1;
        if (std::holds_alternative<T>(thickness)) {
            r = std::get<T>(thickness);
        } else {
            r = std::get<const T*>(thickness)[i];
        }
        
        // place the points for this section
        AffineTransform<T,3> rot = geom::direction_align({0,0,1}, v.unit());
        for (int j = 0; j < section_segments; ++j) {
            Vec<T,3> p_i = rot * section_verts[j];
            Vec<T,3> p_v = p + r * p_i;
            model.verts.push_back({
                .p  = p_v,
                .n  = p_i,
                .uv = {s, j / (T)section_segments},
            });
            out.bounds |= p_v;
        }
    }
    
    // compute the indices (one fewer segments than curve points)
    for (index_t i = 0; i < n - 1; ++i) {
        size_t j = vert_offset + i * section_segments;
        for (index_t k = 0; k < section_segments; ++k) {
            // current section
            gpu_size_t i0 = j + k;
            gpu_size_t i1 = j + (k + 1) % section_segments;
            // next section
            gpu_size_t i2 = i0 + section_segments;
            gpu_size_t i3 = i1 + section_segments;
            model.indices.insert(model.indices.end(), {
                i0, i1, i3,
                i0, i3, i2,
            });
        }
    }
    
    out.index_range |= {
        (gpu_size_t) idx_offset,
        (gpu_size_t) model.indices.size() - 1,
    };
    
    // add caps if requested
    PathCap<T> caps[2] = {start_cap, end_cap};
    for (int i = 0; i < 2; ++i) {
        PathCap<T> c = caps[i];
        if (std::holds_alternative<CapType>(c) and 
            std::get<CapType>(c) == CapType::None) {
                continue;
        }
        Vec<T,3> v;
        if (i == 0) v = pts[0]     - pts[1];
        else        v = pts[n - 1] - pts[n - 2];
        Vec<T,3> p = pts[i == 0 ? 0 : n - 1];
        T thick;
        if (std::holds_alternative<const T*>(thickness)) {
            thick = std::get<const T*>(thickness)[i == 0 ? 0 : n - 1];
        } else {
            thick = std::get<T>(thickness);
        }
        if (std::holds_alternative<ArrowheadCap<T>>(c)) {
            ArrowheadCap<T> ac = std::get<ArrowheadCap<T>>(c);
            v = v.with_length(ac.length);
            out |= add_cone<T>(
                model,
                p,
                v,
                ac.thickness,
                section_segments
            );
        } else if (std::holds_alternative<BoxCap<T>>(c)) {
            // todo: not implemented
        } else if (std::holds_alternative<DotCap<T>>(c)) {
            DotCap<T> dc = std::get<DotCap<T>>(c);
            out |= add_sphere_cap<T>(
                model,
                {std::numbers::pi_v<T>},
                p,
                v,
                dc.radius,
                2 * section_segments,
                section_segments
            );
        } else {
            CapType c_t = std::get<CapType>(c);
            switch (c_t) {
                case CapType::Round: {
                    constexpr T right_angle = std::numbers::pi_v<T> / 2;
                    out |= add_sphere_cap<T>(
                        model,
                        {right_angle},
                        p,
                        v,
                        thick,
                        section_segments,
                        section_segments
                    );
                } break;
                case CapType::Flat: {
                    out |= add_disk<T>(model, p, v, thick, section_segments);
                } break;
                case CapType::None: break;
            }
        }
    }
    
    return out;
}

template <typename T>
SubPrim<T> add_arrow(Model& model, Vec<T,3> p0, Vec<T,3> p1, T thickness) {
    T d = p0.dist(p1);
    T max_arrowhead_fraction = 1 / (T)6;
    T arrow_aspect = 2; // length / width
    T thickness_aspect = 3;
    T arrow_width  = std::min(thickness * thickness_aspect, d * max_arrowhead_fraction / arrow_aspect);
    T arrow_length = arrow_aspect * arrow_width;
    T r = std::min(thickness, arrow_width / thickness_aspect);
    T arrowhead_fraction = arrow_length / d;
    Vec<T,3> path[2] = {
        p0,
        geom::mix(arrowhead_fraction, p1, p0),
    };
    return add_path(
        model,
        path,
        2,
        12,
        thickness,
        CapType::Flat,
        ArrowheadCap<T>{arrow_width * 2, arrow_width}
    );
}

template <typename T>
SubPrim<T> add_box(Model& model, const Rect<T,3>& box) {
    size_t vert_offset = model.verts.size();
    size_t idx_offset  = model.indices.size();
    model.verts.reserve(vert_offset + 16);
    model.indices.reserve(idx_offset + 36);
    
    Vec<T,3> cs[2] = {box.lo, box.hi};
    int faces = 0;
    for (int axis = 0; axis < 3; ++axis) {
        size_t x_axis = (axis + 1) % 3;
        size_t y_axis = (axis + 2) % 3;
        // front and back faces
        for (int sign = -1; sign <= 1; sign += 2) {
            // each of the four corners for this face
            for (int i = 0; i < 4; ++i) {
                Vec<T,3> p;
                int x = (i >> 0) & 1;
                int y = (i >> 1) & 1;
                p[axis]   = cs[sign > 0][axis];
                p[x_axis] = cs[x][x_axis];
                p[y_axis] = cs[y][y_axis];
                Vec<T,3> n;
                n[axis] = sign;
                model.verts.push_back({
                    .p  = p,
                    .n  = n,
                    .uv = {x, y},
                });
            }
            // add the two triangles for this face
            gpu_size_t j = vert_offset + 4 * faces;
            if (sign > 0) {
                model.indices.insert(model.indices.end(), {
                    j, j + 1, j + 3,
                    j, j + 3, j + 2,
                });
            } else {
                model.indices.insert(model.indices.end(), {
                    j, j + 3, j + 1,
                    j, j + 2, j + 3,
                });
            }
            faces += 1;
        }
    }
    return {
        .index_range = {
            (gpu_size_t) idx_offset,
            (gpu_size_t) model.indices.size() - 1,
        },
        .bounds = box,
    };
}
    
}  // namespace stereo
