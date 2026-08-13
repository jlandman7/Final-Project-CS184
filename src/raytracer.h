// raytracer.h
#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "cornell_box.h"
#include "water_mesh.h"
#include "water_simulation.h"

// Forward declaration
class PhotonMapper;

struct Triangle {
    glm::vec3 v0, v1, v2;
    glm::vec3 n0, n1, n2;
    glm::vec3 color;
    glm::vec3 centroid() const { return (v0 + v1 + v2) * (1.0f / 3.0f); }
};

struct AABB {
    glm::vec3 min = glm::vec3(1e30f);
    glm::vec3 max = glm::vec3(-1e30f);
    void grow(const glm::vec3& p) { min = glm::min(min, p); max = glm::max(max, p); }
    void grow(const AABB& b) { min = glm::min(min, b.min); max = glm::max(max, b.max); }
};

struct BVHNode {
    AABB bounds;
    int left_child = -1;
    int first_tri = 0;
    int tri_count = 0;
    bool is_leaf() const { return tri_count > 0; }
};

struct HitRecord {
    float t = 1e30f;
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    bool hit = false;
};

class Raytracer {
public:
    Raytracer(const CornellBox& box, const WaterSimulation& sim);

    void setup_camera(const glm::mat4& view_mat, const glm::mat4& proj_mat);
    void sync_geometry(const CornellBox& box, const WaterMesh& water);

    bool intersect_water_dda(const glm::vec3& origin, const glm::vec3& dir, 
                            glm::vec3& hit_pos, glm::vec3& hit_normal) const;

    void intersect_bvh(const glm::vec3& ray_orig, const glm::vec3& ray_dir, 
                       const glm::vec3& inv_dir, int node_idx, HitRecord& rec) const;

    void render_frame(std::vector<glm::vec4>& hdr_buffer, int width, int height,
                       const glm::vec3& camera_pos, const glm::mat4& view_mat, 
                       const glm::mat4& proj_mat, const PhotonMapper& photon_mapper);

    glm::vec3 trace_ray(const glm::vec3& origin, const glm::vec3& dir, 
                        const PhotonMapper& photon_mapper, int depth = 0) const;

private:
    const CornellBox& cornell_box;
    const WaterSimulation& water_sim;

    std::vector<Triangle> static_triangles; // Cornell box

    std::vector<BVHNode> bvh_nodes;
    int nodes_used = 0;
    bool bvh_built = false;

    glm::mat4 inv_view;
    glm::mat4 inv_proj;
    
    float get_height_bilinear(float wx, float wz) const;

    glm::vec3 compute_caustic_intensity(const glm::vec3& hit_pos, 
                                        const PhotonMapper& photon_mapper) const;

    void build_bvh();
    void update_node_bounds(int node_idx);
    void subdivide(int node_idx);
    bool intersect_aabb(const AABB& aabb, const glm::vec3& ray_orig, const glm::vec3& inv_dir, float ray_t) const;
    void intersect_triangle(const glm::vec3& ray_orig, const glm::vec3& ray_dir, const Triangle& tri, HitRecord& rec) const;
};