#include <iostream>
#include <thread>
#include <chrono>
#include <random>

#include <stereo/util/dumb_argparse.h>
#include <stereo/gpu/render_pipeline.h>
#include <stereo/gpu/shader.h>
#include <stereo/gpu/window.h>
#include <stereo/app/solver.h>
#include <stereo/app/visualize.h>
#include <stereo/app/virtual_scene.h>
#include <stereo/app/make_scene.h>

using namespace stereo;
using namespace std::chrono_literals;

// todo: mip generation does not handle edges correctly

static CaptureRef _get_capture(int index) {
    CaptureRef cap = std::make_shared<cv::VideoCapture>(index);
    while (not cap->isOpened()) {
        // this can happen if the host's security policy requires
        // user approval to start the camera. wait for it to be ready.
        cap->open(index);
        std::this_thread::sleep_for(100ms);
    }
    // cap->grab();
    return cap;
}

static DeviceFrameSourceRef _get_camera_source(SolverRef solver, CaptureRef cap) {
    return std::make_shared<DeviceFrameSource>(solver->mip_generator(), solver->filter(), cap);
}

static int64_t _time_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

enum struct SourceMode {
    Single,
    Dual,
    Split,
    Virtual,
};

// hacky gvars. too lazy to make the callbacks work better
bool     should_run        = false;
SourceMode source_mode     = SourceMode::Single;
ViewMode view_mode         = ViewMode::Splat;
StepMode idle_mode         = StepMode::Fuse;
bool     should_reinit     = false;
int      depth_change      = 0;
bool     should_undisplace = false;
bool     one_fr            = false;

// calibrated with `camcal.py`:
CameraState _logitech_720p_cam = {
    // .lens = {
    //     .aspect      = 1280 / 720.,
    //     .fov_radians = 0.79314559, // ~45.5 degrees
    //     .k_c         = vec2(0.5050f, 0.4939f),
    //     // these values seem to change a lot between calibration runs?
    //     .k_1_3       = vec3(0.08583640f, -0.2447363f, 0.12467069f),
    //     .p_12        = vec2(-0.00054739257f, 0.00372932358f),
    // }
    .lens = {
        .aspect      = 1280 / 720.,
        .fov_radians = 0.79314559, // ~45.5 degrees
        .k_c         = vec2(0.5),
        // these values seem to change a lot between calibration runs?
        .k_1_3       = vec3(0.157115003f, -0.593241488f, 1.01098102f),
        .p_12        = vec2(-0.00054739257f, 0.00372932358f),
    }
};

CameraState _amazon_stereo_usb[2] = {
    {
        .lens = {
            .aspect      = 16. / 9.,
            .fov_radians = 2.0449739866908914,
            .k_c         = vec2( 0.4879256005521788, 0.4642858234200546),
            .k_1_3       = vec3( 0.0113082780018174, -0.029129783926016, 0.0048806219956355),
            .p_12        = vec2( 0.0008961485255962, 0.000528808575619),
        },
        .position = vec3(-2.83310033, -0.53254619, -0.19861688),
        .q = quat(0.01685885, -0.00548326, 0.00342667, 0.99983697),
    },
    {
        .lens = {
            .aspect      = 16. / 9.,
            .fov_radians = 2.0347862566187755,
            .k_c         = vec2( 0.5139234072813802, 0.4996598734199122),
            .k_1_3       = vec3( 0.0057174097990766, -0.024901395923418, 0.0040931625270905),
            .p_12        = vec2( 0.0002215559271998, -0.00021014499986295),
        },
        // this camera is the reference camera
    },
};


