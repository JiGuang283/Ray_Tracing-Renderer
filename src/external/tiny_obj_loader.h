#ifndef TINY_OBJ_LOADER_H_
#define TINY_OBJ_LOADER_H_

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace tinyobj {

struct index_t {
    int vertex_index = -1;
    int normal_index = -1;
    int texcoord_index = -1;
};

struct mesh_t {
    std::vector<index_t> indices;
    std::vector<unsigned char> num_face_vertices;
    std::vector<int> material_ids;
};

struct shape_t {
    mesh_t mesh;
    std::string name;
};

struct attrib_t {
    std::vector<double> vertices;
    std::vector<double> normals;
    std::vector<double> texcoords;
};

struct ObjReaderConfig {
    bool triangulate = true;
    std::string mtl_search_path;
};

class ObjReader {
  public:
    bool ParseFromFile(const std::string &filename,
                       const ObjReaderConfig &config = ObjReaderConfig()) {
        warning_.clear();
        error_.clear();

        attrib_ = attrib_t{};
        shapes_.clear();

        std::ifstream ifs(filename);
        if (!ifs) {
            error_ = "Cannot open file: " + filename;
            return false;
        }

        shape_t current_shape;
        current_shape.name = "default";

        std::string line;
        while (std::getline(ifs, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }

            std::stringstream ss(line);
            std::string keyword;
            ss >> keyword;

            if (keyword == "v") {
                double x, y, z;
                ss >> x >> y >> z;
                attrib_.vertices.push_back(x);
                attrib_.vertices.push_back(y);
                attrib_.vertices.push_back(z);
            } else if (keyword == "vn") {
                double x, y, z;
                ss >> x >> y >> z;
                attrib_.normals.push_back(x);
                attrib_.normals.push_back(y);
                attrib_.normals.push_back(z);
            } else if (keyword == "vt") {
                double u, v;
                ss >> u >> v;
                attrib_.texcoords.push_back(u);
                attrib_.texcoords.push_back(v);
            } else if (keyword == "f") {
                std::vector<index_t> face_indices;
                std::string vert_str;
                while (ss >> vert_str) {
                    face_indices.push_back(ParseFaceIndex(vert_str));
                }

                if (face_indices.size() < 3) {
                    warning_ = "Detected degenerate face";
                    continue;
                }

                if (config.triangulate && face_indices.size() > 3) {
                    for (size_t k = 1; k + 1 < face_indices.size(); ++k) {
                        current_shape.mesh.num_face_vertices.push_back(3);
                        current_shape.mesh.indices.push_back(face_indices[0]);
                        current_shape.mesh.indices.push_back(face_indices[k]);
                        current_shape.mesh.indices.push_back(
                            face_indices[k + 1]);
                    }
                } else {
                    current_shape.mesh.num_face_vertices.push_back(
                        static_cast<unsigned char>(face_indices.size()));
                    current_shape.mesh.indices.insert(current_shape.mesh.indices.end(),
                                                     face_indices.begin(),
                                                     face_indices.end());
                }
            }
        }

        if (!current_shape.mesh.indices.empty()) {
            shapes_.push_back(std::move(current_shape));
        }

        return true;
    }

    const attrib_t &GetAttrib() const {
        return attrib_;
    }

    const std::vector<shape_t> &GetShapes() const {
        return shapes_;
    }

    const std::string &Warning() const {
        return warning_;
    }

    const std::string &Error() const {
        return error_;
    }

  private:
    static index_t ParseFaceIndex(const std::string &token) {
        index_t idx;
        int vertex = 0, tex = 0, normal = 0;

        size_t first_slash = token.find('/');
        size_t second_slash = token.find('/', first_slash + 1);

        vertex = std::stoi(token.substr(0, first_slash));
        idx.vertex_index = vertex - 1;

        if (first_slash != std::string::npos) {
            if (second_slash == std::string::npos) {
                tex = std::stoi(token.substr(first_slash + 1));
                idx.texcoord_index = tex - 1;
            } else {
                if (second_slash > first_slash + 1) {
                    tex = std::stoi(token.substr(first_slash + 1,
                                                 second_slash - first_slash - 1));
                    idx.texcoord_index = tex - 1;
                }
                if (second_slash + 1 < token.size()) {
                    normal = std::stoi(token.substr(second_slash + 1));
                    idx.normal_index = normal - 1;
                }
            }
        }

        return idx;
    }

    attrib_t attrib_;
    std::vector<shape_t> shapes_;
    std::string warning_;
    std::string error_;
};

} // namespace tinyobj

#endif // TINY_OBJ_LOADER_H_
