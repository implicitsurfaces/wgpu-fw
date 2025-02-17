#include <stereo/gpu/window.h>
#include <stereo/gpu/input.h>

namespace stereo {

    
double current_time() {
    static std::chrono::steady_clock timer;
    static auto init_time = timer.now();
    return std::chrono::duration<double>{timer.now() - init_time}.count();
}

/*************************************
 * GLFW callbacks                    *
 *************************************/

static Window* _get_window_obj(GLFWwindow* window) {
    return (Window*) glfwGetWindowUserPointer(window);
}

void _framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    auto window_obj = _get_window_obj(window);
    if (window_obj) {
        vec2ui dims {(uint) width, (uint) height};
        // window_obj->window_target.resize(dims);
        window_obj->input_manager.trigger_framebuffer_resized(dims);
        // if (window_obj->is_inited()) {
        //     window_obj->draw();
        // }
    }
}

void _window_size_callback(GLFWwindow* window, int width, int height) {
    auto window_obj = _get_window_obj(window);
    vec2ui dims {(uint) width, (uint) height};
    if (window_obj) window_obj->input_manager.trigger_window_resized(dims);
}

void _scroll_callback(GLFWwindow* window, double dx, double dy) {
    auto window_obj = _get_window_obj(window);
    if (window_obj) window_obj->input_manager.trigger_scroll({dx, dy});
}

void _cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    auto window_obj = _get_window_obj(window);
    if (window_obj) window_obj->input_manager.trigger_cursor_moved({xpos, ypos});
}

void _mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    auto window_obj = _get_window_obj(window);
    if (window_obj) window_obj->input_manager.trigger_mouse_button(button, action, mods);
}

void _key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto window_obj = _get_window_obj(window);
    if (window_obj) window_obj->input_manager.trigger_key(key, scancode, action, mods);
}

void _text_callback(GLFWwindow* window, uint32_t code_point) {
    auto window_obj = _get_window_obj(window);
    if (window_obj) window_obj->input_manager.trigger_text(code_point);
}

/*************************************
 * InputManager                      *
 *************************************/

InputManager::InputManager(Window* window):
    window(window) {}

InputManager::~InputManager() {}

template <typename... Args>
EventResponse InputManager::_trigger_mouse_event(
        // member function of InputHandler, call it `handler_callback`:
        MouseResponse          (InputHandler::*handler_callback)(InputManager*, Args...),
        // member function of MouseGestureHandler, call it `gesture_callback`:
        GestureResponse (MouseGestureHandler::*gesture_callback)(InputManager*, Args...),
        // args to pass to the member functions:
        Args... args)
{
    // the gesture handler gets first dibs on the event
    if (mouse_gesture_handler) {
        // ... = mouse_gesture_handler->gesture_callback(this, args...);
        GestureResponse resp = std::invoke(
            gesture_callback,
            mouse_gesture_handler,
            this,
            args...
        );
        if (resp.state != GestureState::CONTINUE) {
            mouse_gesture_handler->end_gesture(
                this,
                resp.state == GestureState::FINISH
                    ? GestureFinish::FINISH
                    : GestureFinish::CANCEL
            );
            mouse_gesture_handler = nullptr;
        }
        if (resp.response == EventResponse::CONSUME) {
            return EventResponse::CONSUME;
        }
    }
    
    // if the gesture handler didn't consume the event, pass it to the regular handlers
    for (auto handler : handlers) {
        // ... = handler->handler_callback(this, args...);
        MouseResponse resp = std::invoke(
            handler_callback,
            handler,
            this,
            args...
        );
        if (std::holds_alternative<MouseGestureHandlerRef>(resp)) {
            MouseGestureHandlerRef gesture = std::get<MouseGestureHandlerRef>(resp);
            if (mouse_gesture_handler) {
                gesture->begin_gesture(this, GestureStart::REJECT);
            } else {
                mouse_gesture_handler = gesture;
                gesture->begin_gesture(this, GestureStart::ACCEPT);
            }
        } else if (std::get<EventResponse>(resp) == EventResponse::CONSUME) {
            return EventResponse::CONSUME;
        }
    }
    return EventResponse::PASS;
}

