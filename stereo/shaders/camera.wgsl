// #include "structs.wgsl"
// #include "kalman.wgsl"
// #include "quat.wgsl"

// todo: account for lens distortion.
//   best way to do this:
//   - use the forward distortion model to resample the source
//     image into rectilinear or stereographic space.
//   - apply the map distortion as part of the projection, and apply its
//     jacobian by the chain rule.

// R^2 -> R^2 P and jacobian
struct Dx2x2 {
    f_x: vec2f,
    J_f: mat2x2f,
}

// R^3 -> R^2 P and jacobian
struct Dx3x2 {
    f_x: vec2f,
    J_f: mat3x2f,
}

// R^3 -> R^3 P and jacobian
struct Dx3x3 {
    f_x: vec3f,
    J_f: mat3x3f,
}

// R^4 -> R^2 P and jacobian
struct Dx4x2 {
    f_x: vec2f,
    J_f: mat4x2f,
}

// project camera space to image space (on [0,1]^2)
// image center is (0.5, 0.5); origin at the lower left.
fn pinhole_projection(lens: LensParameters) -> mat3x3f {
    let t:   f32 = 2. * tan(lens.fov_radians / 2.);
    let f_x: f32 = 1. / t;
    let f_y: f32 = f_x * lens.aspect;
    return mat3x3f(
        vec3f(f_x,  0.,  0.),
        vec3f(0.,  f_y,  0.),
        vec3f(-lens.x_c, -1.), // -lens.x_c is negative because Z is negative
    );
}

// compute the world-to-camera matrix from a camera state.
// camera space is Y-up, X-right, Z-backward.
fn world_to_cam(cam: CameraState) -> mat4x3f {
    let Q:  mat3x3f = qvmat(qconj(cam.q)); // invert cam2world for world2cam
    // world2cam transform; i.e. puts a world point in camera space
    let T:  vec3f   = -cam.x;
    return mat4x3f(Q[0], Q[1], Q[2], Q * T);
}

// take an X in camera space and project it to the stereographic image plane.
// return the projected point and its jacobian about the camera space point.
fn J_project_stereographic_dp(x: vec3f) -> Dx3x2 {
    let m:     f32 = length(x);
    let p:   vec3f = x / m;
    let uv:  vec2f = p.xy / (1. - p.z);
    let k:     f32 = 1. / (m * (x.z - m));
    let zzp:   f32 = x.z * (x.z - m);
    let q:     f32 = 1. / dot(uv, uv);
    let J: mat3x2f = mat3x2f(
        k * vec2f(x.y * x.y + zzp, x.x * x.y),
        k * vec2f(x.x * x.y, x.x * x.x + zzp),
        q * vec2f(x.x + x.y * x.z / m, x.y + x.y * x.z / m),
    );
    return Dx3x2(uv, J);
}

// take a point in camera space and project it to the image plane
// using the given camera state. return the jacobian of the projection
// about the camera space point.
fn J_project_dp(cam: CameraState, p: vec3f) -> Dx3x2 {
    let M: mat4x3f = world_to_cam(cam);            // world-to-camera matrix
    let P: mat3x3f = pinhole_projection(cam.lens); // projection matrix
    let Q: mat4x3f = P * M;
    let x_clip: vec3f = Q * vec4f(p, 1.);
    let x: vec2f = x_clip.xy / x_clip.z;
    
    let J: mat3x2f = ( 
        transpose(mat2x3f(x.x * Q[3], x.y * Q[3]))
        + mat3x2f(Q[0].xy, Q[1].xy, Q[2].xy)
    ) * (1. / x_clip.z);
    
    return Dx3x2(x, J);
}

fn J_world_to_cam_dp(cam: CameraState) -> mat3x3f {
    let M: mat4x3f = world_to_cam(cam);
    return mat3x3f(M[0], M[1], M[2]);
}

// projection with jacobian with respect to the camera position
fn J_world_to_cam_dtx(cam: CameraState) -> mat3x3f {
    var one: vec3f = vec3f(1., 0., 0.);
    // inverse bc camera point is negated in the world_to_cam transform
    return mat3x3f(-one.xyy, -one.yxy, -one.yyx);
}

// projection with jacobian with respect to the camera orientation
fn J_world_to_cam_dq(cam: CameraState, p: vec3f) -> mat3x4f {
    return J_qvmult_dq(qconj(cam.q), p - cam.x);
}

fn project(cam: CameraState, p: vec3f) -> vec2f {
    let P: mat3x3f = pinhole_projection(cam.lens);
    let x:   vec3f = qrot(qconj(cam.q), p - cam.x);
    let x_clip: vec3f = P * x;
    return x_clip.xy / x_clip.z;
}

fn project_scene_feature(cam: CameraState, f: SceneFeature) -> ImageFeature {
    let p: vec3f   = f.x;
    let M: mat4x3f = world_to_cam(cam);
    let J: Dx3x2   = J_project_dp(cam, M * vec4f(p, 1));
    let P: mat3x2f = J.J_f * mat3x3f(M[0], M[1], M[2]);
    let R: mat2x2f = P * f.x_cov * transpose(P);
    // todo: more robustly compute the basis.
    //   Q: What actually drives the basis scale?
    //     > the window needs to be big enough to encapsulate the covariance, but
    //     > the window also needs to have a size that encompasses the feature
    //       - needs to be able to correlate with previous frame
    //       - needs to encompass all the child freatures.
    //     > the above suggests that "semantic size" is what matters, so the Q is then
    //       "what do we do about excessive state uncertainty?"
    //       - vague idea: push it up to a higher level somehow and rely on the
    //         coarser parent to narrow the search
    //         > this is only a problem at the root level, because children are confined to
    //           have uncertainty less than the parent's window by construction
    //         > don't much like this because there's no guarantee the feature is
    //           resolvable at the parent level if there isn't an existing parent.
    //           could get "attached" to a totally unrelated feature
    //         > there will not be a corresponding feature in the previous frame,
    //           unless we backpropagate or some BS. yeah, don't like this.
    //       - vague idea: particle filter that shit. spawn a bunch of new trees
    //         in the expanded search space; rely on quality results to reveal the
    //         true result
    let area: f32 = sqrt(determinant(R));
    let d:    f32 = 2 * sqrt(area);
    return ImageFeature(
        J.f_x,
        R,
        // axis-aligned basis, with the area of the 2-SD covariance ellipsoid
        mat2x2f(
            d,  0.,
            0., d,
        ),
    );
}

fn unproject_kalman_view_difference(
    prior:         Estimate3D,
    measured_diff: Estimate2D,
    cam_a:         CameraState,
    cam_b:         CameraState) -> Estimate3D
{
    // we have only an estimate of the difference of the projections of the
    // point in the two camera views. we can incorporate this difference
    // into the projection matrix though, and treat the whole "project twice
    // and difference" as a single projection of the 3D state.
    let p_a: Dx3x2 = J_project_dp(cam_a, prior.x);
    let p_b: Dx3x2 = J_project_dp(cam_b, prior.x);
    // transpose so we can vector-select rows
    let a_H: mat2x3f = transpose(p_a.J_f);
    let b_H: mat2x3f = transpose(p_b.J_f);
    let prior_diff: vec2f = p_b.f_x - p_a.f_x;
    // u_diff = u_b.uv - u_a.uv
    let P_H: mat3x2f = transpose(
        mat2x3f(b_H[0] - a_H[0], b_H[1] - a_H[1])
    );
    return update_ekf_unproject_2d_3d(prior, prior_diff, measured_diff, P_H);
}
