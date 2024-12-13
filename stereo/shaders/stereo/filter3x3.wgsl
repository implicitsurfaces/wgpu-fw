@group(0) @binding(0) var src_tex: texture_2d<f32>;

@group(1) @binding(0) var r_tex: texture_storage_2d<rgba8snorm,write>;
@group(1) @binding(1) var g_tex: texture_storage_2d<rgba8snorm,write>;
@group(1) @binding(2) var b_tex: texture_storage_2d<rgba8snorm,write>;

@group(2) @binding(0) var<uniform> mip_level: i32;

// a row major packing of all the souce texel values.
// the first row is the top row.
var<private> src_texels: array<vec3f, 9>;

struct DxDy {
    dx : vec3f,
    dy : vec3f,
};

fn fill_window(c: vec2<u32>) {
    for (var row: u32 = 0u; row < 3u; row++) {
        for (var col: u32 = 0u; col < 3u; col++) {
            // +y is up; +row is "down"; treat row like +y
            let xy: vec2<u32> = c + vec2<u32>(col - 1u, 1u - row);
            src_texels[row * 3u + col] = textureLoad(src_tex, xy, mip_level).rgb;
        }
    }
}

fn sobel_feldman(m: array<vec3f,9>) -> DxDy {
    let dx: vec3f =
        -3. * m[0] + -10. * m[3] + -3. * m[6] +
         3. * m[2] +  10. * m[5] +  3. * m[8];
    let dy: vec3f =
         3. * m[0] +  10. * m[1] +  3. * m[2] +
        -3. * m[6] + -10. * m[7] + -3. * m[8];
    return DxDy(dx, dy);
}

fn laplace(m: array<vec3f,9>) -> vec3f {
    // weights:
    //  0.25 0.5 0.25
    //  0.5  -3   0.5
    //  0.25 0.5 0.25
    return
        0.25 * m[0] + 0.5 * m[1] + 0.25 * m[2] +
        0.5  * m[3] - 3.  * m[4] + 0.5  * m[5] +
        0.25 * m[6] + 0.5 * m[7] + 0.25 * m[8];
}

@compute @workgroup_size(8,8)
fn filter_main(@builtin(global_invocation_id) id: vec3<u32>) {
    var a: f32 = 1. / f32(1u << u32(mip_level));
    var b: f32 = 16. * a;
    fill_window(id.xy);
    var f:     vec3f = src_texels[4];
    var dx_dy:  DxDy = sobel_feldman(src_texels);
    var del:   vec3f = b * laplace(src_texels);
    dx_dy.dx *= a;
    dx_dy.dy *= a;
    textureStore(r_tex, id.xy, vec4f(f.r, dx_dy.dx.r, dx_dy.dy.r, del.r));
    textureStore(g_tex, id.xy, vec4f(f.g, dx_dy.dx.g, dx_dy.dy.g, del.g));
    textureStore(b_tex, id.xy, vec4f(f.b, dx_dy.dx.b, dx_dy.dy.b, del.b));
}
