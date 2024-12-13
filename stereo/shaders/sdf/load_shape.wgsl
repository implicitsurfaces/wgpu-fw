// #include "sdf_structs.wgsl"

// todo: would be nice to do this automatically from the struct definition

// xxx problem: wgpu does not support storage arrays passed as
//   function parameters, so we have to access via global variables >:(
//   expects sdf_params_x, sdf_params_dx to be visible in the global scope.

fn load_scalard(offs: ParamOffset) -> Dual {
    return Dual(
        sdf_params_x [offs.x_offset],
        sdf_params_dx[offs.dx_offset],
    );
}

fn load_vecd(offs: ParamOffset) -> DualV3 {
    let i: Index = offs.x_offset;
    let j: Index = offs.dx_offset;
    let x: vec3f = vec3f(
        sdf_params_x[i + 0],
        sdf_params_x[i + 1],
        sdf_params_x[i + 2],
    );
    let dx: vec3f = vec3f(
        sdf_params_dx[j + 0],
        sdf_params_dx[j + 1],
        sdf_params_dx[j + 2],
    );
    return DualV3(x, dx);
}

fn load_quatd(offs: ParamOffset) -> DualQ {
    let i: Index = offs.x_offset;
    let j: Index = offs.dx_offset;
    let q: vec4f = vec4f(
        sdf_params_x[i + 0],
        sdf_params_x[i + 1],
        sdf_params_x[i + 2],
        sdf_params_x[i + 3],
    );
    let dq: vec4f = vec4f(
        sdf_params_dx[j + 0],
        sdf_params_dx[j + 1],
        sdf_params_dx[j + 2],
        sdf_params_dx[j + 3],
    );
    return DualQ(q, dq);
}

fn load_transformd(offs:  ParamOffset) -> RigidTransformD {
    let q:  DualQ  = load_quatd(offs);
    let p2: ParamOffset = ParamOffset(offs.x_offset + 4, offs.dx_offset + 4);
    let tx: DualV3 = load_vecd(p2);
    return RigidTransformD(q, tx);
}

fn load_boxd(offs:  ParamOffset) -> BoxD {
    let lo: DualV3 = load_vecd(offs);
    let hi: DualV3 = load_vecd(ParamOffset(offs.x_offset + 3, offs.dx_offset + 3));
    return BoxD(lo, hi);
}

fn load_sphered(offs:  ParamOffset) -> SphereD {
    let center: DualV3 = load_vecd(offs);
    let radius: Dual   = load_scalard(ParamOffset(offs.x_offset + 3, offs.dx_offset + 3));
    return SphereD(center, radius);
}

fn load_cylinderd(offs:  ParamOffset) -> CylinderD {
    let p0: DualV3 = load_vecd(offs);
    let p1: DualV3 = load_vecd(ParamOffset(offs.x_offset + 3, offs.dx_offset + 3));
    let radius: Dual = load_scalard(ParamOffset(offs.x_offset + 6, offs.dx_offset + 6));
    return CylinderD(p0, p1, radius);
}

fn load_capsuled(offs:  ParamOffset) -> CapsuleD {
    let p0: DualV3 = load_vecd(offs);
    let p1: DualV3 = load_vecd(ParamOffset(offs.x_offset + 3, offs.dx_offset + 3));
    let radius: Dual = load_scalard(ParamOffset(offs.x_offset + 6, offs.dx_offset + 6));
    return CapsuleD(p0, p1, radius);
}

fn load_planed(offs:  ParamOffset) -> PlaneD {
    let n: DualV3 = load_vecd(offs);
    let d: Dual   = load_scalard(ParamOffset(offs.x_offset + 3, offs.dx_offset + 3));
    return PlaneD(n, d);
}

fn load_trid(offs:  ParamOffset) -> TriD {
    let p0: DualV3 = load_vecd(offs);
    let p1: DualV3 = load_vecd(ParamOffset(offs.x_offset + 3, offs.dx_offset + 3));
    let p2: DualV3 = load_vecd(ParamOffset(offs.x_offset + 6, offs.dx_offset + 6));
    return TriD(p0, p1, p2);
}

// todo: more shape types
