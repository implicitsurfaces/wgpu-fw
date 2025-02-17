#pragma once

#include <geomc/linalg/Vec.h>
#include <geomc/linalg/Matrix.h>
#include <geomc/shape/Frustum.h>

#include <stereo/defs.h>

enum struct ProjectionType {
    Perspective,
    Orthographic
};


namespace stereo {

/**
 * @brief Compute an orthographic projection matrix.
 *
 * The computed matrix sends points in camera coordinates to NDC coordinates.
 *
 * The Z-axis is the view direction, and the Y-axis is the up direction.
 * The near and far planes are at negative Z values, with the near plane being
 * at `box.hi.z`.
 *
 * @param box Region to render.
 * @return A world-to-NDC matrix.
 */
inline mat4 ortho_projection(const range3& box) {
    float l =  box.lo.x;
    float r =  box.hi.x;
    float b =  box.lo.y;
    float t =  box.hi.y;
    float n = -box.hi.z; // least negative
    float f = -box.lo.z; // most negative
    float w = r - l;
    float h = t - b;
    float d = f - n;
    return {
        2 / w, 0,  0, -(r + l) / w,
        0, 2 / h,  0, -(t + b) / h,
        0, 0, -2 / d, -(f + n) / d,
        0, 0, 0, 1
    };
}

/**
 * @brief A camera is a perspective viewpoint of the scene.
 *
 * This class carries transform and projection information. (It does not do
 * any OpenGL rendering calls by itself).
 *
 * An un-transformed camera is looking down the Z- axis, with X+ to camera right, and
 * Y+ to camera-up. (The coordinate system is right-handed).
 */
template <typename T=float>
struct Camera {

    using rangez    = geom::Rect<T,1>;
    using range2z   = geom::Rect<T,2>;
    using range3z   = geom::Rect<T,3>;
    using frustum3z = geom::Frustum<range2z>;
    using xf3z      = geom::AffineTransform<T,3>;
    using vec2z     = geom::Vec<T,2>;
    using vec3z     = geom::Vec<T,3>;
    using vec4z     = geom::Vec<T,4>;
    using mat3z     = geom::SimpleMatrix<T,3,3>;
    using mat4z     = geom::SimpleMatrix<T,4,4>;
    using ray3z     = geom::Ray<T,3>;

    using Projection = std::variant<frustum3z, range3z>;

    /// Transform taking a camera-space point to world coordinates.
    /// (Can also be considered the "position and orientation" of the camera).
    xf3z cam_to_world;
    /// Frustum pointing down Z-. The height range should be negative, with the upper
    /// limit being the near plane.
    Projection frustum;

    ProjectionType projection_type() const {
        return std::holds_alternative<frustum3z>(frustum) ?
            ProjectionType::Perspective :
            ProjectionType::Orthographic;
    }

    /**
     * @brief Set perspective projection parameters.
     *
     * @param fov_degrees Horizontal field of view (full angle) in degrees.
     * @param aspect Aspect ratio of the frame (width / height).
     * @param near Positive distance to the near plane.
     * @param far Positive distance to the far plane.
     */
    void set_perspective(T fov_degrees, T aspect, T near, T far) {
        T tan_fov = std::tan(M_PI * fov_degrees / 360); // half angle
        T w       = 2 * tan_fov;
        frustum   = frustum3z {
            range2z::from_center({0.}, {w, w / aspect}),
            rangez(-far, -near)
        };
    }

    /**
     * @brief Set orthographic projection parameters.
     *
     * The provided box represents the extents of the viewing area in camera space.
     * The Z- axis is the viewing direction, and the Y+ axis is the up direction. The far
     * plane is at the lowest Z extent of the box, and the near plane at the highest.
     */
    void set_orthographic(const range3z& view_box) {
        frustum = view_box;
    }

    vec3z get_look() const {
        return cam_to_world.apply_direction(vec3z(0,0,-1));
    }

    /// Positive distance to the near plane.
    T near() const {
        // least negative Z coordinate is the near plane
        if (std::holds_alternative<frustum3z>(frustum)) {
            const frustum3z& f = std::get<frustum3z>(frustum);
            return -f.clipped_height().hi;
        } else {
            const range3z& b = std::get<range3z>(frustum);
            return b.hi.z;
        }
        static_assert(
            std::variant_size_v<Projection> == 2,
            "Expected two alternatives for projection type"
        );
    }

    /// Positive distance to the far plane.
    T far() const {
        // most negative Z coordinate is the far plane
        if (std::holds_alternative<frustum3z>(frustum)) {
            const frustum3z& f = std::get<frustum3z>(frustum);
            return -f.clipped_height().lo;
        } else {
            const range3z& b = std::get<range3z>(frustum);
            return -b.lo.z;
        }
        static_assert(
            std::variant_size_v<Projection> == 2,
            "Expected two alternatives for projection type"
        );
    }

