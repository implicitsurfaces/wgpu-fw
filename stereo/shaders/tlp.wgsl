// #include "structs.wgsl"
// #include "mat_helpers.wgsl"

// top-level predict shader

struct TlpUniforms {
    dt:            f32,
    process_scale: f32,
}

@group(0) @binding(0) var<uniform> uniforms: TlpUniforms;
@group(1) @binding(0) var<storage, read>       feature_idx_buffer: array<u32>;
@group(2) @binding(0) var<storage, read_write> features:           array<SceneFeature>;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) global_id: vec3u) {
    let index_number: u32 = global_id.x;
    if index_number >= arrayLength(&feature_idx_buffer) { return; }
    let feature_idx: u32 = feature_idx_buffer[index_number];
    if feature_idx >= arrayLength(&features) { return; }
    
    let src_feature: SceneFeature = features[feature_idx];
    let k: f32 = uniforms.dt * src_feature.scale * uniforms.process_scale;
    let process_noise: mat3x3f = k * I_3x3;
    features[feature_idx].x_cov = src_feature.x_cov + process_noise;
}
