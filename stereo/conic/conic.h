#pragma once

#include <geomc/linalg/AffineTransform.h>

using namespace geom;

namespace stereo {

// todo: degeneracies (plane through origin, etc)

/**
 * A conic is a linear transformation of the unit cone.
 *
 * All conics have their vertex at the origin. The unit cone has a unit circle cross section
 * on the homogeneous plane (z=1), centered on the Z axis.
 * 
 * Other conics are rotations, shears, and scalings of the unit cone. The part of the conic
 * that intersects the z=1 plane is the conic section figure for that conic.
 */
template <typename T>
struct Conic {
    /**
     * @brief Matrix of the conic.
     * 
     * The conic is the set of points `v` such that `v^T * M^T * Q * M * v = 0`,
     * where Q is `diag(1, 1, -1)`.
     * 
     * This matrix moves points from the z=1 plane to the plane which slices the
     * unit cone.
     */
    SimpleMatrix<T,3,3> M_fwd;
    
    /**
     * @brief Inverse of the matrix of the conic.
     *
     * This matrix moves points from the plane which slices the unit cone to the
     * z=1 plane. It may also be thought of as the transform that distorts the
     * unit cone to this conic.
     */
    SimpleMatrix<T,3,3> M_inv;
    
    /**
     * @brief Evaluate the conic's implicit equation at a point on the homogeneous plane.
     */
    T operator()(Vec<T,2> p) const {
        Vec<T,3> v {p.x, p.y, 1};
        Vec<T,3> Av = Vec<T,3>(1, 1,-1) * M_fwd * v;
        return v.dot(transpose(M_fwd) * Av);
    }
    
    /**
     * @brief Compute the matrix of the quadratic form of the conic.
     *
     * The value of the implicit equation is given by `v^T * B * v` where `v` is the
     * point at which to evaluate the conic.
     */
    SimpleMatrix<T,3,3> quadratic_matrix() const {
        constexpr SimpleMatrix<T,3,3> Q = {
            1,  0,  0,
            0,  1,  0,
            0,  0, -1
        };
        return transpose(M_fwd) * Q * M_fwd;
    }
    
    /**
     * @brief Project a point on the homogeneous plane of the transformed unit cone
     * onto the homogeneous plane of this conic.
     */
    Vec<T,2> from_unit_cone(Vec<T,2> p) const {
        // the same transform that moves the slice plane to the
        // homogeneous plane also moves the unit cone to this conic.
        // take a point in unit cone space, move it to conic space,
        // then project it back to the homogeneous plane.
        Vec<T,3> p_conic = M_inv * Vec<T,3> {p.x, p.y, 1};
        return p_conic.template resized<2>() / p_conic.z;
    }
    
    /**
     * @brief Slice the conic with a plane.
     *
     * The provided plane will become the homogeneous plane of the new conic.
     *
     * `n` is a the point on the plane closest to the origin, which
     * is normal to the plane.
     */
    Conic<T> slice(Vec<T,3> n) const {
        T m = n.mag();
        Vec<T,3> n_hat = n / m;
        // we pre-apply a transform to the point which moves the slice plane
        // to the homogeneous plane (z=1). first we rotate the normal onto
        // the z axis, then we scale the z coordinate to 1.
        SimpleMatrix<T,3,3> R = geom::rotmat_direction_align(n_hat, {0,0,1});
        return {
            M_fwd * (1 / m) * R,
            m * transpose(R) * M_inv,
        };
    }
};

/**
 * @brief Apply an affine transform to the figure on the homogeneous plane of a Conic,
 * yielding new Conic.
 */
template <typename T>
Conic<T> operator*(const AffineTransform<T,2>& xf, const Conic<T>& c) {
    return {
        c.M_fwd * xf.inv,
        xf.mat  * c.M_inv,
    };
}

/**
 * @brief Apply an inverse affine transform to the figure on the homogeneous plane of a Conic,
 * yielding new Conic.
 */
template <typename T>
Conic<T> operator/(const Conic<T>& c, const AffineTransform<T,2>& xf) {
    return {
        c.M_fwd * xf.mat,
        xf.inv  * c.M_inv,
    };
}

template <typename T>
Conic<T> operator*(const SimpleMatrix<T,3,3>& m, const Conic<T>& c) {
    bool ok = true;
    Conic<T> out = {
        c.M_fwd * inv(m, &ok),
        m * c.M_inv,
    };
    if (not ok) return c;
    return out;
}

template <typename T>
Conic<T> operator/(const Conic<T>& c, const SimpleMatrix<T,3,3>& m) {
    bool ok = true;
    Conic<T> out = {
        c.M_fwd * m,
        inv(m, &ok) * c.M_inv,
    };
    if (not ok) return c;
    return out;
}

} // namespace stereo
