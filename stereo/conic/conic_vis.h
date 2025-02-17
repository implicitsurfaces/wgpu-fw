#pragma once

#include <numbers>

#include <stereo/gpu/model.h>
#include <stereo/conic/conic.h>

namespace stereo {

class ConicVisualizer {
public:
    ConicVisualizer() = default;

    /**
     * @brief Build a Model containing a triangle strip that approximates the conic.
     *
     * @param conic       The conic object to visualize.
     * @param n_samples   Number of segments (e.g., 128).
     * @param thickness   Half-width of the "ribbon" used to draw the curve (in the same units as the conic).
     * @param angle_start Starting angle in radians for sampling the unit cone angle.
     * @param angle_end   Ending angle in radians for sampling the unit cone angle.
     * @return            The ID of the primitive created.
     *
     * Note: For hyperbolas or partial conics, you may want a smaller angular range
     * or more advanced logic to pick which portion of the unit cone to sample.
     */
    size_t add_conic_section(
        ModelRef model,
        const Conic<float>& conic,
        size_t n_samples    = 128,
        float thickness     = 0.01f,
        range angle_range   = {0.f, 2.f * std::numbers::pi_v<float>}
    );
    
    size_t build_cone(
        ModelRef model,
        const Conic<float>& conic,
        size_t n_samples,
        range  height,
        range  angle_range
    );
};

} // namespace stereo
