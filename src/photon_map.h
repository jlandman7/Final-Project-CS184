// photon_map.h
#pragma once
#include <vector>
#include <glm/glm.hpp>

struct PhotonNode {
    glm::vec3 position;
    glm::vec3 direction; // Refracted direction
    glm::vec3 energy;    // Power of the photon
    
    int split_axis;      // 0 for X, 1 for Y, 2 for Z
    int left_child;      // Index in the vector, -1 if leaf
    int right_child;     // Index in the vector, -1 if leaf
};

class PhotonMap {
public:
    PhotonMap() = default;

    // Call this during your emission loop to store raw hits
    void add_photon(const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& energy);

    // Call this exactly once after emission is complete to balance the tree
    void build();

    // Call this during the rendering pass to find nearby photons
    void gather(const glm::vec3& surface_pos, float gather_radius, 
                std::vector<const PhotonNode*>& results) const;

    const std::vector<PhotonNode>& get_nodes() const { return nodes; }
    int get_root() const { return root_index; }

private:
    std::vector<PhotonNode> nodes;
    int root_index = -1;

    // Recursive builder
    int balance_segment(int start, int end);
    
    // Recursive search
    void gather_recursive(int node_idx, const glm::vec3& pos, float radius_sq, 
                          std::vector<const PhotonNode*>& results) const;
};