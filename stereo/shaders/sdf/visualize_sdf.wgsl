// #include "sdf_structs.wgsl"

// sdf expression
@group(0) @binding(0) var<storage,read> sdf_tree:      array<SdfOp>;
@group(0) @binding(1) var<storage,read> sdf_params_x:  array<f32>;
@group(0) @binding(2) var<storage,read> sdf_params_dx: array<f32>;

// included here so it can see the expression buffers above:
// #include "sdf_eval.wgsl"

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0)       uv:       vec2f,
}

fn sdf_color(f: Dual) -> vec3f {
    let d: f32 = f.x;
    var col: vec3f = select(vec3f(0.9, 0.6, 0.3), vec3f(0.6, 0.8, 1.), d < 0.);
    col *= 1.0 - exp(-9. * abs(d));
	col *= 1.0 + 0.2 * cos(128. * abs(d));
	col = mix(col, vec3(1.0), 1. - smoothstep(0., 0.015, abs(d)));
	return col;
}

@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> VertexOutput {
    // emit a quad
    var v = array<vec2f, 4>(
        vec2f(-1,  1),
        vec2f(-1, -1),
        vec2f( 1,  1),
        vec2f( 1, -1)
    );
    var p = v[in_vertex_index % 4];
    var out: VertexOutput;
    out.position = vec4f(p, 0.5, 1.0);
    out.uv = p * 0.5 + 0.5;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    var st = 2. * in.uv - vec2f(1.); // -1 to 1
    st = st * 4;
    let x: SdfDomain = SdfDomain(
        DualV3(vec3f(st, 0.), vec3f(0.)),
        dm3_identity(),
    );
    let param_index: ParamOffset = ParamOffset(0, 0);
    let result: SdfContext = sdf_eval(param_index, x);
    // let result = SdfContext(
    //     x,
    //     sdf_union(
    //         sdf_sphere(
    //             SphereD(DualV3(vec3f(-1., 0., 0.), vec3f()), Dual(1., 0.)),
    //             x
    //         ),
    //         sdf_sphere(
    //             SphereD(DualV3(vec3f( 1., 0., 0.), vec3f()), Dual(1., 0.)),
    //             x
    //         ),
    //     )
    // );
    let c: vec3f = sdf_color(result.f_x.f);
    return vec4f(c * c, 1.0);
}
