/* the below header is a single-file include.
 * we need to enable its definitions in exactly one translation unit. let it be this one.
 */

// for the WEBGPU_BACKEND_WGPU macro:
#include <stereo/defs.h>

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>