EventResponse InputManager::trigger_framebuffer_resized(vec2ui new_dims) {
    // update the window's dimension variables
    auto old_dims = window->surface_dims;
    window->surface_dims = new_dims;
    // report the event
    for (auto handler : handlers) {
        EventResponse resp = handler->framebuffer_resize_event(this, old_dims, new_dims);
        if (resp == EventResponse::CONSUME) {
            return resp;
        }
    }
    return EventResponse::PASS;
}

EventResponse InputManager::trigger_window_resized(vec2ui new_dims) {
    // update the window's dimension variables
    vec2ui old_dims = window->window_dims;
    window->window_dims = new_dims;
    // report the event
    for (auto handler : handlers) {
        EventResponse resp = handler->window_resize_event(this, old_dims, new_dims);
        if (resp == EventResponse::CONSUME) {
            return resp;
        }
    }
    return EventResponse::PASS;
}

EventResponse InputManager::trigger_cursor_moved(vec2d new_window_pos) {
    // compute + accumulate per-button deltas from the last frame
    vec2d prev = input.cursor_pos;
    vec2d cur  = window->convert_window_coordinates(
        new_window_pos,
        CoordSpace::WINDOW_COORDS,
        CoordSpace::ASPECT_COORDS
    );
    vec2d dx   = cur - prev;
    input.cursor_pos = cur;
    input.cursor_dx += dx;
    for (int button = 0; button < 3; ++button) {
        auto& b = input.buttons[button];
        if (b.is_down) {
            b.pan += dx;
        }
    }
    
    // report the event and/or initiate gestures
    return this->_trigger_mouse_event(
        &InputHandler::cursor_move_event,
        &MouseGestureHandler::cursor_move_event,
        dx, cur
    );
}

EventResponse InputManager::trigger_mouse_button(int button, int action, int mods) {
    // record per-button modifiers, cursor position, and time of event
    auto& b = input.buttons[button];
    b.is_down = (action == GLFW_PRESS);
    if (action == GLFW_PRESS) {
        auto prev          = b.last_down;
        auto now           = current_time();
        auto last_down_pos = b.down_pos;
        b.down_pos         = input.cursor_pos;
        b.down_modifiers   = mods;
        b.last_down        = now;
        
        // attempt to detect a double click; if found, report it
        if (now - prev <= DOUBLE_CLICK_THRESHOLD) {
            b.down_modifiers = mods |= MOD_DOUBLECLICK;
            EventResponse resp = trigger_double_click(button, last_down_pos, input.cursor_pos);
            if (resp == EventResponse::CONSUME) {
                // double click event was consumed; do not bubble
                return resp;
            }
        }
    }
    
    // report the event and/or initiate gestures
    return this->_trigger_mouse_event(
        &InputHandler::mouse_button_event,
        &MouseGestureHandler::mouse_button_event,
        button, action, mods
    );
}

EventResponse InputManager::trigger_double_click(int button, vec2d click_1, vec2d click_2) {
    // report the event and/or initiate gestures
    return this->_trigger_mouse_event(
        &InputHandler::double_click_event,
        &MouseGestureHandler::double_click_event,
        button, click_1, click_2
    );
}

EventResponse InputManager::trigger_key(int key, int scancode, int action, int mods) {
    // report the event
    for (auto handler : handlers) {
        EventResponse resp = handler->key_event(this, key, scancode, action, mods);
        if (resp == EventResponse::CONSUME) {
            return resp;
        }
    }
    return EventResponse::PASS;
}

EventResponse InputManager::trigger_text(uint32_t codepoint) {
    // report the event
    for (auto handler : handlers) {
        EventResponse resp = handler->text_event(this, codepoint);
        if (resp == EventResponse::CONSUME) {
            return resp;
        }
    }
    return EventResponse::PASS;
}

