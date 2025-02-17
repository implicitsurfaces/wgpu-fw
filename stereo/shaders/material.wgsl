// todo: I think not all energy-conserving terms are implemented

// order of ops: tint, add, over
struct OverlayColor {
    tint:     vec4f,
    additive: vec4f,
    over:     vec4f,
}

struct MaterialCoeffs {
    k_diff:  vec3f,
    k_spec:  vec3f,
    k_emit:  vec3f,
    k_rough: f32,
    k_metal: f32,
    alpha:   f32,
}

// point source at infinity
struct DistantLight {
    intensity: f32,
    dir:   vec3f,
    color: vec3f,
}

// bi-color gradient over the sphere of directions
struct AmbientLight {
    intensity: f32,
    axis:    vec3f, // axis of color gradient
    color_a: vec3f, // color along +axis
    color_b: vec3f, // color along -axis
}

const PI: f32 = 3.1415926535897932384;


////// material functions //////

// generalized Trowbridge-Reitz microfacet specular model
// (also known as GGX)
fn gtr_specular(roughness: f32, gamma: f32, NdotH: f32) -> f32 {
    let a_2: f32 = roughness * roughness;
    let NdotH_2: f32 = NdotH * NdotH;
    let sin_theta_2: f32 = 1. - NdotH_2;
    if abs(gamma - 1.) < 0.001 {
        // there is a singularity at gamma; use alternate form:
        let c: f32 = (a_2 - 1.) / (PI * log(a_2));
        return c / (1. + (a_2 - 1.) * NdotH_2);
    } else {
        let c: f32 = (gamma - 1.) * (a_2 - 1.) / (PI * 1. - pow(a_2, 1. - gamma));
        return c / pow(NdotH_2 * (a_2 - 1.) + 1., gamma);
    }
}

// Cook-Torrance microfacet specular model
// (same as GTR with gamma=2)
fn ggx_specular(roughness: f32, NdotH: f32) -> f32 {
    let a_2: f32 = roughness * roughness;
    let NdotH_2: f32 = NdotH * NdotH;
    let c: f32 = a_2 / PI;
    let k: f32 = 1. + (a_2 - 1.) * NdotH_2;
    return c / (k * k);
}

// schlick approximation to fresnel coeff
fn schlick_fresnel(V_dot_H: f32, k_n: f32) -> f32 {
    var R_0: f32 = (1. - k_n) / (1. + k_n);
    R_0 = R_0 * R_0;
    return R_0 + (1. - R_0) * pow(1. - V_dot_H, 5.);
}

fn cos_theta(a: vec3f, b: vec3f) -> f32 {
    return max(dot(a, b), 0.);
}

// compute illumination from a distant light
fn illuminate_distant(
    material: MaterialCoeffs,
    V: vec3f, // unit view vector; from P to eye in world space
    N: vec3f, // unit normal at P in world space
    light: DistantLight,
    ) -> vec3f
{
    let I: vec3f = light.intensity * light.color;
    let L: vec3f = normalize(light.dir);
    let H: vec3f = normalize(V + L);
    let NdotL:  f32 = max(dot(N, L), 0.); // cos(angle N to L)
    let NdotH:  f32 = max(dot(N, H), 0.); // cos(half angle N to L)
    let VdotH:  f32 = max(dot(V, H), 0.); // cos(half angle V to L)
    let k_fres: f32 = schlick_fresnel(VdotH, 1.5);
    // diffuse
    var c:    vec3f = I * material.k_diff * NdotL;
    // specular
    c = mix(
        c,
        I * ggx_specular(material.k_rough, NdotH),
        material.k_spec * k_fres
    );
    return c;
}

// compute the color of the ambient light in a particular direction
fn amb_color(light: AmbientLight, v: vec3f) -> vec3f {
    return light.intensity * mix(
        light.color_b,
        light.color_a,
        0.5 * dot(v, light.axis) + 0.5
    );
}

// compute illumination from an ambient light
fn illuminate_amb(
    material: MaterialCoeffs,
    V: vec3f,       // unit view vector; from P to eye in world space
    N: vec3f,       // unit normal at P in world space,
    light: AmbientLight
    ) -> vec3f
{
    let R: vec3f = reflect(-V, N);
    let VdotN: f32 = cos_theta(V, N); // N is the half-vector in the reflection case
    // diffuse
    var c: vec3f = material.k_diff * amb_color(light, N);
    // specular
    let reflected_color: vec3f = amb_color(light, R);
    let k_fres: f32 = schlick_fresnel(VdotN, 1.5);
    c = mix(
        c,
        reflected_color,
        material.k_spec * k_fres,
    );
    return c;
}
