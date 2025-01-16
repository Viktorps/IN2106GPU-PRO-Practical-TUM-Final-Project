#pragma once
#include <tga/tga.hpp>
#include <tga/tga_math.hpp>
#include "perlinNoise.hpp"

struct Voxel {
    int type; // 0: empty, 1: dirt, 2: grass, etc.
    bool visible; // If the voxel is exposed to air.
    bool isSource = false;          // True if this voxel is a source block
    int sourceID = -1;           // ID of the source block that created this water
    int tickCounter = 0;
};

// Helper for 2D chunk coordinate keys
struct ChunkKey {
    int x, z;

    bool operator==(const ChunkKey& other) const {
        return x == other.x && z == other.z;
    }
};

// Custom hash for ChunkKey to use in unordered_map
struct ChunkKeyHasher {
    std::size_t operator()(const ChunkKey& key) const {
        return std::hash<int>()(key.x) ^ std::hash<int>()(key.z);
    }
};

constexpr int CHUNK_SIZE = 16;
constexpr int CHUNK_SIZE_Y = 48;


class Chunk {

public:
    Voxel voxels[CHUNK_SIZE][CHUNK_SIZE_Y][CHUNK_SIZE];
    glm::vec3 position; // Position of the chunk in world coordinates
    ChunkKey key; 
    bool isGenerated;
    bool isDirty;

    // Retrieves the type of a voxel at local chunk-relative coordinates
    int getVoxelType(int x, int y, int z) const {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE_Y || z < 0 || z >= CHUNK_SIZE) {
            // Out of bounds
            return 0; // Assume empty if out of bounds
        }
        return voxels[x][y][z].type;
    }

    // Retrieves the type of a voxel at world-relative coordinates
    int getVoxelTypeWorld(const glm::vec3& worldPosition) const {
        glm::vec3 localPosition = worldPosition - position; // Convert to local chunk-relative coordinates
        int x = static_cast<int>(localPosition.x);
        int y = static_cast<int>(localPosition.y);
        int z = static_cast<int>(localPosition.z);

        return getVoxelType(x, y, z);
    }
};
