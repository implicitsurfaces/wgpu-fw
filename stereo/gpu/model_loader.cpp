#include "geomc/Hash.h"
#include <fstream>
#include <sstream>
#include <stereo/gpu/model_loader.h>

// stg, c++ is dumb as hell for not providing this shit:
template <>
struct std::hash<std::tuple<uint32_t, uint32_t, uint32_t>> {
    size_t operator()(const std::tuple<uint32_t, uint32_t, uint32_t>& t) const {
        return geom::hash_many(
            (size_t) 0x4366c0eb0fb4ac79ULL, // nonce
            std::get<0>(t),
            std::get<1>(t),
            std::get<2>(t)
        );
    }
};

namespace stereo {

using VertKey = std::tuple<uint32_t, uint32_t, uint32_t>;

range1i add_model(Model& model, std::string_view filename) {
    std::ifstream in(filename);
    if (!in) {
        std::cerr << "Cannot open OBJ file: " << filename << std::endl;
        return range1i::empty;
    }

    // Temporary storage for positions, normals, and texture coordinates
    std::vector<vec3> temp_positions;
    std::vector<vec3> temp_normals;
    std::vector<vec2> temp_uvs;

    // Cache to avoid duplicate vertices
    std::unordered_map<VertKey, uint32_t> vert_cache;

    std::string line;
    Model::Prim prim;
    prim.index_range.lo = 0;

    // list of prims IDs created
    gpu_size_t last_prim_id = model.prims.size() - 1;

    while (std::getline(in, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            // Vertex position
            vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            temp_positions.push_back(pos);
        } else if (prefix == "vt") {
            // Texture coordinate
            vec2 uv;
            iss >> uv.x >> uv.y;
            temp_uvs.push_back(uv);
        } else if (prefix == "vn") {
            // Vertex normal
            vec3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            temp_normals.push_back(normal);
        } else if (prefix == "f") {
            // Face definition
            std::vector<uint32_t> face_indices;
            std::string vertex_str;
            while (iss >> vertex_str) {
                int pos_idx = 0, uv_idx = 0, norm_idx = 0;
                char* p = &vertex_str[0];

                // Parse indices (v/vt/vn)
                pos_idx = strtol(p, &p, 10);
                if (*p == '/') {
                    p++;
                    if (*p != '/') {
                        uv_idx = strtol(p, &p, 10);
                    }
                    if (*p == '/') {
                        p++;
                        norm_idx = strtol(p, &p, 10);
                    }
                }

                // Adjust negative indices and convert to zero-based
                auto adjust_index = [](int idx, int size) {
                    return idx > 0 ? idx - 1 : size + idx;
                };
                pos_idx  = adjust_index(pos_idx, temp_positions.size());
                uv_idx   = adjust_index(uv_idx, temp_uvs.size());
                norm_idx = adjust_index(norm_idx, temp_normals.size());

                // Create a unique key for the vertex
                auto key = std::make_tuple(pos_idx, uv_idx, norm_idx);

                // Check if the vertex is already in the cache
                auto it = vert_cache.find(key);
                uint32_t index;
                if (it != vert_cache.end()) {
                    index = it->second;
                } else {
                    // Create a new vertex
                    Model::Vert vert;
                    if (pos_idx >= 0 && pos_idx < temp_positions.size())
                        vert.p = temp_positions[pos_idx];
                    if (uv_idx >= 0 && uv_idx < temp_uvs.size())
                        vert.uv = temp_uvs[uv_idx];
                    if (norm_idx >= 0 && norm_idx < temp_normals.size())
                        vert.n = temp_normals[norm_idx];
                    model.verts.push_back(vert);
                    index = static_cast<uint32_t>(model.verts.size() - 1);
                    vert_cache[key] = index;
                }

                face_indices.push_back(index);
            }

            // Triangulate faces (assuming convex polygons)
            for (size_t i = 1; i + 1 < face_indices.size(); ++i) {
                model.indices.push_back(face_indices[0]);
                model.indices.push_back(face_indices[i]);
                model.indices.push_back(face_indices[i + 1]);
            }
        } else if (prefix == "g" || prefix == "o") {
            // Handle new group or object
            if (not model.indices.empty()) {
                prim.index_range.hi = static_cast<uint32_t>(model.indices.size()) - 1;
                model.prims.push_back(prim);
                prim.index_range.lo = prim.index_range.hi + 1;
            }
        }
    }

    // Add the last primitive
    if (!model.indices.empty()) {
        prim.index_range.hi = static_cast<uint32_t>(model.indices.size()) - 1;
        model.prims.push_back(prim);
    }

    return {
        static_cast<int32_t>(last_prim_id) + 1,
        static_cast<int32_t>(model.prims.size()) - 1
    };
}

}  // namespace stereo
