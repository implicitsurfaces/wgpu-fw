#pragma once

#include <stereo/gpu/model.h>

namespace stereo {

// Load a wavefront OBJ file and add it to the model.
// return the range of primitive ids added
range1i add_model(Model& model, std::string_view filename);

} // namespace stereo