int main(int argc, char** argv) {
    rng_t rng {13242752664001236155ULL ^ _time_ns()};
    bool swap_buffers = true;

    if (find_cmd_option(argc, argv, "-h")) {
        std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  --kernels           : view the kernels" << std::endl;
        std::cout << "  --source            : specify stream source {single, dual, split} "
            "(default single)" << std::endl;
        std::cout << "  --no-swap           : do not swap buffers" << std::endl;
        std::cout << "  --render-depth <n>  : render the specified mip level" << std::endl;
        std::cout << "  --x-tiles <n>       : set the number of root-level x tiles (default 16)"
            << std::endl;
        std::cout << "  --y-tiles <n>       : set the number of root-level y tiles (default 9)"
            << std::endl;
        std::cout << "  --perturb-cam <f>   : perturb the camera orientation by a "
            "random amount near `f` degrees" << std::endl;
        std::cout << "  --init-distance <f> : set the initial distance of the root features "
            "(default 25)" << std::endl;
        std::cout << "  --idle-mode <s>     : set the idle mode to one of {fuse, match, none}."
            << std::endl;
        return 0;
    }
    if (find_cmd_option(argc, argv, "--kernels"))    view_mode    = ViewMode::Kernels;
    if (find_cmd_option(argc, argv, "--no-swap"))    swap_buffers = false;
    auto render_depth = get_option_u32(argc, argv, "--render-depth").value_or(0);
    auto x_tiles      = get_option_u32(argc, argv, "--x-tiles").value_or(16);
    auto y_tiles      = get_option_u32(argc, argv, "--y-tiles").value_or(9);
    auto cam_perturb  = get_option_f32(argc, argv, "--perturb-cam").value_or(0.);
    auto init_depth   = get_option_f32(argc, argv, "--init-distance").value_or(25.);
    idle_mode         = get_choice<StepMode>(argc, argv, "--idle-mode", {
        {"fuse",  StepMode::Fuse},
        {"match", StepMode::Match},
        {"none",  StepMode::None},
    }).value_or(StepMode::Match);
    source_mode = get_choice<SourceMode>(argc, argv, "--source", {
        {"single",  SourceMode::Single},
        {"dual",    SourceMode::Dual},
        {"split",   SourceMode::Split},
        {"virtual", SourceMode::Virtual}
    }).value_or(SourceMode::Single);

    CameraState cam_0 = _logitech_720p_cam;
    CameraState cam_1 = cam_0;

    if (cam_perturb > 0.) {
        // perturb the orientation of the second camera
        auto gauss_dist = std::normal_distribution<float>(0., 1.);
        vec3 axis {gauss_dist(rng), gauss_dist(rng), gauss_dist(rng)};
        float angle = cam_perturb * M_PI / 180.;
        quat dq = quat::rotation_from_axis_angle(axis.unit(), gauss_dist(rng) * angle);
        cam_1.q = dq * cam_1.q;
    }

    SolverRef solver = std::make_shared<StereoSolver>(
        x_tiles * y_tiles,
        _compute_tree_depth(1920, 1080, x_tiles * y_tiles, 16.)
    );
    std::vector<FrameSourceRef> frame_sources;
    FeedRef feed_0;
    FeedRef feed_1;

    switch (source_mode){
        case SourceMode::Single: {
            // displace and shift camera 1's view to align with a plane in camera 0's view
            cam_1.position = vec3(0.8, 0., 0.);
            cam_1.lens.k_c = vec2(0.51, 0.5);
            CaptureRef cap = _get_capture(0);
            // add a device and two views into that device with different virtual cameras.
            // we don't share textures, but this will a better representation of performance
            // for true dual-feed.
            DeviceFrameSourceRef device = _get_camera_source(solver, cap);
            feed_0 = device->add_viewpoint(range2ul::full, cam_0, "monostream cam A").feed;
            feed_1 = device->add_viewpoint(range2ul::full, cam_1, "monostream cam B").feed;
            frame_sources.push_back(device);
        } break;
        case SourceMode::Dual: {
            cam_0.position = vec3(-2., 0., 0.);
            cam_1.position = vec3( 2., 0., 0.);
            // cam_1.lens.k_c = vec2(0.51, 0.5);
            DeviceFrameSourceRef d0 = _get_camera_source(solver, _get_capture(0));
            DeviceFrameSourceRef d1 = _get_camera_source(solver, _get_capture(1));
            feed_0 = d0->add_viewpoint(range2ul::full, cam_0, "dual stream cam A").feed;
            feed_1 = d1->add_viewpoint(range2ul::full, cam_1, "dual stream cam B").feed;
            frame_sources.push_back(d0);
            frame_sources.push_back(d1);
        } break;
        case SourceMode::Split: {
            cam_0 = _amazon_stereo_usb[0];
            cam_1 = _amazon_stereo_usb[1];
            DeviceFrameSourceRef dev = _get_camera_source(solver, _get_capture(0));
            vec2ul res = dev->res;
            uint64_t w  = res.x;
            uint64_t h  = res.y;
            uint64_t w2 = w / 2;
            range2ul l_rect = {{0,  0}, {w2 - 1, h - 1}};
            range2ul r_rect = {{w2, 0}, { w - 1, h - 1}};
            feed_0 = dev->add_viewpoint(l_rect, cam_0, "split cam L").feed;
            feed_1 = dev->add_viewpoint(r_rect, cam_1, "split cam R").feed;
            frame_sources.push_back(dev);
        } break;
        case SourceMode::Virtual: {
            vec2u virtual_res = {1280, 720};
            ModelRef model = make_scene_model(cam_0, init_depth * 2.);
            VirtualSceneRef scene {
                new VirtualScene(
                    solver->device(),
                    solver->mip_generator(),
                    solver->filter(),
                    {
                        {virtual_res, cam_0},
                        {virtual_res, cam_1},
                    },
                    model
                )
            };
        } break;
    }

    Visualizer* viewer = new Visualizer{
        solver,
        {feed_0, feed_1},
        x_tiles,
        y_tiles,
        init_depth,
        render_depth
    };

    bool ok = true;
    // this time division is useful for separating initialization errors from
    // frame processing errors.
    std::cout << std::endl << "====== begin ======" << std::endl;

    // make the initial state live now, because we won't swap buffers again.
    viewer->solver->begin_new_frame();

    auto key_resp = [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_SPACE and action == GLFW_PRESS) {
            // toggle run state
            should_run = not should_run;
            std::cout << (should_run ? "running" : "paused") << std::endl;
        } else if (key == GLFW_KEY_RIGHT and action == GLFW_PRESS) {
            view_mode = (ViewMode) (((int)view_mode + 1) % 4);
        } else if (key == GLFW_KEY_LEFT and action == GLFW_PRESS) {
            view_mode = (ViewMode) positive_mod((int)view_mode - 1, 4);
        } else if ((key == GLFW_KEY_Q or key == GLFW_KEY_ESCAPE) and action == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        } else if (key == GLFW_KEY_R and action == GLFW_PRESS) {
            should_reinit = true;
        } else if (key == GLFW_KEY_UP and action == GLFW_PRESS) {
            depth_change += -1;
        } else if (key == GLFW_KEY_DOWN and action == GLFW_PRESS) {
            depth_change +=  1;
        } else if (key == GLFW_KEY_D and action == GLFW_PRESS) {
            should_undisplace = not should_undisplace;
        } else if (key == GLFW_KEY_S and action == GLFW_PRESS) {
            one_fr     = true;
            should_run = true;
        }
    };

    glfwSetKeyCallback(viewer->window.window, key_resp);

    bool running = should_run;
    while (not glfwWindowShouldClose(viewer->window.window)) {
        glfwPollEvents();
        bool first_step = false;

        for (auto& dev : frame_sources) {
            dev->capture();
        }

        if (should_reinit) {
            viewer->init_scene_features();
            viewer->solver->begin_new_frame();
            should_reinit = false;
        } else if (should_run and not running) {
            // initial frame. do not swap buffers, they are already primed.
            // also, do not time-solve, because we don't have a previous frame.
            running    = true;
            first_step = true;
        } else if (should_run and swap_buffers) {
            // intermediate frame
            viewer->solver->begin_new_frame();
        }

        if (depth_change != 0) {
            viewer->set_render_depth(viewer->get_render_depth() + depth_change);
            depth_change = 0;
        }

        bool do_time_solve = not first_step;
        viewer->do_frame(
            view_mode,
            should_run ? StepMode::Fuse : idle_mode,
            do_time_solve,
            should_undisplace
        );

        if (one_fr) {
            should_run = false;
            one_fr     = false;
        }

        // allow async / callbacks to be processed (e.g. buffer read/writes)
        // false: do not block if no events are pending
        viewer->solver->device().poll(false);
        // todo: control frame rate
        if (viewer->solver->has_error()) {
            ok = false;
            break;
        }
    }
    for (auto& f : frame_sources) {
        DeviceFrameSourceRef dev = std::dynamic_pointer_cast<DeviceFrameSource>(f);
        if (dev) {
            CaptureRef cap = dev->capture_device;
            if (cap->isOpened()) cap->release();
        }
    }

    if (ok) std::cout << "finished!" << std::endl;
    else    std::cerr << "Aborted."   << std::endl;
}
