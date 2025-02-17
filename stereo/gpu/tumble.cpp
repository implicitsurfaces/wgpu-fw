#include <stereo/gpu/tumble.h>

namespace stereo {

void Tumbler::_update_camera() {
    // recall that transforms are outer -> inner
    _cam.cam_to_world =
        // put the orbit-space origin at _ctr:
        geom::translation(_ctr) *
        // apply orbit orientation:
        _orbit_orient * 
        // apply azimuth. positive azimuth is cw about +y:
        geom::rotation_y(_azi) *
        // apply elevation. positive elevation looks from a higher angle;
        // so +elev means a ccw rotation about +x:
        geom::rotation_x(-_elev) *
        // camera starts at the origin, pointing down Z.
        // translate it in the +Z direction by _r:
        geom::translation(0.f, 0.f, _r);
    _set_projection();
}

void Tumbler::_set_projection() {
    auto d = _window->window_dims;
    _cam.set_perspective(_fov_degrees, d.x / (float) d.y, _near, _far);
}

void Tumbler::_perform_pick() {
    /*
    if (_pick_pt) {
        std::optional<vec3> world_p = _window->depth_pick_in_valid_context(
            _cam,
            *_pick_pt,
            _pick_radius
        );
        if (world_p) {
            _ctr = *world_p;
            _update_camera();
        }
        _pick_pt = std::nullopt;
    }
    */
}

Tumbler::Tumbler(Window* window, vec3 axis, vec3 ctr):
    _window(window),
    _ctr(ctr),
    _axis(axis)
{
    set_axis(axis);
    window->input_manager.handlers.push_back(static_cast<InputHandler*>(this));
}

Tumbler::~Tumbler() {
    auto& handlers = _window->input_manager.handlers;
    auto it = std::find(handlers.begin(), handlers.end(), this);
    if (it != handlers.end()) {
        handlers.erase(it);
    }
}

vec3 Tumbler::center() const {
    return _ctr;
}

void Tumbler::set_near(float positive_near) {
    _near = positive_near;
    _set_projection();
}

void Tumbler::set_far(float positive_far) {
    _far = positive_far;
    _set_projection();
}

void Tumbler::set_axis(vec3 axis) {
    _axis = axis;
    // camera begins at the origin, pointing down Z, with Y up.
    // put Y in alignment with the orbit axis, making it the new "up":
    _orbit_orient = geom::direction_align({0, 1, 0}, axis);
    _update_camera();
}

void Tumbler::set_distance(float r) {
    _r = r;
    _update_camera();
}

void Tumbler::center_on(vec3 o) {
    _pick_pt = std::nullopt; // clear any pending pick
    _ctr = o;
    _update_camera();
}

void Tumbler::update_aspect() {
    _set_projection();
}

Input& Tumbler::input() const {
    return _window->input_manager.input;
}

Camera<float>& Tumbler::camera() {
    return _cam;
}

/*===================*
 * event handlers    *
 *===================*/

EventResponse Tumbler::window_resize_event(
        InputManager* input_manager,
        vec2ui old_dims,
        vec2ui new_dims)
{
    update_aspect();
    return EventResponse::PASS;
}


/// Process cursor movement, in signed-normalized uniform-aspect window coordinates.
MouseResponse Tumbler::cursor_move_event(
        InputManager* input_manager,
        vec2d window_dx,
        vec2d window_pos)
{
    bool needs_update = false;
    if (input_manager->input.buttons[_tumble_button].is_down) {
        // +x -> ccw rotation about pole, to make the object look like it's
        // rotating cw; i.e., front being pulled in direction of mouse drag
        _azi  -= window_dx.x * 2 * M_PI * _azi_sensitivity;
        // _azi   = fmod(_azi, 2 * M_PI);
        // +y -> lower elevation angle, to make the object look like it's
        // rotating toward its underside.
        _elev -= window_dx.y * M_PI * _elev_sensitivity;
        _elev  = clamp<float>(_elev, -M_PI / 2, M_PI / 2);
        needs_update = true;
    }
    if (input_manager->input.buttons[_dolly_button].is_down) {
        // drag up -> +y -> closer (-r)
        double k = std::pow(_dolly_sensitivity, -window_dx.y);
        _r    *= k;
        _near *= k;
        _far  *= k;
        needs_update = true;
    }
    if (needs_update) _update_camera();
    return needs_update ? EventResponse::CONSUME : EventResponse::PASS;
}

/// Process a double click event. The (signed, normalized, uniform-aspect) window
/// position of each click is reported.
MouseResponse Tumbler::double_click_event(
        InputManager* input_manager,
        int button,
        vec2d click_1,
        vec2d click_2)
{
    if (button == _pick_button) {
        _pick_pt = click_1;
        return EventResponse::CONSUME;
    }
    return EventResponse::PASS;
}

/// Process a scroll event, in native GLFW scroll coordinates.
ScrollResponse Tumbler::scroll_event(
        InputManager* input_manager,
        vec2d scroll_pos,
        vec2d scroll_dx)
{
    if (scroll_dx.y != 0) {
        double k = std::pow(1 + _zoom_sensitivity, scroll_dx.y);
        _fov_degrees = _fov_range_degrees.clip(k * _fov_degrees);
        _set_projection();
        return EventResponse::CONSUME;
    }
    return EventResponse::PASS;
}

/// Process a screen touch event, in WINDOW coordinates.
TouchResponse Tumbler::touch_event(
        InputManager* input_manager,
        touch_id_t id,
        TouchChange change,
        vec2d window_pos,
        vec2d window_dx,
        double force)
{
    // do nothing
    return EventResponse::PASS;
}


} // namespace stereo
