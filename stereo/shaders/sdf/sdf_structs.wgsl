// sdf_structs.wgsl
// common structs across multiple shaders/stages for sdf computation

// #include "../dual3.wgsl"

// todo: dual quaternions
//   https://faculty.sites.iastate.edu/jia/files/inline-files/dual-quaternion.pdf
// todo: finish sdfs for shapes

alias OpEnum     = u32;
alias Index      = u32;
alias OpVariant  = u32;
alias ParamArray = ptr<storage,array<f32>,read>;

// range operations:
const OpEnum_PopDomain:   OpEnum =  0; // ✓
const OpEnum_Union:       OpEnum =  1; // ✓
const OpEnum_Intersect:   OpEnum =  2; // ✓
const OpEnum_Subtract:    OpEnum =  3; // ✓
const OpEnum_Xor:         OpEnum =  4;
const OpEnum_Shell:       OpEnum =  5;
const OpEnum_Dilate:      OpEnum =  6; // ✓
// domain operations:
const OpEnum_Transform:   OpEnum =  7; // ✓
const OpEnum_Revolve:     OpEnum =  8;
const OpEnum_Repeat:      OpEnum =  9;
const OpEnum_Mirror:      OpEnum = 10;
const OpEnum_RotSym:      OpEnum = 11;
const OpEnum_Extrude:     OpEnum = 12;
const OpEnum_Envelope:    OpEnum = 13;
const OpEnum_Elongate:    OpEnum = 14;
const OpEnum_CurveSweep:  OpEnum = 15;
const OpEnum_Helix:       OpEnum = 16;
// shapes:
const OpEnum_Sphere:      OpEnum = 100; // ✓
const OpEnum_Box:         OpEnum = 101; // ✓
const OpEnum_Cylinder:    OpEnum = 102; // ✓
const OpEnum_Capsule:     OpEnum = 103; // ✓
const OpEnum_Cone:        OpEnum = 104;
const OpEnum_Torus:       OpEnum = 105;
const OpEnum_Plane:       OpEnum = 106; // ✓
const OpEnum_Triangle:    OpEnum = 107; // ✓
const OpEnum_Curve:       OpEnum = 108;
const Openum_ClosedCurve: OpEnum = 109;

const OpVariant_None      = 0;
// envelope / sweep curve classes:
const OpVariant_Linear    = 1;
const OpVariant_Quadratic = 2;
const OpVariant_Cubic     = 3;
const OpVariant_Arc       = 4;
// smoothness for unions/intersections:
const OpVariant_Smooth    = 5;
// revolve / symmetry axes:
const OpVariant_X_Axis    = 6;
const OpVariant_Y_Axis    = 7;
const OpVariant_Z_Axis    = 8;

struct SdfOp {
    kind:            OpEnum,
    // may also keep integer parameters, or index of popped expression:
    variant:         OpVariant,
    parameter_begin: Index,
    parameter_end:   Index,
}

struct SdfDomain {
    p: DualV3,
    J: DualM3,
}

struct SdfRange {
    f:      Dual,   // value of sdf
    grad_f: DualV3, // gradient of sdf
}

struct ParamOffset {
    x_offset:   Index,
    dx_offset: Index,
}

// shape structures:

struct BoxD {
    lo: DualV3,
    hi: DualV3,
}

struct SphereD {
    center: DualV3,
    radius: Dual,
}

struct CylinderD {
    p0:     DualV3,
    p1:     DualV3,
    radius: Dual,
}

struct CapsuleD {
    p0:     DualV3,
    p1:     DualV3,
    radius: Dual,
}

struct PlaneD {
    n: DualV3, // normalized
    d: Dual,
}

struct TriD {
    p0: DualV3,
    p1: DualV3,
    p2: DualV3,
}

struct TorusD {
    major_radius: Dual,
    minor_radius: Dual,
}

struct ConeD {
    radius: Dual,
    height: Dual,
}

struct IntervalD {
    lo: Dual,
    hi: Dual,
}

struct RigidTransformD {
    // a rigid transform is first a rotation,
    // then a translation
    q:  DualQ,
    tx: DualV3,
}
