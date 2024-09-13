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
    return cap;
}

// calibrated with `camcal.py`:
CameraState _logitech_720p_cam = {
    .lens = {
        .aspect      = 1280 / 720.,
        .fov_radians = 0.9985, // ~57 degrees
        .k_c         = vec2(0.5),
        // these values seem to change a lot between calibration runs?
        .k_1_3       = vec3(0.157115003, -0.593241488, 1.01098102),
        .p_12        = vec2(-0.00054739257, 0.00372932358),
    }
};

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

static int64_t _time_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

bool should_run = false;
ViewMode view_mode = ViewMode::Splat;

int main(int argc, char** argv) {
    rng_t rng {13242752664001236155ULL ^ _time_ns()};
    StepMode step_mode = StepMode::Fuse;
    bool one_source   = false;
    bool swap_buffers = true;
    
    if (_find_cmd_option(argc, argv, "-h")) {
        std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  --kernels          : view the kernels" << std::endl;
        std::cout << "  --match-only       : only perform the stereo matching step; "
            "no time or hierarchical fusion" << std::endl;
        std::cout << "  --one-source       : use only one image source" << std::endl;
        std::cout << "  --no-swap          : do not swap buffers" << std::endl;
        std::cout << "  --render-depth <n> : render the specified mip level" << std::endl;
        std::cout << "  --x-tiles <n>      : set the number of root-level x tiles (default 16)" << std::endl;
        std::cout << "  --y-tiles <n>      : set the number of root-level y tiles (default 9)" << std::endl;
        std::cout << "  --perturb-cam <f>  : perturb the camera orientation by a "
            "random amount near `f` degrees" << std::endl;
        return 0;
    }
    if (_find_cmd_option(argc, argv, "--kernels"))    view_mode    = ViewMode::Kernels;
    if (_find_cmd_option(argc, argv, "--match-only")) step_mode    = StepMode::Match;
    if (_find_cmd_option(argc, argv, "--one-source")) one_source   = true;
    if (_find_cmd_option(argc, argv, "--no-swap"))    swap_buffers = false;
    auto render_depth = _get_option_u32(argc, argv, "--render-depth").value_or(0);
    auto x_tiles      = _get_option_u32(argc, argv, "--x-tiles").value_or(16);
    auto y_tiles      = _get_option_u32(argc, argv, "--y-tiles").value_or(9);
    auto cam_perturb  = _get_option_f32(argc, argv, "--perturb-cam").value_or(0.);
    
    CameraState cam_0 = _logitech_720p_cam;
    CameraState cam_1 = _logitech_720p_cam;
    Visualizer* viewer = nullptr;
    
    if (one_source) {
        auto gauss_dist = std::normal_distribution<float>(0., 1.);
        // displace and shift camera 1's view to align with a plane in camera 0's view
        cam_1.position = vec3(0.8, 0., 0.);
        cam_1.lens.k_c = vec2(0.51, 0.5);
        CaptureRef cap = _get_capture(0);
        
        if (cam_perturb > 0.) {
            // perturb the orientation ever so slightly
            vec3 axis {gauss_dist(rng), gauss_dist(rng), gauss_dist(rng)};
            float angle = cam_perturb * M_PI / 180.;
            quat dq = quat::rotation_from_axis_angle(axis.unit(), gauss_dist(rng) * angle);
            cam_1.q = dq * cam_1.q;
        }
        
        viewer = new Visualizer{{FrameSource{cap, cam_0}}, x_tiles, y_tiles, render_depth};
        // duplicate the capture source and add it as a new one.
        // share the filtered textures and captured frame.
        CaptureFrame cf   = viewer->solver->frame_source(0, FrameSelection::Current);
        cf.camera_state() = cam_1;
        viewer->solver->add_source(cf);
    } else {
        cam_0.position = vec3(-2., 0., 0.);
        cam_1.position = vec3( 2., 0., 0.);
        FrameSource fs[] = {
            {_get_capture(0), cam_0},
            {_get_capture(1), cam_1},
        };
        viewer = new Visualizer{{fs[0], fs[1]}, x_tiles, y_tiles, render_depth};
    }
    
    bool ok = true;
    std::cout << std::endl << std::endl << "====== begin ======" << std::endl;
    
    // make the initial state live now, because we won't swap buffers again.
    viewer->solver->begin_new_frame();
    
    auto key_resp = [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_SPACE and action == GLFW_PRESS) {
            should_run = true;
        } else if (key == GLFW_KEY_ENTER and action == GLFW_PRESS) {
            if (view_mode == ViewMode::Splat) view_mode = ViewMode::Kernels;
            else                              view_mode = ViewMode::Splat;
        } else if ((key == GLFW_KEY_Q or key == GLFW_KEY_ESCAPE) and action == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    };
    
    glfwSetKeyCallback(viewer->window.window, key_resp);
    
    while (not glfwWindowShouldClose(viewer->window.window)) {
        glfwPollEvents();
        
        if (one_source) viewer->solver->capture(0);
        else            viewer->solver->capture_all();
        
        if (should_run and swap_buffers) viewer->solver->begin_new_frame();
        
        viewer->do_frame(view_mode, should_run ? step_mode : StepMode::Match);
        viewer->solver->device().poll(false); // xxx what does this do...?
        // todo: control frame rate
        if (viewer->solver->has_error()) {
            ok = false;
            break;
        }
    }
    // free the cameras
    for (size_t i = 0; i < viewer->solver->frame_count(); ++i) {
        auto& fs = viewer->solver->frame_source(i, FrameSelection::Current);
        if (fs.source and fs.source->isOpened()) {
            fs.source->release();
        }
    }
    
    if (ok) std::cout << "finished!" << std::endl;
    else    std::cerr << "Aborted."   << std::endl;
}
