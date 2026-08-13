// photon_map.cpp
#include "photon_map.h"
#include <algorithm>

void PhotonMap::add_photon(const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& energy) {
    PhotonNode node;
    node.position = pos;
    node.direction = dir;
    node.energy = energy;
    node.left_child = -1;
    node.right_child = -1;
    nodes.push_back(node);
}

void PhotonMap::build() {
    if (nodes.empty()) return;
    root_index = balance_segment(0, nodes.size() - 1);
}

int PhotonMap::balance_segment(int start, int end) {
    if (start > end) return -1;

    // Find the bounding box of the current segment to determine the split axis
    glm::vec3 min_pt = nodes[start].position;
    glm::vec3 max_pt = nodes[start].position;
    for (int i = start + 1; i <= end; ++i) {
        min_pt = glm::min(min_pt, nodes[i].position);
        max_pt = glm::max(max_pt, nodes[i].position);
    }

    glm::vec3 extent = max_pt - min_pt;
    
    int axis = 0;
    if (extent.y > extent.x && extent.y > extent.z) axis = 1;
    else if (extent.z > extent.x && extent.z > extent.y) axis = 2;

    // Partition the array so the median element is in the correct place
    int median = start + (end - start) / 2;
    std::nth_element(nodes.begin() + start, nodes.begin() + median, nodes.begin() + end + 1,
        [axis](const PhotonNode& a, const PhotonNode& b) {
            return a.position[axis] < b.position[axis];
        });

    // Link the children
    nodes[median].split_axis = axis;
    nodes[median].left_child = balance_segment(start, median - 1);
    nodes[median].right_child = balance_segment(median + 1, end);

    return median;
}

void PhotonMap::gather(const glm::vec3& surface_pos, float gather_radius, 
                       std::vector<const PhotonNode*>& results) const {
    if (root_index == -1) return;
    float radius_sq = gather_radius * gather_radius;
    gather_recursive(root_index, surface_pos, radius_sq, results);
}

void PhotonMap::gather_recursive(int node_idx, const glm::vec3& pos, float radius_sq, 
                                 std::vector<const PhotonNode*>& results) const {
    if (node_idx == -1) return;

    const PhotonNode& node = nodes[node_idx];
    
    // Check distance to current photon
    glm::vec3 diff = node.position - pos;
    float dist_sq = glm::dot(diff, diff);
    
    if (dist_sq <= radius_sq) {
        results.push_back(&node);
    }

    // Determine which child to visit first based on the splitting plane
    int axis = node.split_axis;
    float plane_dist = pos[axis] - node.position[axis];
    
    int first_child = plane_dist < 0.0f ? node.left_child : node.right_child;
    int second_child = plane_dist < 0.0f ? node.right_child : node.left_child;

    // Always visit the side the target point is on
    gather_recursive(first_child, pos, radius_sq, results);

    // Only visit the other side if the search sphere intersects the splitting plane
    if (plane_dist * plane_dist <= radius_sq) {
        gather_recursive(second_child, pos, radius_sq, results);
    }
}