// structs.wgsl
// common structs across multiple stages of stereo fusion

struct FeatureRange {
    feature_start: u32,
    feature_end:   u32,
}

struct SceneFeature {
    q:     vec4f,
    q_cov: mat4x4f,
    x:     vec3f,
    x_cov: mat3x3f,
    scale: f32, // radius of the feature in world space
    wt:    f32,
    
    // xxx debug
    color: vec3f,
}

struct TreeNode {
    parent:      u32,
    // global index of first child, if extant
    child_begin: u32,
    // off-end global child index
    child_end:   u32,
}

struct CorrelationKernel {
    correlation: mat4x4f, // unnormalized correlation
    mag2:        f32,     // product of magnitude^2 of all the kernels
}

struct ImageFeature {
    // texture-space coordinates
    st:    vec2f,
    // texture-space covariance matrix
    cov:   mat2x2f,
    // window basis (in texture space)
    basis: mat2x2f,
}

struct FeaturePair {
    a: ImageFeature,
    b: ImageFeature,
}

struct DebugFeature2D {
    x:     vec2f,
    sigma: mat2x2f,
    q:     f32,
}

struct MatcherUniforms {
    tree_depth:       i32,
    feature_offset:   u32,
    refine_parent:    bool,
    branching_factor: i32,
}

struct WeightedSample {
    x: vec2f,
    w: f32,
    f: f32,
}

// a SampleKernel is a 4x4 pixel patch from the source image.
// the samples are taken from the [-1,1]^2 patch of the feature's local coordinate system.
// the edge samples are at |x| = 1 exactly.
struct SampleKernel {
    r: mat4x4f,
    g: mat4x4f,
    b: mat4x4f,
}

struct KernelPair {
    a: SampleKernel,
    b: SampleKernel,
}

struct LensParameters {
    fov_radians: f32,
    aspect:      f32,
    x_c:         vec2f, // x_c (center of distortion)
    p_12:        vec2f, // p1, p2 (tangential distortion)
    k_1_3:       vec3f, // k1 ... k3 (radial distortion coeffs)
}

// camera coordinates are opengl convention:
// +x right, +y up, -z forward.
struct CameraState {
    x:      vec3f,  // position of camera in world space
    q:      vec4f,  // rotation that orients the camera in world space
    lens:   LensParameters,
}

struct RigidEstimate {
    x:   vec3f,  // position
    q:   vec4f,  // orientation
    cov: array<f32, 28>, // 7x7 covariance matrix, upper tri (tx.xyz | q.ijkw)^2
}
