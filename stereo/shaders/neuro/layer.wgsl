// #include "neuro_structs.wgsl"
// #include "../complex.wgsl"

// todo: sort by edge destination to minimize concurrency issues
// or use atomics? see https://github.com/gpuweb/gpuweb/issues/4894

// observation: computing the cdf is the same cost as just
// computing the influence of every edge
// - it only matters if we want to do sparse/deep backpropagation, then
//   we can use less than the full network cheaply, and share the cdf
//   across many samples
// - would we have to keep long-term activations, or could we sparsify that also?

struct LayerUniforms {
    batch_width: u32,
}

// brain definition
@group(0) @binding(0) var<storage,read> edge_weights: array<complex>;
@group(0) @binding(1) var<storage,read> edges:        array<Edge>;
@group(0) @binding(2) var<uniform> layer_uniforms:    LayerUniforms;

// sample set for this frame
@group(1) @binding(0) var<storage,read> edge_indices: array<u32>;
@group(1) @binding(1) var<storage,read> pdf:          array<f32>;
// @group(1) @binding(2) var<storage,read> cdf:          array<f32>;

// input and output activations
@group(2) @binding(0) var<storage,read> input: array<complex>;

@group(3) @binding(0) var<storage,write> output: array<complex>;

fn atomic_add(out: ptr<storage,complex,read_write>, value: complex) {
    // xxx todo: this
}

@compute @workgroup_size(32)
fn gather_layer(@builtin(global_invocation_id) gid: vec3u) {
    let base_index: u32 = gid.x * layer_uniforms.batch_width;
    if base_index >= arrayLength(&edge_indices) { return; }

    for (var i = 0u; i < layer_uniforms.batch_width; i = i + 1u) {
        if (base_index + i >= arrayLength(&edge_indices)) { break; }
        let edge_index: u32 = edge_indices[base_index + i];
        let edge:      Edge = edges[edge_index];
        let weight: complex = weights[edge_index];
        let input_activation: complex = input[edge.n0];
        let output_activation = output[edge.n1];
        let pdf = pdfs[base_index + i];

        // update output activation
        let ds: complex = c_mul(weight, input_activation);
    }
}

@compute @workgroup_size(32)
fn apply_activation(@builtin(global_invocation_id) gid: vec3u) {

}
