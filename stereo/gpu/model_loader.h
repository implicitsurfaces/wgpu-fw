#pragma once

#include <stereo/gpu/model.h>

namespace stereo {

// Load a wavefront OBJ file and add it to the model.
Model add_model(Model& model, std::string_view filename);

} // namespace stereo
