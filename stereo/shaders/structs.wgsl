// structs.wgsl
// common structs across multiple stages of stereo fusion

struct SceneFeature {
    q:     vec4f,
    q_cov: mat4x4f,
    x:     vec3f,
    x_cov: mat3x3f,
    wt:    f32,
}

struct TreeNode {
    parent:      u32,
    child_begin: u32,
    child_end:   u32,
}

struct CorrelationWindow {
    correlation: mat4x4f,
}

struct ImageFeature {
    st:    vec2f,
    cov:   mat2x2f,
    basis: mat2x2f,
}

struct FeaturePair {
    a: ImageFeature,
    b: ImageFeature,
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
}

struct SampleWindow {
    r: mat4x4f,
    g: mat4x4f,
    b: mat4x4f,
}

struct WindowPair {
    a: SampleWindow,
    b: SampleWindow,
}

struct LensParameters {
    fov_radians: f32,
    aspect:      f32,
    x_c:         vec2f, // x_c (center of distortion)
    p_12:        vec2f, // p1, p2 (tangential distortion)
    // we can't do array<f32,6> because there are draconian stride requirements.
    k_1_4: vec4f, // k1 ... k4 (radial distortion coeffs)
    k_5_6: vec2f, // k5, k6 (remaining radial distortion coeffs)
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
