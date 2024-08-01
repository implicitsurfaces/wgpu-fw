// structs.wgsl
// common structs across multiple stages of stereo fusion

struct CorrelationWindow {
    correlation: mat4x4,
}

struct ImageFeature {
    st:        vec2f,
    sqrt_cov:  mat2x2f,
    basis:     mat2x2f,
}

struct FeaturePair {
    id:     u32,
    depth:  u32,
    parent: u32,
    a:      ImageFeature,
    b:      ImageFeature,
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