    /**
     * @brief Position the camera at the location `from` and point it toward `at`.
     *
     * @param at Point to look toward.
     * @param from Position of the camera.
     * @param up Approximate up direction.
     */
    void look_at(vec3z at, vec3z from, vec3z up) {
        // todo: untested
        vec3z look  = from - at;
        vec3z right = up   ^ look;
        up          = look ^ right;
        // todo: handle degenerate case where look == up
        vec3z mx[]  = {right.unit(), up.unit(), look.unit()}; // a column basis...
        mat3z orient(&mx[0][0]);       // ...but this is row major...
        transpose(&orient, orient);    // ...so we do this.
        // point the camera the right way, then position it
        // (recall that orientations are applied outside-to-inside)
        cam_to_world = translation(from) * transformation(orient);
    }

    /**
     * @brief Orbit a point about an axis.
     *
     * @param at Center of orbit; camera will look towards this point.
     * @param radius Distance from the center of orbit to the camera's position
     * @param alt_radians Altitude angle from equatorial plane, radians.
     * Positive values look from a higher angle.
     * @param azi_radians Azimuth angle from x+, radians.
     * @param up The axis of the orbit.
     */
    void orbit(vec3z at, T radius, T alt_radians, T azi_radians, vec3z up) {
        // remember: transformations are applied outside-to-inside
        cam_to_world =
            // translate the center of view
              geom::translation(at)
            // realign the entire gimbal system's polar axis
            * geom::direction_align(vec3z{0,1,0}, up)
            // spin the camera's position (somewhere above z+) to its azimuth angle
            * geom::rotation_y(azi_radians)
            // rotate the camera's position (somewhere along z+) to its proper altitude
            * geom::rotation_x(-alt_radians)
            // translate the camera from (0,0,0) "backwards" along z+
            * geom::translation((T) 0, (T) 0, radius);
    }

    /**
     * @brief Return a ray from the camera's near plane into the scene,
     * originating at the given point in NDC coordinates ([-1 : 1] in x and y).
     */
    ray3z ndc_to_world_ray(vec2z ndc_near_point) const {
        vec2z p_01 = ndc_near_point / 2 + 0.5;
        if (std::holds_alternative<frustum3z>(frustum)) {
            const frustum3z& fm = std::get<frustum3z>(frustum);
            // near plane at Z upper bound.
            vec3z p_s = -vec3z{fm.base.remap(p_01), 1};
            T near_z = fm.clipped_height().hi;
            vec3z p_dir = near_z * p_s;
            return cam_to_world * ray3z(p_dir, p_dir);
        } else {
            const range3z& b = std::get<range3z>(frustum);
            vec3z p_s = b.remap({p_01, 1}); // near plane at upper Z bound
            return cam_to_world * ray3z(p_s, vec3z(0,0,-1));
        }
        static_assert(
            std::variant_size_v<Projection> == 2,
            "Expected two alternatives for variant size"
        );
    }

    /**
     * @brief Return a ray from the camera's near plane into the scene,
     * originating at the given point in aspect-normalized coordinates
     * (i.e., coordinates ranging from -1 to 1 in x, and a proportional
     * zero-centered range in y).
     */
    ray3z aspect_to_world_ray(vec2z aspect_point) const {
        vec2z dims;
        if (std::holds_alternative<frustum3z>(frustum)) {
            const frustum3z& fm = std::get<frustum3z>(frustum);
            dims = fm.base.dimensions();
        } else {
            const range3z& box = std::get<range3z>(frustum);
            dims = box.dimensions().template resized<2>();
        }
        T aspect = dims.x / dims.y;
        static_assert(
            std::variant_size_v<Projection> == 2,
            "Expected two alternatives for variant size"
        );
        // width is 2 (-1 to 1); therefore height is 2 * aspect
        vec2d p_ndc = aspect_point * vec2d{1, 2 * aspect};
        return ndc_to_world_ray(p_ndc);
    }

    /**
     * @brief Compute the projection matrix.
     */
    mat4z compute_projection() const {
        if (std::holds_alternative<frustum3z>(frustum)) {
            // todo: this could be simplified by using the frustum's base directly
            const frustum3z& fm = std::get<frustum3z>(frustum);
            auto h_range = fm.clipped_height();
            T n = -h_range.hi; // least negative
            T f = -h_range.lo; // most negative
            T l = n * fm.base.lo.x;
            T r = n * fm.base.hi.x;
            T b = n * fm.base.lo.y;
            T t = n * fm.base.hi.y;
            T w = r - l;
            T h = t - b;
            T d = f - n;
            return {
                2 * n / w, 0, (r + l) / w, 0,
                0, 2 * n / h, (t + b) / h, 0,
                0, 0, -(f + n) / d, -2 * f * n / d,
                0, 0, -1, 0
            };
        } else {
            return ortho_projection(std::get<range3z>(frustum));
        }
        static_assert(
            std::variant_size_v<Projection> == 2,
            "Expected two alternatives for variant size"
        );
    }

    /**
     * @brief Convert a point in NDC coordinates to camera-space coordinates.
     */
    vec3z unproject_cam(vec3z ndc) const {
        mat4z inv_proj;
        inv(&inv_proj, compute_projection());
        vec4z cam = inv_proj * vec4z(ndc, 1.f);
        return cam.template resized<3>() / cam.w;
    }
};

} // namespace stereo
