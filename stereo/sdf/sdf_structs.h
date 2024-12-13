#pragma once

#include <stereo/defs.h>

using namespace geom;

namespace stereo {

// keep in sync with sdf_structs.wgsl
enum struct SdfOp: uint32_t {
    // range operations:
    PopDomain = 0,
    Union = 1,
    Intersect,
    Subtract,
    Xor,
    Shell,
    Dilate,
    // domain-transforming operations:
    Transform,
    Revolve,
    Repeat,
    Mirror,
    RotSym,
    Extrude,
    Envelope,
    Elongate,
    CurveSweep,
    Helix,
    // shapes:
    Sphere = 100,
    Box,
    Cylinder,
    Capsule,
    Cone,
    Torus,
    Plane,
    Triangle,
    Curve,
    ClosedCurve,
};

enum struct SdfOpVariant: uint32_t {
    None = 0,
    // curve types
    Linear = 1,
    Quadratic,
    Cubic,
    Arc,
    // smoothness for union / intersection
    Smooth,
    // revolve / symmetry axes:
    XAxis,
    YAxis,
    ZAxis,
};

template <typename T>
struct SdfNode;

template <typename T>
using SdfNodeRef = std::shared_ptr<SdfNode<T>>;

template <typename T>
struct SdfNode {
private:
    SdfOp _op;

public:
    
    SdfNode(SdfOp op) : _op(op) {}
    virtual ~SdfNode() {}
    
    SdfOp op() const { return _op; }
    
    virtual size_t        n_children()        const { return 0;       }
    virtual SdfNodeRef<T> child(size_t i)     const { return nullptr; }
    virtual size_t        n_params()          const { return 0;       }
    virtual const T*      params()            const { return nullptr; }
    virtual const SdfOpVariant variant()      const { return SdfOpVariant::None; }
    virtual bool          transforms_domain() const { return false;    }
    
};

template <typename T, SdfOp Op>
struct SdfBinop : public SdfNode<T> {
private:
    SdfNodeRef<T> _children[2];
    
public:
    
    SdfBinop(SdfNodeRef<T> a, SdfNodeRef<T> b):SdfNode<T>(Op), _children{a, b} {}
    size_t n_children() const override { return 2; }
    SdfNodeRef<T> child(size_t i) const override { return _children[i]; }
};

template <typename T, SdfOp Op>
struct SdfUnop : public SdfNode<T> {
private:
    SdfNodeRef<T> _child;
public:
    SdfUnop(SdfNodeRef<T> c): SdfNode<T>(Op), _child(c) {}
    size_t n_children() const override { return 1; }
    SdfNodeRef<T> child(size_t i) const override { return _child; }
};

template <typename T, SdfOp Op>
struct SdfScalarOp : public SdfUnop<T, Op> {
    T s;
    SdfScalarOp(SdfNodeRef<T> c, T scalar): SdfUnop<T, Op>(c), s(scalar) {}
    size_t n_params() const override { return 1; }
    const T* params() const override { return &s; }
};

template <SdfOp Op, typename Shape, size_t M>
struct SdfShape : public SdfNode<typename Shape::elem_t> {
    // assumption: `Shape` is densely packed scalar values of type `T`.
    // (this is true for all the geomc shapes)
    using T = typename Shape::elem_t;
    Shape shape;
    
    SdfShape(const Shape& shape): SdfNode<T>(Op), shape(shape) {}
    size_t n_params() const override { return M; }
    const T* params() const override { return reinterpret_cast<const T*>(&shape); }
};

// unparameterized sdf binops:
template <typename T> struct SdfUnion:     public SdfBinop<T, SdfOp::Union> {
    using SdfBinop<T, SdfOp::Union>::SdfBinop;
};
template <typename T> struct SdfIntersect: public SdfBinop<T, SdfOp::Intersect> {
    using SdfBinop<T, SdfOp::Intersect>::SdfBinop;
};
template <typename T> struct SdfSubtract:  public SdfBinop<T, SdfOp::Subtract> {
    using SdfBinop<T, SdfOp::Subtract>::SdfBinop;
};
template <typename T> struct SdfXor:       public SdfBinop<T, SdfOp::Xor> {
    using SdfBinop<T, SdfOp::Xor>::SdfBinop;
};

// parameterized sdf unops:
template <typename T> struct SdfShell:     public SdfScalarOp<T, SdfOp::Shell> {
    using SdfScalarOp<T, SdfOp::Shell>::SdfScalarOp;
};
template <typename T> struct SdfDilate:    public SdfScalarOp<T, SdfOp::Dilate> {
    using SdfScalarOp<T, SdfOp::Dilate>::SdfScalarOp;
};

// bare shapes:
template <typename T> struct SdfSphere:    public SdfShape<SdfOp::Sphere,   Sphere<T,3>,   4> {
    using SdfShape<SdfOp::Sphere, Sphere<T,3>, 4>::SdfShape;
};
template <typename T> struct SdfBox:       public SdfShape<SdfOp::Box,      Rect<T,3>,     6> {
    using SdfShape<SdfOp::Box, Rect<T,3>, 6>::SdfShape;
};
template <typename T> struct SdfCylinder:  public SdfShape<SdfOp::Cylinder, Cylinder<T,3>, 7> {
    using SdfShape<SdfOp::Cylinder, Cylinder<T,3>, 7>::SdfShape;
};
template <typename T> struct SdfCapsule:   public SdfShape<SdfOp::Capsule,  Capsule<T,3>,  7> {
    using SdfShape<SdfOp::Capsule, Capsule<T,3>, 7>::SdfShape;
};
template <typename T> struct SdfPlane:     public SdfShape<SdfOp::Plane,    Plane<T,3>,    4> {
    using SdfShape<SdfOp::Plane, Plane<T,3>, 4>::SdfShape;
};

// explicit parameterized ops:
template <typename T> struct SdfTransform: public SdfUnop<T, SdfOp::Transform> {
    Quat<T>  q;
    Vec<T,3> tx;
    
    SdfTransform(SdfNodeRef<T> child, Quat<T> q, Vec<T,3> tx):
        SdfUnop<T,SdfOp::Transform>(child), q(q), tx(tx) {}
    
    const size_t n_params() const override { return 7; }
    const T*     params()   const override { return &q[0]; }
    const bool   transforms_domain() const override { return true; }
};

template <typename T>
struct SdfTriangle : public SdfNode<T> {
    // Simplex is too general (it can have 0 <= n <= 4 vertices)
    // so we specify three points explicitly:
    Vec<T,3> pts[3];
    
    SdfTriangle(const Vec<T,3> vertices[3]):
        SdfNode<T>(SdfOp::Triangle),
        pts{vertices[0], vertices[1], vertices[2]} {}
    
    const size_t n_params() const override { return 9; }
    const T*     params()   const override { return &pts[0][0]; }
};


}  // namespace stereo
