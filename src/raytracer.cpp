#define GLM_ENABLE_EXPERIMENTAL
#include "raytracer.h"
#include "photon_mapper.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <iostream>

static bool refract_ray(const glm::vec3& I, const glm::vec3& N, float eta, glm::vec3& refracted) {
    float cos_i = -glm::dot(N, I);
    float sin2_t = eta * eta * (1.0f - cos_i * cos_i);
    
    // Total Internal Reflection
    if (sin2_t > 1.0f) return false; 

    float cos_t = std::sqrt(1.0f - sin2_t);
    refracted = eta * I + (eta * cos_i - cos_t) * N;
    return true;
}

Raytracer::Raytracer(const CornellBox& box, const WaterSimulation& sim)
    : cornell_box(box), water_sim(sim) {
        (void)cornell_box; // Silences private member unused warning
}

void Raytracer::setup_camera(const glm::mat4& view_mat, const glm::mat4& proj_mat) {
    this->inv_view = glm::inverse(view_mat);
    this->inv_proj = glm::inverse(proj_mat);
}

void Raytracer::sync_geometry(const CornellBox& box, const WaterMesh& water) {
    (void)water; // Silences unused parameter warning
    
    // Build Static BVH for Cornell Box ONCE
    if (!bvh_built) {
        static_triangles.clear();
        const Mesh& box_mesh = box.get_mesh();
        for (size_t i = 0; i < box_mesh.indices.size(); i += 3) {
            Triangle t;
            t.v0 = box_mesh.positions[box_mesh.indices[i]];
            t.v1 = box_mesh.positions[box_mesh.indices[i+1]];
            t.v2 = box_mesh.positions[box_mesh.indices[i+2]];
            
            t.n0 = box_mesh.normals[box_mesh.indices[i]];
            t.n1 = box_mesh.normals[box_mesh.indices[i+1]];
            t.n2 = box_mesh.normals[box_mesh.indices[i+2]];
            
            t.color = box_mesh.colors[box_mesh.indices[i]]; 
            static_triangles.push_back(t);
        }
        build_bvh();
        bvh_built = true;
    }
}

void Raytracer::build_bvh() {
    bvh_nodes.resize(static_triangles.size() * 2 - 1);
    nodes_used = 1;

    BVHNode& root = bvh_nodes[0];
    root.left_child = -1;
    root.first_tri = 0;
    root.tri_count = static_triangles.size();

    update_node_bounds(0);
    subdivide(0);
}

void Raytracer::update_node_bounds(int node_idx) {
    BVHNode& node = bvh_nodes[node_idx];
    node.bounds = AABB();
    for (int i = 0; i < node.tri_count; ++i) {
        const Triangle& tri = static_triangles[node.first_tri + i];
        node.bounds.grow(tri.v0);
        node.bounds.grow(tri.v1);
        node.bounds.grow(tri.v2);
    }
}

void Raytracer::subdivide(int node_idx) {
    BVHNode& node = bvh_nodes[node_idx];
    if (node.tri_count <= 2) return; 

    glm::vec3 extent = node.bounds.max - node.bounds.min;
    int axis = 0;
    if (extent.y > extent.x) axis = 1;
    if (extent.z > extent[axis]) axis = 2;

    float split_pos = node.bounds.min[axis] + extent[axis] * 0.5f;

    int i = node.first_tri;
    int j = i + node.tri_count - 1;
    while (i <= j) {
        if (static_triangles[i].centroid()[axis] < split_pos) {
            i++;
        } else {
            std::swap(static_triangles[i], static_triangles[j--]);
        }
    }

    int left_count = i - node.first_tri;
    if (left_count == 0 || left_count == node.tri_count) return;

    int left_child_idx = nodes_used++;
    int right_child_idx = nodes_used++;

    bvh_nodes[left_child_idx].first_tri = node.first_tri;
    bvh_nodes[left_child_idx].tri_count = left_count;
    
    bvh_nodes[right_child_idx].first_tri = i;
    bvh_nodes[right_child_idx].tri_count = node.tri_count - left_count;
    
    node.left_child = left_child_idx;
    node.tri_count = 0; 

    update_node_bounds(left_child_idx);
    update_node_bounds(right_child_idx);
    
    subdivide(left_child_idx);
    subdivide(right_child_idx);
}

void Raytracer::intersect_triangle(const glm::vec3& ray_orig, const glm::vec3& ray_dir, const Triangle& tri, HitRecord& rec) const {
    glm::vec3 edge1 = tri.v1 - tri.v0;
    glm::vec3 edge2 = tri.v2 - tri.v0;
    glm::vec3 h = glm::cross(ray_dir, edge2);
    float a = glm::dot(edge1, h);

    if (a > -1e-6f && a < 1e-6f) return; 

    float f = 1.0f / a;
    glm::vec3 s = ray_orig - tri.v0;
    float u = f * glm::dot(s, h);
    if (u < 0.0f || u > 1.0f) return;

    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(ray_dir, q);
    if (v < 0.0f || u + v > 1.0f) return;

    float t = f * glm::dot(edge2, q);
    if (t > 1e-4f && t < rec.t) { 
        rec.t = t;
        rec.hit = true;
        rec.position = ray_orig + ray_dir * t;
        
        float w = 1.0f - u - v;
        rec.normal = glm::normalize(tri.n0 * w + tri.n1 * u + tri.n2 * v);
        rec.color = tri.color;
    }
}

