// sdf_eval_main.wgsl
// #include "sdf_structs.wgsl"

struct WorkRange {
    n_samples:    u32,
    n_variations: u32,
}

const wg_size: vec3u = vec3u(8,8,1);

// expression
@group(0) @binding(0) var<storage,read> sdf_tree:      array<SdfOp>;
@group(0) @binding(1) var<storage,read> sdf_params_x:  array<f32>;
@group(0) @binding(2) var<storage,read> sdf_params_dx: array<f32>;

// samples
@group(1) @binding(0) var<storage,read> sdf_pts_x:     array<vec3f>;
@group(1) @binding(1) var<storage,read> sdf_pts_dx:    array<vec3f>;

// output
@group(2) @binding(0) var<storage,read_write> sdf_out_sdf_x:     array<f32>;
@group(2) @binding(1) var<storage,read_write> sdf_out_sdf_dx:    array<f32>;
@group(2) @binding(2) var<storage,read_write> sdf_out_normals:   array<vec3f>;
@group(2) @binding(3) var<storage,read_write> sdf_out_d_normals: array<vec3f>;

// offsets
@group(3) @binding(0) var<storage,read> pt_offsets:    array<ParamOffset>;
@group(3) @binding(1) var<storage,read> param_offsets: array<ParamOffset>;
@group(3) @binding(2) var<uniform>      work_range:    WorkRange;

// >>>>>>> ALERT: NONSTANDARD INCLUDE BEHAVIOR <<<<<<<
// we must include this here so that its contents can see the storage arrays above,
// because wgpu does not (yet) support passing storage arrays as function parameters:

// #include "sdf_eval.wgsl"

// >>>>>>> !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! <<<<<<<

@compute @workgroup_size(wg_size.x, wg_size.y, wg_size.z)
fn main(
        @builtin(workgroup_id)           workgroup_id: vec3u,
        @builtin(local_invocation_index) local_index:  u32,
        @builtin(global_invocation_id)   global_id:    vec3u)
{
    // x-axis: sample point variation
    // y-axis: parameter variation
    if global_id.x >= work_range.n_samples    { return; }
    if global_id.y >= work_range.n_variations { return; }
    let param_index: ParamOffset = param_offsets[global_id.y];
    var pt_index:    ParamOffset = pt_offsets[global_id.y];
    pt_index.x_offset  += global_id.x;
    pt_index.dx_offset += global_id.x;
    
    let p: DualV3 = DualV3(
        sdf_pts_x [pt_index.x_offset],
        sdf_pts_dx[pt_index.dx_offset],
    );
    
    let domain: SdfDomain = SdfDomain(
        p,
        dm3_identity(),
    );
    let result: SdfContext = sdf_eval(param_index, domain);
    
    // compute the final index
    let threads_per_workgroup: u32 = wg_size.x * wg_size.y * wg_size.z;
    let workgroup_index: u32 = workgroup_id.x
        + workgroup_id.y * wg_size.x
        + workgroup_id.z * wg_size.x * wg_size.y;
    let out_index: u32 = workgroup_index * threads_per_workgroup + local_index;
    
    // write the output
    let sdf: SdfRange = result.f_x;
    sdf_out_sdf_x[out_index]   = sdf.f.x;
    sdf_out_sdf_dx[out_index]  = sdf.f.dx;
    sdf_out_normals[out_index] = sdf.grad_f.x;
    sdf_out_normals[out_index] = sdf.grad_f.dx;
}
