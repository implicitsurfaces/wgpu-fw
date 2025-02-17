// #include "material.wgsl"

struct V {
    @location(0) x:  vec3f,
    @location(1) n:  vec3f,
    @location(2) st: vec2f,
}

struct VOut {
    @builtin(position) p:        vec4f,
    @location(0)       p_world:  vec3f,
    @location(1)       n_world:  vec3f,
    @location(2)       st: vec2f,
}

struct ObjectUniforms {
    obj2w:   mat4x4f,
    w2obj:   mat4x4f,
    overlay: OverlayColor,
}

struct CameraUniforms {
    w2cam:    mat4x4f,
    cam2w:    mat4x4f,
    P:        mat4x4f,
    exposure: f32,
}

struct Lighting {
    key_light: DistantLight,
    amb_light: AmbientLight,
}

// global binding
@group(0) @binding(0) var<uniform> cam_uniforms: CameraUniforms;
@group(0) @binding(1) var<uniform> lighting:     Lighting;
@group(0) @binding(2) var          u_sampler:    sampler;

// material binding
@group(1) @binding(0) var<uniform> u_material: MaterialCoeffs;
@group(1) @binding(1) var t_diffuse:   texture_2d<f32>;
@group(1) @binding(2) var t_specular:  texture_2d<f32>;
@group(1) @binding(3) var t_emission:  texture_2d<f32>;
@group(1) @binding(4) var t_roughness: texture_2d<f32>;
@group(1) @binding(5) var t_metallic:  texture_2d<f32>; // todo: unused
@group(1) @binding(6) var t_alpha:     texture_2d<f32>;
@group(1) @binding(7) var t_normal:    texture_2d<f32>; // todo: unused

// object binding
@group(2) @binding(0) var<uniform> object_uniforms: ObjectUniforms;


////// entry points //////

@vertex
fn vs_main(v: V) -> VOut {
    var out: VOut;
    let B: mat4x4f = object_uniforms.obj2w;
    let C: mat4x4f = object_uniforms.w2obj;
    let N: mat3x3f = transpose(mat3x3f(C[0].xyz, C[1].xyz, C[2].xyz));
    let p_world: vec4f = object_uniforms.obj2w * vec4f(v.x, 1.);
    out.p_world = p_world.xyz;
    out.p       = cam_uniforms.P * cam_uniforms.w2cam * p_world;
    out.n_world = normalize(N * v.n);
    out.st      = v.st;
    return out;
}

@fragment
fn fs_main(
        v: VOut,
        @builtin(front_facing) is_front: bool
    ) -> @location(0) vec4f {
    // view vector, from P to eye
    let V: vec3f = normalize(cam_uniforms.cam2w[3].xyz - v.p_world);
    var normal   = normalize(v.n_world);
    if !is_front {
        normal = -normal;
    }
    // sample material
    let k_diff:  vec3f = u_material.k_diff  * textureSample(t_diffuse,   u_sampler, v.st).rgb;
    let k_spec:  vec3f = u_material.k_spec  * textureSample(t_specular,  u_sampler, v.st).rgb;
    let k_emit:  vec3f = u_material.k_emit  * textureSample(t_emission,  u_sampler, v.st).rgb;
    let k_rough: f32   = u_material.k_rough * textureSample(t_roughness, u_sampler, v.st).r;
    let k_metal: f32   = u_material.k_metal * textureSample(t_metallic,  u_sampler, v.st).r;
    let alpha:   f32   = u_material.alpha   * textureSample(t_alpha,     u_sampler, v.st).r;
    let material_sample: MaterialCoeffs = MaterialCoeffs(
        k_diff, k_spec, k_emit, k_rough, k_metal, alpha
    );
    // let normal      = normalize(textureSample(t_normal, sampler, v.st).xyz * 2. - 1.);
    // for now, just use the normal from the vertex shader; we're not dealing with
    // tangent spaces yet
    
    // compute illumination
    var c = illuminate_distant(material_sample, V, normal, lighting.key_light);
    c += illuminate_amb(material_sample, V, normal, lighting.amb_light);
    
    // apply overlay color
    let overlay: OverlayColor = object_uniforms.overlay;
    c = c * overlay.tint.rgb + overlay.additive.rgb * overlay.additive.a;
    c = overlay.over.a * overlay.over.rgb + (1. - overlay.over.a) * c;
    c = pow(2., cam_uniforms.exposure) * c;
    c = pow(c, vec3(2.2));
    return vec4f(c, alpha);
}
