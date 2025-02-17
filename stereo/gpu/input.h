#pragma once

#include <shared_mutex>
#include <stereo/defs.h>

namespace stereo {

// fwd decl
struct Window;

using touch_id_t = uint64_t;

#ifdef __APPLE__
    #define GLFW_MOD_COMMAND GLFW_MOD_SUPER
#else
    #define GLFW_MOD_COMMAND GLFW_MOD_CONTROL
#endif

// to be used as a GLFW modifier mask bit with mouse events
// take the highest GLFW modifier and shift it a few times for safety.
constexpr int MOD_DOUBLECLICK = GLFW_MOD_NUM_LOCK << 3;
// maximum double click delay, in seconds
constexpr double DOUBLE_CLICK_THRESHOLD = 0.250;


/// Category of touch event
enum TouchChange {
    BEGIN,
    MOVE,
    END,
    CANCEL,
};

struct ButtonState {
    /// Whether this button is currently pressed
    bool   is_down;
    /// Position of the cursor when this button was last pressed,
    /// in ratio-preserving window coordinates (ASPECT_COORDS).
    vec2d  down_pos;
    /// Accumulated translation while this button has been pressed.
    vec2d  pan;
    /// Modifiers that were active when this button was pressed.
    int    down_modifiers;
    /// Time at which the button was last pressed.
    double last_down;
    
    constexpr ButtonState():
        is_down        (false),
        down_pos       (0.),
        pan            (0.),
        down_modifiers (0),
        last_down      (0) {}
};


struct Touch : public ButtonState {
    /// Current position (in WINDOW coords) of the touch.
    vec2d pos;
    /// The force of the touch
    double force;
    /// A matrix encoding a representative area of the touch.
    /// A circle of radius 1 drawn at the origin, transformed by `patch`, and translated
    // to `pan` position will cover the area of the touch.
    mat2d patch;
    
    Touch():
        pos   (0.),
        force (0.),
        patch () {}
};


using touch_index_t = DenseMap<touch_id_t, Touch>;


struct Input {
    /// Current position of the cursor, in normalized aspect-preserving window coordinates.
    vec2d cursor_pos;
    /// Accumulated change in cursor position. Reset to zero to consume the delta.
    vec2d cursor_dx;
    /// Current scroll position.
    vec2d scroll;
    /// Accumulated change in scroll position. Reset to zero to consume the delta.
    vec2d scroll_dx;
    /// Button states for all available GLFW buttons.
    ButtonState buttons[GLFW_MOUSE_BUTTON_LAST + 1];
    /// Active touches
    touch_index_t touches;
};


////////// Event responses //////////

enum struct EventResponse {
    /// The event was not consumed and should be propagated to other handlers.
    PASS,
    /// The event was consumed and propagation should stop.
    CONSUME,
};

enum struct GestureState {
    /// Continue the gesture
    CONTINUE,
    /// Finish the gesture
    FINISH,
    /// Cancel the gesture
    CANCEL,
};

enum struct GestureStart {
    /// The gesture was accepted.
    ACCEPT,
    /// The gesture was rejected; i.e. by another in-progress gesture.
    REJECT
};

enum struct GestureFinish {
    /// Gesture completed normally.
    FINISH,
    /// Gesture was aborted.
    CANCEL,
};

struct GestureResponse {
    /// Whether to propagate the event to other handlers.
    EventResponse response;
    /// Whether to continue the gesture, finish it, or cancel it.
    GestureState  state;
};


struct InputManager;
struct InputHandler;
struct MouseGestureHandler;
struct TouchGestureHandler;
struct ScrollGestureHandler;

using MouseGestureHandlerRef  = std::shared_ptr<MouseGestureHandler>;
using TouchGestureHandlerRef  = std::shared_ptr<TouchGestureHandler>;
using ScrollGestureHandlerRef = std::shared_ptr<ScrollGestureHandler>;

using MouseResponse  = std::variant<EventResponse, MouseGestureHandlerRef>;
using TouchResponse  = std::variant<EventResponse, TouchGestureHandlerRef>;
using ScrollResponse = std::variant<EventResponse, ScrollGestureHandlerRef>;


////////// Handlers //////////

// these are classes that client code should derive from to handle input events.

struct InputHandler {
    
    virtual ~InputHandler();
    
    /// Process a framebuffer resize event, in buffer pixel coordinates.
    virtual EventResponse framebuffer_resize_event(
        InputManager* input,
        vec2ui old_dims,
        vec2ui new_dims
    );
    
    /// Process a window resize event, in window coordinates,
    virtual EventResponse window_resize_event(
        InputManager* input,
        vec2ui old_dims,
        vec2ui new_dims
    );
    
    /// Process cursor movement, in signed-normalized uniform-aspect window coordinates.
    virtual MouseResponse cursor_move_event(
        InputManager* input,
        vec2d dx,
        vec2d new_pos
    );
    
    /// Process mouse button events. In addition to the standard GLFW button codes,
    /// double click events are reported with modifier `MOD_DOUBLECLICK`. A gesture
    /// may be initiated by returning a MouseGestureHandlerRef.
    virtual MouseResponse mouse_button_event(
        InputManager* input,
        int button,
        int action,
        int mods
    );
    
    /// Process double click events. The (signed, normalized, uniform-aspect) window
    /// position of each click is reported. A gesture may be initiated by returning
    /// a MouseGestureHandlerRef.
    virtual MouseResponse double_click_event(
        InputManager* input,
        int button,
        vec2d click_1,
        vec2d click_2
    );
    
