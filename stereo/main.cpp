#include "stereo/app/frame_source.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>

#include <stereo/gpu/render_pipeline.h>
#include <stereo/gpu/shader.h>
#include <stereo/gpu/window.h>
#include <stereo/app/solver.h>
#include <stereo/app/visualize.h>

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

static std::optional<size_t> _find_cmd_option(int argc, char** argv, std::string_view option) {
    auto it = std::find(argv, argv + argc, option);
    if (it == argv + argc) return std::nullopt;
    return std::distance(argv, it);
}

static std::optional<uint32_t> _get_option_u32(int argc, char** argv, std::string_view option) {
    auto i = _find_cmd_option(argc, argv, option);
    if (i) {
        size_t index = *i + 1;
        if (index < argc) {
            try {
                return std::stoi(argv[index]);
            } catch (std::invalid_argument& e) {
                std::cerr << "Invalid integer argument for option `" << option << "`" << std::endl;
                return std::nullopt;
            }
        } else {
            std::cerr << "Missing integer argument for option `" << option << "`" << std::endl;
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }
}

static std::optional<float> _get_option_f32(int argc, char**argv, std::string_view option) {
    auto i = _find_cmd_option(argc, argv, option);
    if (i) {
        size_t index = *i + 1;
        if (index < argc) {
            try {
                return std::stof(argv[index]);
            } catch (std::invalid_argument& e) {
                std::cerr << "Invalid float argument for option `" << option << "`" << std::endl;
                return std::nullopt;
            }
        } else {
            std::cerr << "Missing float argument for option `" << option << "`" << std::endl;
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }
}

template <typename T>
static std::optional<T> _get_choice(
    int argc,
    char** argv,
    std::string_view option,
    std::initializer_list<std::pair<std::string_view, T>> choices)
{
    auto i = _find_cmd_option(argc, argv, option);
    if (i) {
        size_t index = *i + 1;
        if (index < argc) {
            auto choice = std::string_view(argv[index]);
            for (auto& [name, value] : choices) {
                if (choice == name) return value;
            }
            std::cerr << "Invalid choice for option `" << option << "`" << std::endl;
            return std::nullopt;
        } else {
            std::cerr << "Missing choice argument for option `" << option << "`" << std::endl;
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }
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
    .lens = {
        .aspect      = 1280 / 720.,
        .fov_radians = 0.9985, // ~57 degrees
        .k_c         = vec2(0.5),
        // these values seem to change a lot between calibration runs?
        .k_1_3       = vec3(0.157115003f, -0.593241488f, 1.01098102f),
        .p_12        = vec2(-0.00054739257f, 0.00372932358f),
    }
};

CameraState _amazon_stereo_usb = {
    .lens = {
        .aspect      = 16. / 9.,
        .fov_radians = 2.,
        .k_c         = vec2( 0.486832534,    0.45909096),
        .k_1_3       = vec3( 0.0011709166,  -0.015928833, 0.00073791838),
        .p_12        = vec2(-0.00075877646, -0.0015043166)
    }
};


int main(int argc, char** argv) {
    rng_t rng {13242752664001236155ULL ^ _time_ns()};
    bool swap_buffers = true;

    if (_find_cmd_option(argc, argv, "-h")) {
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
    if (_find_cmd_option(argc, argv, "--kernels"))    view_mode    = ViewMode::Kernels;
    if (_find_cmd_option(argc, argv, "--no-swap"))    swap_buffers = false;
    auto render_depth = _get_option_u32(argc, argv, "--render-depth").value_or(0);
    auto x_tiles      = _get_option_u32(argc, argv, "--x-tiles").value_or(16);
    auto y_tiles      = _get_option_u32(argc, argv, "--y-tiles").value_or(9);
    auto cam_perturb  = _get_option_f32(argc, argv, "--perturb-cam").value_or(0.);
    auto init_depth   = _get_option_f32(argc, argv, "--init-depth").value_or(25.);
    idle_mode         = _get_choice<StepMode>(argc, argv, "--idle-mode", {
        {"fuse",  StepMode::Fuse},
        {"match", StepMode::Match},
        {"none",  StepMode::None},
    }).value_or(StepMode::Match);
    source_mode = _get_choice<SourceMode>(argc, argv, "--source", {
        {"single", SourceMode::Single},
        {"dual",   SourceMode::Dual},
        {"split",  SourceMode::Split},
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

    std::shared_ptr<StereoSolver> solver = std::make_shared<StereoSolver>(
        x_tiles * y_tiles,
        _compute_tree_depth(1920, 1080, x_tiles * y_tiles, 16.)
    );
    std::vector<DeviceFrameSourceRef> devices;
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
            DeviceFrameSourceRef device = solver->create_device(cap);
            feed_0 = device->add_viewpoint(range2ul::full, cam_0, "monostream cam A").feed;
            feed_1 = device->add_viewpoint(range2ul::full, cam_1, "monostream cam B").feed;
            devices.push_back(device);
        } break;
        case SourceMode::Dual: {
            cam_0.position = vec3(-2., 0., 0.);
            cam_1.position = vec3( 2., 0., 0.);
            // cam_1.lens.k_c = vec2(0.51, 0.5);
            DeviceFrameSourceRef d0 = solver->create_device(_get_capture(0));
            DeviceFrameSourceRef d1 = solver->create_device(_get_capture(1));
            feed_0 = d0->add_viewpoint(range2ul::full, cam_0, "dual stream cam A").feed;
            feed_1 = d1->add_viewpoint(range2ul::full, cam_1, "dual stream cam B").feed;
            devices.push_back(d0);
            devices.push_back(d1);
        } break;
        case SourceMode::Split: {
            cam_0 = cam_1 = _amazon_stereo_usb;
            cam_0.position = vec3(-2., 0., 0.);
            cam_1.position = vec3( 2., 0., 0.);
            DeviceFrameSourceRef dev = solver->create_device(_get_capture(0));
            vec2ul res = dev->res;
            uint64_t w  = res.x;
            uint64_t h  = res.y;
            uint64_t w2 = w / 2;
            range2ul l_rect = {{0,  0}, {w2 - 1, h - 1}};
            range2ul r_rect = {{w2, 0}, { w - 1, h - 1}};
            feed_0 = dev->add_viewpoint(l_rect, cam_0, "split cam L").feed;
            feed_1 = dev->add_viewpoint(r_rect, cam_1, "split cam R").feed;
            devices.push_back(dev);
        }
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
        } else if (key == GLFW_KEY_ENTER and action == GLFW_PRESS) {
            // cycle through view modes
            if      (view_mode == ViewMode::Splat)   view_mode = ViewMode::Kernels;
            else if (view_mode == ViewMode::Kernels) view_mode = ViewMode::Feed;
            else if (view_mode == ViewMode::Feed)    view_mode = ViewMode::Splat;
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

        for (auto& dev : devices) {
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

        viewer->solver->device().poll(false); // xxx what does this do...?
        // todo: control frame rate
        if (viewer->solver->has_error()) {
            ok = false;
            break;
        }
    }
    for (auto& dev : devices) {
        CaptureRef cap = dev->capture_device;
        if (cap and cap->isOpened()) {
            cap->release();
        }
    }

    if (ok) std::cout << "finished!" << std::endl;
    else    std::cerr << "Aborted."   << std::endl;
}
