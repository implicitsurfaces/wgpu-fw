#pragma once

#include <stereo/defs.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stereo/gpu/texture.h>
#include <stereo/gpu/input.h>


namespace stereo {


/// Names for coordinate systems for identifying screen locations.
enum class CoordSpace {
    /// Window pixel coordinates: (0, 0) : (window_width, window_height).
    /// These are coordinates used by the operating system to identify screen positions,
    /// and in HighDPI environments, may include fractional pixels. (0,0) is the
    /// upper left-hand corner.
    WINDOW_COORDS,
    /// Buffer pixel coordinates: (0, 0) : (buffer_width, buffer_height).
    /// These coordinates identify actual pixel locations in the screen buffer, and may
    /// be different from window coordinates if the screen pixel ratio is different from 1.
    /// (0,0) is the lower left-hand corner.
    BUFFER_COORDS,
    /// Window NDC coordinates: (-1, -1) : (1, 1). (0,0) is the center of the screen, and
    /// (1,1) is the upper right hand corner.
    NDC_COORDS,
    /// Aspect-preserving coordinates: (-1, -aspect / 2) : (1, aspect / 2).
    /// These coordinates keep X in the range (-1, 1), and Y in a range derived from
    /// the aspect ratio. (0,0) is the center of the screen, and (1, aspect / 2) is the
    /// upper right hand corner.
    ASPECT_COORDS
};


struct Window {
    GLFWwindow*          window         = nullptr;
    wgpu::Instance       instance       = nullptr;
    wgpu::Device         device         = nullptr;
    wgpu::Queue          queue          = nullptr;
    wgpu::Surface        surface        = nullptr;
	wgpu::TextureFormat  surface_format = wgpu::TextureFormat::Undefined;
	Texture              depth_buffer;
	wgpu::TextureView    depth_view     = nullptr;
	vec2ui               window_dims    = {1024, 1024};
	vec2ui               surface_dims   = {1024, 1024};
	
	InputManager         input_manager;
    
private:
    
    void _rebuild_depth_buffer();

public:
	
    Window(wgpu::Instance instance, wgpu::Device device, vec2ui dims);
    ~Window();
    
    void configure_surface(vec2ui dims);
    wgpu::TextureView next_target();
    
    /**
     * @brief Convert window position coordinates between various spaces.
     * 
     * @param coords The position vector to convert
     * @param from_coord The coordinate system in which `coords` is expressed.
     * @param to_coord The coordinate system into which `coords` should be converted.
     */
    vec2d convert_window_coordinates(
            vec2d coords,
            CoordSpace from_coord,
            CoordSpace to_coord) const;
};

}  // namespace stereo