    /// Process a keyboard event, using standard GLFW key event codes.
    virtual EventResponse key_event(
        InputManager* input,
        int key,
        int scancode,
        int action,
        int mods
    );
    
    /// Process a character being typed.
    virtual EventResponse text_event(
        InputManager* input,
        uint32_t codepoint
    );
    
    /// Process a 'paste text' event.
    virtual EventResponse paste_text_event(
        InputManager* input,
        std::string_view text
    );
    
    /// Process a scroll event, in GLFW scroll coordinates. A scroll gesture may be
    /// initiated by returning a ScrollGestureHandlerRef.
    virtual ScrollResponse scroll_event(
        InputManager* input,
        vec2d scroll,
        vec2d scroll_dx
    );
    
    /// Process a screen touch event, in window coordinates. A touch gesture may be
    /// initiated by returning a TouchGestureHandlerRef.
    virtual TouchResponse touch_event(
        InputManager* input,
        touch_id_t id,
        TouchChange change,
        vec2d window_pos,
        vec2d window_dx,
        double force
    );
    
};

/**
 * @brief A class for handling mouse gestures.
 * 
 * When a gesture begins, before any input events are sent to it, the handler is 
 * notified with `begin_gesture`. 
 * 
 * There can only be one gesture at a time. If an input handler tries to start a gesture
 * and one is already in progress, `begin_gesture` is called with `GestureStart::REJECT` on the
 * blocked gesture, and no further events are sent to the gesture handler, nor will
 * `end_gesture` be called.
 * 
 * Each event method returns a `GestureResponse` object, which contains an `EventResponse`
 * and a `GestureState`. The `EventResponse` indicates whether the event was consumed
 * by the gesture handler, and the `GestureState` indicates whether the gesture should
 * continue, finish, or cancel. Consumed events will not propagate to other handlers.
 */
struct MouseGestureHandler {
    
    virtual void begin_gesture(InputManager* input, GestureStart  start);
    virtual void   end_gesture(InputManager* input, GestureFinish finish);
    
    virtual GestureResponse mouse_button_event(
        InputManager* input,
        int button,
        int action,
        int mods
    );
    
    virtual GestureResponse cursor_move_event(
        InputManager* input,
        vec2d dx,
        vec2d new_pos
    );
    
    virtual GestureResponse double_click_event(
        InputManager* input,
        int button,
        vec2d click_1,
        vec2d click_2
    );
    
};

/**
 * @brief A class for handling touch gestures.
 * 
 * The conventions for TouchGestureHandler are the same as for MouseGestureHandler.
 */
struct TouchGestureHandler {
    virtual void begin_gesture(InputManager* input, GestureStart  start);
    virtual void   end_gesture(InputManager* input, GestureFinish finish);
    
    virtual GestureResponse touch_event(
        InputManager* input,
        touch_id_t id,
        TouchChange change,
        vec2d window_pos,
        vec2d window_dx,
        double force
    );
    
};

/**
 * @brief A class for handling scroll gestures.
 * 
 * The conventions for ScrollGestureHandler are the same as for MouseGestureHandler.
 */
struct ScrollGestureHandler {
    virtual void begin_gesture(InputManager* input, GestureStart  start);
    virtual void   end_gesture(InputManager* input, GestureFinish finish);
    
    virtual GestureResponse scroll_event(
        InputManager* input,
        vec2d scroll,
        vec2d scroll_dx
    );
    
};


////////// Manager //////////

/**
 * @brief A class for dispatching input events to handlers and managing gestures.
 * 
 * This class acts as an intermediary between the raw GLFW events and the input handlers.
 * Many input handlers can be added to this manager, and they will be called in order
 * until one accepts the event.
 * 
 * It is also possible to mock input events by calling `trigger_.*()` methods.
 */
struct InputManager {
    Input   input;
    Window* window;
    
    MouseGestureHandlerRef  mouse_gesture_handler  = nullptr;
    TouchGestureHandlerRef  touch_gesture_handler  = nullptr;
    ScrollGestureHandlerRef scroll_gesture_handler = nullptr;
    
    /// List of handlers, processed in forward order (lower indices have priority).
    std::vector<InputHandler*> handlers;

private:
    
    template <typename... Args>
    EventResponse _trigger_mouse_event(
        // member function of InputHandler, call it `handler_callback`:
        MouseResponse   (InputHandler::*handler_callback)(InputManager*, Args...),
        // member function of MouseGestureHandler, call it `gesture_callback`:
        GestureResponse (MouseGestureHandler::*gesture_callback)(InputManager*, Args...),
        // args to pass to the member functions:
        Args... args
    );
    
public:
    
    InputManager(Window* window);
    virtual ~InputManager();
    
    EventResponse trigger_framebuffer_resized(vec2ui new_dims);
    EventResponse trigger_window_resized(vec2ui new_dims);
    EventResponse trigger_cursor_moved(vec2d new_window_pos);
    EventResponse trigger_mouse_button(int button, int action, int mods);
    EventResponse trigger_double_click(int button, vec2d click_1, vec2d click_2);
    EventResponse trigger_key(int key, int scancode, int action, int mods);
    EventResponse trigger_text(uint32_t codepoint);
    EventResponse trigger_paste_text(std::string_view text);
    EventResponse trigger_scroll(vec2d scroll_dx);
    EventResponse trigger_touch(
        touch_id_t id,
        TouchChange change,
        vec2d window_pos,
        double force,
        mat2d patch
    );
};

} // namespace stereo
