// #include "stereo_structs.wgsl"

@group(0) @binding(0) var<storage,read_write> img_features: array<FeaturePair>;
@group(0) @binding(1) var<storage,read>       dx_array:     array<DebugFeature2D>;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) global_id: vec3u) {
    if global_id.x >= arrayLength(&img_features) || global_id.x >= arrayLength(&dx_array) {
        return;
    }
    let feature: FeaturePair = img_features[global_id.x];
    let dx: vec2f = dx_array[global_id.x].x;
    let est_feature = ImageFeature(
        feature.b.st + feature.b.basis * dx,
        feature.b.cov,
        feature.b.basis,
    );
    img_features[global_id.x] = FeaturePair(feature.a, est_feature);
}