bool Raytracer::intersect_aabb(const AABB& aabb, const glm::vec3& ray_orig, const glm::vec3& inv_dir, float ray_t) const {
    glm::vec3 t0 = (aabb.min - ray_orig) * inv_dir;
    glm::vec3 t1 = (aabb.max - ray_orig) * inv_dir;
    
    glm::vec3 tmin = glm::min(t0, t1);
    glm::vec3 tmax = glm::max(t0, t1);
    
    float tnear = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
    float tfar = glm::min(glm::min(tmax.x, tmax.y), tmax.z);
    
    return tfar >= tnear && tfar > 0.0f && tnear < ray_t;
}

void Raytracer::intersect_bvh(const glm::vec3& ray_orig, const glm::vec3& ray_dir, const glm::vec3& inv_dir, int node_idx, HitRecord& rec) const {
    const BVHNode& node = bvh_nodes[node_idx];
    if (!intersect_aabb(node.bounds, ray_orig, inv_dir, rec.t)) return;

    if (node.is_leaf()) {
        for (int i = 0; i < node.tri_count; ++i) {
            intersect_triangle(ray_orig, ray_dir, static_triangles[node.first_tri + i], rec);
        }
    } else {
        intersect_bvh(ray_orig, ray_dir, inv_dir, node.left_child, rec);
        intersect_bvh(ray_orig, ray_dir, inv_dir, node.left_child + 1, rec);
    }
}

// Phase 1: Smooth Bilinear Surface Sampler
float Raytracer::get_height_bilinear(float wx, float wz) const {
    float domain = water_sim.get_domain_size();
    
    // Clamp world position to water boundary margins
    wx = std::clamp(wx, 0.001f, domain - 0.001f);
    wz = std::clamp(wz, 0.001f, domain - 0.001f);

    int res_x = 128; // Grid resolution
    int res_z = 128;

    // Convert world float to continuous grid space
    float gx = (wx / domain) * (res_x - 1);
    float gz = (wz / domain) * (res_z - 1);

    int x0 = static_cast<int>(std::floor(gx));
    int z0 = static_cast<int>(std::floor(gz));
    int x1 = std::min(x0 + 1, res_x - 1);
    int z1 = std::min(z0 + 1, res_z - 1);

    float u = gx - x0;
    float v = gz - z0;

    // Sample 4 grid points
    float h00 = water_sim.get_height(x0, z0);
    float h10 = water_sim.get_height(x1, z0);
    float h01 = water_sim.get_height(x0, z1);
    float h11 = water_sim.get_height(x1, z1);

    // Bilinear blend formula
    return (1.0f - u) * (1.0f - v) * h00 +
           u * (1.0f - v) * h10 +
           (1.0f - u) * v * h01 +
           u * v * h11;
}

bool Raytracer::intersect_water_dda(const glm::vec3& origin, const glm::vec3& dir, 
                                    glm::vec3& hit_pos, glm::vec3& hit_normal) const {
    if (std::abs(dir.y) < 1e-5f) return false;

    float domain = water_sim.get_domain_size();

    // 1. Scan grid min/max heights
    float y_min = 0.4f, y_max = 0.4f;
    int res_x = 128, res_z = 128;
    for (int x = 0; x < res_x; x += 8) {
        for (int z = 0; z < res_z; z += 8) {
            float h = water_sim.get_height(x, z);
            y_min = std::min(y_min, h);
            y_max = std::max(y_max, h);
        }
    }
    y_min -= 0.01f;
    y_max += 0.01f;

    // 2. Intersect ray with full 3D AABB bounding box of the water domain
    glm::vec3 box_min(0.0f, y_min, 0.0f);
    glm::vec3 box_max(domain, y_max, domain);

    glm::vec3 inv_dir = 1.0f / dir;
    glm::vec3 t0 = (box_min - origin) * inv_dir;
    glm::vec3 t1 = (box_max - origin) * inv_dir;

    glm::vec3 tmin = glm::min(t0, t1);
    glm::vec3 tmax = glm::max(t0, t1);

    float t_entry = std::max(std::max(tmin.x, tmin.y), tmin.z);
    float t_exit  = std::min(std::min(tmax.x, tmax.y), tmax.z);

    t_entry = std::max(t_entry, 0.0f);
    if (t_entry >= t_exit) return false; // Ray missed the water volume entirely

    // 3. March through the valid domain intersection range
    const int num_steps = 32;
    float step_size = (t_exit - t_entry) / static_cast<float>(num_steps);
    
    // Start marching slightly inside the entry point
    float t_curr = t_entry;
    glm::vec3 pt = origin + dir * t_curr;
    float prev_t = t_curr;
    float prev_h_diff = pt.y - get_height_bilinear(pt.x, pt.z);

    bool found = false;
    for (int i = 0; i < num_steps; ++i) {
        t_curr += step_size;
        pt = origin + dir * t_curr;

        float h = get_height_bilinear(pt.x, pt.z);
        float h_diff = pt.y - h;

        if (prev_h_diff > 0.0f && h_diff <= 0.0f) {
            found = true;
            break;
        }

        prev_t = t_curr;
        prev_h_diff = h_diff;
    }

    if (!found) return false;

    // 4. Binary search refinement
    float t_low = prev_t;
    float t_high = t_curr;
    for (int b = 0; b < 8; ++b) {
        float t_mid = 0.5f * (t_low + t_high);
        glm::vec3 test_pt = origin + dir * t_mid;
        float h_diff = test_pt.y - get_height_bilinear(test_pt.x, test_pt.z);

        if (h_diff > 0.0f) t_low = t_mid;
        else t_high = t_mid;
    }

    float t_final = 0.5f * (t_low + t_high);
    hit_pos = origin + dir * t_final;

    // 5. Normal calculation using smooth bilinear gradient
    float eps = domain / (2.0f * res_x);
    float h_x1 = get_height_bilinear(hit_pos.x + eps, hit_pos.z);
    float h_x0 = get_height_bilinear(hit_pos.x - eps, hit_pos.z);
    float h_z1 = get_height_bilinear(hit_pos.x, hit_pos.z + eps);
    float h_z0 = get_height_bilinear(hit_pos.x, hit_pos.z - eps);

    hit_normal = glm::normalize(glm::vec3(h_x0 - h_x1, 2.0f * eps, h_z0 - h_z1));

    return true;
}

