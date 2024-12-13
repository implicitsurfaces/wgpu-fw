// #include "sdf_structs.wgsl"
// #include "sdf_shapes.wgsl"
// #include "sdf_ops.wgsl"
// #include "load_shape.wgsl"

// >>>>>>> ALERT: NONSTANDARD INCLUDE BEHAVIOR <<<<<<<
// because wgpu doesn't support passing storage arrays as fn parameters,
// we have to resort to using named global variables. this file
// must be included _below_ the definitions of the ops and params arrays,
// which must be named `sdf_tree`, `sdf_params_x`, and `sdf_params_dx` respectively.

struct SdfContext {
    x:   SdfDomain,
    f_x: SdfRange,
}

// @id(1000) override STACK_SIZE: u32 = 16u; // (incorrectly) not supported in wgpu
const STACK_SIZE: u32 = 16u;

fn sdf_eval(
    offsets:   ParamOffset,
    sample_pt: SdfDomain) -> SdfContext
{
    var p_stack:   array<SdfDomain, STACK_SIZE>;
    var f_p_stack: array<SdfRange,  STACK_SIZE>;
    var p_size:    u32 = 1;
    var f_p_size:  u32 = 0;
    p_stack[0] = sample_pt;
    for (var i: u32 = 0; i < arrayLength(&sdf_tree); i++) {
        var op = sdf_tree[i];
        var is_pop: bool = false;
        if op.kind == OpEnum_PopDomain {
            p_size -= 1u;
            // `variant` holds the pushed op's index in this case:
            let pushed_idx: u32 = u32(op.variant);
            op      = sdf_tree[pushed_idx];
            is_pop  = true;
        }
        let offs = ParamOffset(
            offsets.x_offset  + op.parameter_begin,
            offsets.dx_offset + op.parameter_begin
        );
        let p: SdfDomain = p_stack[p_size - 1];
        switch op.kind {
            case OpEnum_PopDomain: {
                p_size -= 1u;
            }
            // transformations
            case OpEnum_Union: {
                let f_a: SdfRange = f_p_stack[f_p_size - 2];
                let f_b: SdfRange = f_p_stack[f_p_size - 1];
                f_p_stack[f_p_size - 2] = sdf_union(f_a, f_b);
                f_p_size -= 1u;
            }
            case OpEnum_Intersect: {
                let f_a: SdfRange = f_p_stack[f_p_size - 2];
                let f_b: SdfRange = f_p_stack[f_p_size - 1];
                f_p_stack[f_p_size - 2] = sdf_intersection(f_a, f_b);
                f_p_size -= 1u;
            }
            case OpEnum_Subtract: {
                let f_a: SdfRange = f_p_stack[f_p_size - 2];
                let f_b: SdfRange = f_p_stack[f_p_size - 1];
                f_p_stack[f_p_size - 2] = sdf_subtract(f_a, f_b);
                f_p_size -= 1u;
            }
            case OpEnum_Transform: {
                let xf: RigidTransformD = load_transformd(offs);
                if is_pop {
                    // transform the normal
                    // nb: the normal is transformed by the inverse transpose
                    // of the rotation part; but since the rotation is orthogonal,
                    // the inverse transpose is the same as the original matrix
                    let f_p = f_p_stack[f_p_size - 1];
                    let n   = dqv_mul(xf.q, f_p.grad_f);
                    f_p_stack[f_p_size - 1].grad_f = n;
                } else {
                    // inverse transform the domain
                    p_stack[p_size] = sdf_transform(xf, p);
                    p_size += 1u;
                }
            }
            case OpEnum_Dilate: {
                let r: Dual = load_scalard(offs);
                f_p_stack[f_p_size - 1] = sdf_dilate(f_p_stack[f_p_size - 1], r);
            }
            // shapes
            case OpEnum_Sphere: {
                let sphere = load_sphered(offs);
                f_p_stack[f_p_size] = sdf_sphere(sphere, p);
                f_p_size += 1u;
            }
            case OpEnum_Box: {
                let box = load_boxd(offs);
                f_p_stack[f_p_size] = sdf_box(box, p);
                f_p_size += 1u;
            }
            case OpEnum_Cylinder: {
                let cylinder = load_cylinderd(offs);
                f_p_stack[f_p_size] = sdf_cylinder(cylinder, p);
                f_p_size += 1u;
            }
            case OpEnum_Capsule: {
                let capsule = load_capsuled(offs);
                f_p_stack[f_p_size] = sdf_capsule(capsule, p);
                f_p_size += 1u;
            }
            case OpEnum_Plane: {
                let plane = load_planed(offs);
                f_p_stack[f_p_size] = sdf_plane(plane, p);
                f_p_size += 1u;
            }
            case OpEnum_Triangle: {
                let triangle = load_trid(offs);
                f_p_stack[f_p_size] = sdf_triangle(triangle, p);
                f_p_size += 1u;
            }
            
            default: {
                // not implemented yet
                continue;
            }
        }
    }
    
    return SdfContext(
        p_stack[p_size - 1],
        f_p_stack[f_p_size - 1]
    );   
}
