struct MipUniforms {
    src_gamma: f32,
    dst_gamma: f32,
}

@group(0) @binding(0) var<uniform> mip_uniforms: MipUniforms;

@group(1) @binding(0) var previousMipLevel: texture_2d<f32>;
@group(1) @binding(1) var nextMipLevel: texture_storage_2d<rgba8unorm,write>;

@compute @workgroup_size(8, 8)
fn compute_mipmap(@builtin(global_invocation_id) id: vec3<u32>) {
    let offset = vec2<u32>(0u, 1u);
    let decode_gamma = vec4f(vec3f(     mip_uniforms.src_gamma), 1.);
    let encode_gamma = vec4f(vec3f(1. / mip_uniforms.dst_gamma), 1.);
    var samples: array<vec4f, 4> = array<vec4f, 4>(
        textureLoad(previousMipLevel, 2u * id.xy + offset.xx, 0),
        textureLoad(previousMipLevel, 2u * id.xy + offset.xy, 0),
        textureLoad(previousMipLevel, 2u * id.xy + offset.yx, 0),
        textureLoad(previousMipLevel, 2u * id.xy + offset.yy, 0),
    );
    if (mip_uniforms.src_gamma != 1.) {
        for (var i: u32 = 0u; i < 4u; i = i + 1u) {
            samples[i] = pow(samples[i], decode_gamma);
        }
    }
    var color = (
        samples[0] + samples[1] + samples[2] + samples[3]
    ) * 0.25;
    if (mip_uniforms.dst_gamma != 1.) {
        color = pow(color, encode_gamma);
    }
    textureStore(nextMipLevel, id.xy, color);
}