void Raytracer::render_frame(std::vector<glm::vec4>& hdr_buffer, int width, int height,
                            const glm::vec3& camera_pos, const glm::mat4& view_mat, 
                            const glm::mat4& proj_mat, const PhotonMapper& photon_mapper) {
    (void)view_mat;
    (void)proj_mat;

    float eta_air_to_water = 1.0f / 1.333f;
    float search_radius = 0.02f; 
    float search_radius_sq = search_radius * search_radius;
    float search_area = M_PI * search_radius_sq * 0.5f;

    #pragma omp parallel for collapse(2) schedule(dynamic)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Standard NDC mapping: y = 0 (top row) -> v = +1.0 (ceiling)
            float u = (2.0f * (x + 0.5f) / width) - 1.0f;
            float v = (2.0f * (y + 0.5f) / height) - 1.0f; // If still inverted, change to: 1.0f - (2.0f * (y + 0.5f) / height);

            // Unproject ray into world space
            glm::vec4 target = inv_proj * glm::vec4(u, v, 1.0f, 1.0f);
            glm::vec3 eye_dir = glm::normalize(glm::vec3(target) / target.w);
            glm::vec3 ray_dir = glm::normalize(glm::vec3(inv_view * glm::vec4(eye_dir, 0.0f)));

            glm::vec3 water_hit_pos, water_hit_normal;
            glm::vec3 final_color(0.0f);

            if (intersect_water_dda(camera_pos, ray_dir, water_hit_pos, water_hit_normal)) {
                // Ensure normal faces against incoming camera ray
                glm::vec3 N = water_hit_normal;
                if (glm::dot(ray_dir, N) > 0.0f) N = -N;

                glm::vec3 refr_dir;
                if (refract_ray(ray_dir, N, eta_air_to_water, refr_dir)) {
                    HitRecord floor_hit;
                    glm::vec3 inv_refr_dir = 1.0f / refr_dir;
                    
                    // Trace down to floor/walls below water level
                    intersect_bvh(water_hit_pos + refr_dir * 0.001f, refr_dir, inv_refr_dir, 0, floor_hit);

                    if (floor_hit.hit) {
                        auto near_photons = photon_mapper.find_near_photons(floor_hit.position, search_radius);
                        glm::vec3 accumulated_power(0.0f);

                        for (const auto& p : near_photons) {
                            glm::vec3 diff = p.position - floor_hit.position;
                            float dist_sq = glm::dot(diff, diff);
                            float weight = 1.0f - (dist_sq / search_radius_sq);
                            accumulated_power += p.power * std::max(0.0f, weight);
                        }

                        glm::vec3 density = accumulated_power / search_area;
                        glm::vec3 water_tint(0.85f, 0.95f, 1.0f);
                        
                        // Base ambient = 0.8f so unlit underwater floor matches scene brightness
                        final_color = floor_hit.color * water_tint * (0.8f + density);
                    }
                }
            } else {
                // Primary ray hits walls/ceiling directly
                HitRecord box_hit;
                glm::vec3 inv_dir = 1.0f / ray_dir;
                intersect_bvh(camera_pos, ray_dir, inv_dir, 0, box_hit);
                if (box_hit.hit) {
                    final_color = box_hit.color;
                }
            }

            // Direct index write (y = 0 is top row in image memory)
            hdr_buffer[y * width + x] = glm::vec4(final_color, 1.0f);
        }
    }
}