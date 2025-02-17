#pragma once

#include <geomc/linalg/AffineTransform.h>

#include <stereo/defs.h>
#include <stereo/gpu/window.h>
#include <stereo/gpu/camera.h>
#include <stereo/gpu/input.h>

// todo: put picking back
// todo: add panning
//   note: recentering should reset the pan
// todo: add ortho support for zooming, etc.

namespace stereo {

/**
 * @brief A camera that can be tumbled about a point.
 */
struct Tumbler : public InputHandler {
protected:
    
    Window* _window;
    Camera<float> _cam;
    xf3     _orbit_orient; // todo: should be similar transform
    float   _elev        = 0;
    float   _azi         = 0;
    float   _r           = 1;
    float   _fov_degrees = 90.;
    vec3    _ctr         = {};
    vec3    _axis        = {0,0,1};
    float   _near        = 1 / 1000.;
    float   _far         = 10.;
    
    std::optional<vec2d> _pick_pt = std::nullopt;
    
    int   _tumble_button     = GLFW_MOUSE_BUTTON_1;
    int   _dolly_button      = GLFW_MOUSE_BUTTON_2;
    int   _pick_button       = GLFW_MOUSE_BUTTON_1;
    int   _pick_radius       = 4;     // number of pixels above/below a pick pixel to search
    float _elev_sensitivity  = 2;     // number of half-turns per screen height
    float _azi_sensitivity   = 2;     // number of turns per screen width
    float _dolly_sensitivity = 8;     // multiplier on distance per screen height of drag
    float _zoom_sensitivity  = 1/32.; // multiplier on FOV per unit of scroll
    range _fov_range_degrees = {1, 170.}; // allowable range of FOV, degrees

    void _update_camera();
    void _set_projection();
    void _perform_pick();
    
public:
    
    Tumbler(Window* window, vec3 axis={0,0,1}, vec3 ctr={});
    ~Tumbler();
    
    vec3 center() const;
    void set_near(float positive_near);
    void set_far(float positive_far);
    void set_axis(vec3 axis);
    void set_distance(float r);
    void center_on(vec3 o);
    void update_aspect();
    Input& input() const;
    Camera<float>& camera();
    
    /*===================*
     * event handlers    *
     *===================*/
    
    EventResponse window_resize_event(
            InputManager* input_manager,
            vec2ui old_dims,
            vec2ui new_dims);
    
    /// Process cursor movement, in signed-normalized uniform-aspect window coordinates.
    MouseResponse cursor_move_event(
            InputManager* input_manager,
            vec2d window_dx,
            vec2d window_pos);
    
    /// Process a double click event. The (signed, normalized, uniform-aspect) window
    /// position of each click is reported.
    MouseResponse double_click_event(
            InputManager* input_manager,
            int button,
            vec2d click_1,
            vec2d click_2);
    
    /// Process a scroll event, in native GLFW scroll coordinates.
    ScrollResponse scroll_event(InputManager* input_manager, vec2d scroll_pos, vec2d scroll_dx);
    
    /// Process a screen touch event, in WINDOW coordinates.
    TouchResponse touch_event(
            InputManager* input_manager,
            touch_id_t id,
            TouchChange change,
            vec2d window_pos,
            vec2d window_dx,
            double force);
};


} // namespace stereo