EventResponse InputManager::trigger_paste_text(std::string_view text) {
    // report the event
    for (auto handler : handlers) {
        EventResponse resp = handler->paste_text_event(this, text);
        if (resp == EventResponse::CONSUME) {
            return resp;
        }
    }
    return EventResponse::PASS;
}

EventResponse InputManager::trigger_scroll(vec2d scroll_dx) {
     // accumulate scroll delta
    input.scroll_dx = scroll_dx;
    input.scroll   += scroll_dx;
    // the gesture handler gets first dibs on the event
    if (mouse_gesture_handler) {
        GestureResponse resp = scroll_gesture_handler->scroll_event(this, input.scroll, scroll_dx);
        if (resp.state != GestureState::CONTINUE) {
            mouse_gesture_handler->end_gesture(
                this,
                resp.state == GestureState::FINISH
                    ? GestureFinish::FINISH
                    : GestureFinish::CANCEL
            );
            mouse_gesture_handler = nullptr;
        }
        if (resp.response == EventResponse::CONSUME) {
            return EventResponse::CONSUME;
        }
    }
    
    // report the event and/or initiate gestures
    for (auto handler : handlers) {
        ScrollResponse resp = handler->scroll_event(this, input.scroll, scroll_dx);
        if (std::holds_alternative<ScrollGestureHandlerRef>(resp)) {
            ScrollGestureHandlerRef gesture = std::get<ScrollGestureHandlerRef>(resp);
            if (scroll_gesture_handler) {
                gesture->begin_gesture(this, GestureStart::REJECT);
            } else {
                scroll_gesture_handler = gesture;
                gesture->begin_gesture(this, GestureStart::ACCEPT);
            }
        } else if (std::get<EventResponse>(resp) == EventResponse::CONSUME) {
            return EventResponse::CONSUME;
        }
    }
    return EventResponse::PASS;
}

EventResponse InputManager::trigger_touch(
            touch_id_t  id,
            TouchChange change,
            vec2d       window_pos,
            double      force,
            mat2d       patch)
{
    // process the touch event
    const auto [i, existing] = input.touches.insert_or_assign(id, Touch{});
    Touch  prev  = i->second; // snapshot prev value
    Touch& touch = i->second; // update cur value
    switch (change) {
        case TouchChange::BEGIN: {
            touch.last_down = current_time();
            touch.is_down   = true;
            touch.down_pos  = window_pos;
            if (not existing) {
                prev.pos    = window_pos; // prev value was nonsense (0,0)
            }
        } break;
        case TouchChange::MOVE: {
            // do nothing
        } break;
        case TouchChange::END:
        case TouchChange::CANCEL: {
            touch.is_down = false;
        } break;
    }
    
    touch.pos   = window_pos;
    touch.pan   = touch.down_pos - window_pos;
    touch.force = force;
    touch.patch = patch;
    
    // the gesture handler gets first dibs on the event
    if (touch_gesture_handler) {
        GestureResponse resp = touch_gesture_handler->touch_event(
            this,
            id,
            change,
            window_pos,
            window_pos - prev.pos,
            force
        );
        if (resp.state != GestureState::CONTINUE) {
            touch_gesture_handler->end_gesture(
                this,
                resp.state == GestureState::FINISH
                    ? GestureFinish::FINISH
                    : GestureFinish::CANCEL
            );
            touch_gesture_handler = nullptr;
        }
        if (resp.response == EventResponse::CONSUME) {
            return EventResponse::CONSUME;
        }
    }
    
    // report the event and/or initiate gestures
    for (auto handler : handlers) {
        TouchResponse resp = handler->touch_event(
            this,
            id,
            change,
            window_pos,
            window_pos - prev.pos,
            force
        );
        if (std::holds_alternative<TouchGestureHandlerRef>(resp)) {
            TouchGestureHandlerRef gesture = std::get<TouchGestureHandlerRef>(resp);
            if (touch_gesture_handler) {
                gesture->begin_gesture(this, GestureStart::REJECT);
            } else {
                touch_gesture_handler = gesture;
                gesture->begin_gesture(this, GestureStart::ACCEPT);
            }
        } else if (std::get<EventResponse>(resp) == EventResponse::CONSUME) {
            return EventResponse::CONSUME;
        }
    }
    return EventResponse::PASS;
}


/*************************************
 * InputHandler                      *
 *************************************/

InputHandler::~InputHandler() {}

EventResponse InputHandler::framebuffer_resize_event(
    InputManager* input,
    vec2ui        old_dims,
    vec2ui        new_dims)
{
    return EventResponse::PASS;
}

EventResponse InputHandler::window_resize_event(
    InputManager* input,
    vec2ui        old_dims,
    vec2ui        new_dims)
{
    return EventResponse::PASS;
}

MouseResponse InputHandler::cursor_move_event(
    InputManager* input,
    vec2d         delta,
    vec2d         position)
{
    return EventResponse::PASS;
}

MouseResponse InputHandler::mouse_button_event(
    InputManager* input,
    int           button,
    int           action,
    int           mods)
{
    return EventResponse::PASS;
}

MouseResponse InputHandler::double_click_event(
    InputManager* input,
    int           button,
    vec2d         click_1,
    vec2d         click_2)
{
    return EventResponse::PASS;
}

EventResponse InputHandler::key_event(
    InputManager* input,
    int           key,
    int           scancode,
    int           action,
    int           mods)
{
    return EventResponse::PASS;
}

EventResponse InputHandler::text_event(
    InputManager* input,
    uint32_t      codepoint)
{
    return EventResponse::PASS;
}

EventResponse InputHandler::paste_text_event(
    InputManager* input,
    std::string_view text)
{
    return EventResponse::PASS;
}

ScrollResponse InputHandler::scroll_event(
    InputManager* input,
    vec2d         scroll,
    vec2d         scroll_dx)
{
    return EventResponse::PASS;
}

TouchResponse InputHandler::touch_event(
    InputManager* input,
    touch_id_t    id,
    TouchChange   change,
    vec2d         window_pos,
    vec2d         window_dx,
    double        force)
{
    return EventResponse::PASS;
}


/*************************************
 * GestureHandlers                   *
 *************************************/

// MouseGestureHandler

void MouseGestureHandler::begin_gesture(InputManager* input, GestureStart start) {
    // do nothing
}

void MouseGestureHandler::end_gesture(InputManager* input, GestureFinish finish) {
    // do nothing
}

GestureResponse MouseGestureHandler::mouse_button_event(
    InputManager* input,
    int           button,
    int           action,
    int           mods)
{
    return {EventResponse::PASS, GestureState::CONTINUE};
}

GestureResponse MouseGestureHandler::cursor_move_event(
    InputManager* input,
    vec2d         delta,
    vec2d         position)
{
    return {EventResponse::PASS, GestureState::CONTINUE};
}

GestureResponse MouseGestureHandler::double_click_event(
    InputManager* input,
    int           button,
    vec2d         click_1,
    vec2d         click_2)
{
    return {EventResponse::PASS, GestureState::CONTINUE};
}


// TouchGestureHandler

void TouchGestureHandler::begin_gesture(InputManager* input, GestureStart start) {
    // do nothing
}

void TouchGestureHandler::end_gesture(InputManager* input, GestureFinish finish) {
    // do nothing
}

GestureResponse TouchGestureHandler::touch_event(
    InputManager* input,
    touch_id_t    id,
    TouchChange   change,
    vec2d         window_pos,
    vec2d         window_dx,
    double        force)
{
    return {EventResponse::CONSUME, GestureState::CONTINUE};
}


// ScrollGestureHandler

void ScrollGestureHandler::begin_gesture(InputManager* input, GestureStart start) {
    // do nothing
}

void ScrollGestureHandler::end_gesture(InputManager* input, GestureFinish finish) {
    // do nothing
}

GestureResponse ScrollGestureHandler::scroll_event(
    InputManager* input,
    vec2d         scroll,
    vec2d         scroll_dx)
{
    return {EventResponse::CONSUME, GestureState::CONTINUE};
}


} // namespace behold
