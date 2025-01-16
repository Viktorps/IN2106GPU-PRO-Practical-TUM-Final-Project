#include "terrainManager.hpp"

#include <random>

namespace std {
    template <>
    struct hash<std::pair<int, int>> {
        size_t operator()(const std::pair<int, int>& pair) const {
            // Combine the hash of the two integers
            return std::hash<int>()(pair.first) ^ (std::hash<int>()(pair.second) << 1);
        }
    };
}

void TerrainManager::generateTerrain(Chunk& chunk, int chunkX, int chunkZ, PerlinNoise& perlin) {
    if (chunk.isGenerated) {
        return; // Skip regeneration for already generated chunks
    }

    const int sandThreshold = 15; // Threshold below which exposed grass blocks become sand

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> randomChance(0.0, 1.0);

    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            int worldX = chunkX * CHUNK_SIZE + x;
            int worldZ = chunkZ * CHUNK_SIZE + z;

            // Multi-layer noise for varied terrain
            double frequency = 0.05;
            double amplitude = 20.0;
            int height = 0;

            for (int octave = 0; octave < 4; ++octave) {
                height += static_cast<int>(perlin.noise(worldX * frequency, worldZ * frequency, 0.0) * amplitude);
                frequency *= 2.0;
                amplitude *= 0.5;
            }
            height += 20; // Base height
            height = std::clamp(height, 1, CHUNK_SIZE_Y - 1); // Clamp height

            // Set block types based on height
            for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
                if (y == 0) {
                    chunk.voxels[x][y][z].type = 3; // Bedrock
                    chunk.voxels[x][y][z].visible = true;
                } else if (y < height - 3) {
                    // Spawn diamonds rarely and more frequently the deeper the layer
                    if (randomChance(gen) < 0.01 * (1.0 - static_cast<double>(y) / CHUNK_SIZE_Y)) {
                        chunk.voxels[x][y][z].type = 4; // Diamond
                        chunk.voxels[x][y][z].visible = true;
                    } else {
                        chunk.voxels[x][y][z].type = 2; // Stone
                        chunk.voxels[x][y][z].visible = true;
                    }
                } else if (y < height) {
                    // Initially assign grass
                    chunk.voxels[x][y][z].type = 1; // Grass
                    chunk.voxels[x][y][z].visible = true;

                    // If the block is below the sand threshold and exposed, set to sand
                    if (y <= sandThreshold && isExposed(chunk, x, y, z)) {
                        chunk.voxels[x][y][z].type = 8; // Sand
                    }
                } else {
                    chunk.voxels[x][y][z].type = 0; // Air
                    chunk.voxels[x][y][z].visible = false;
                }
            }
            // Add trees on grass blocks
            if (height < CHUNK_SIZE_Y - 8 && height > sandThreshold && chunk.voxels[x][height - 1][z].type == 1 && randomChance(gen) < 0.02) {
                int treeHeight = 4 + (gen() % 3); // Random height between 4 and 6
                int trunkOffsetX = (gen() % 3) - 1; // Randomize trunk position by -1, 0, or 1
                int trunkOffsetZ = (gen() % 3) - 1;

                // Check if there's enough space for the tree
                bool spaceForTree = true;
                for (int h = 0; h < treeHeight; ++h) {
                    if (height + h >= CHUNK_SIZE_Y || 
                        x + trunkOffsetX < 0 || x + trunkOffsetX >= CHUNK_SIZE || 
                        z + trunkOffsetZ < 0 || z + trunkOffsetZ >= CHUNK_SIZE || 
                        chunk.voxels[x + trunkOffsetX][height + h][z + trunkOffsetZ].type != 0) {
                        spaceForTree = false;
                        break;
                    }
                }

                if (spaceForTree) {
                    // Place the trunk
                    for (int h = 0; h < treeHeight; ++h) {
                        chunk.voxels[x + trunkOffsetX][height + h][z + trunkOffsetZ].type = 5; // Wood
                        chunk.voxels[x + trunkOffsetX][height + h][z + trunkOffsetZ].visible = true;

                        // Add occasional branches
                        if (h > 1 && randomChance(gen) < 0.3) {
                            int branchLength = 1 + (gen() % 2); // Randomize branch length
                            int branchDirX = (gen() % 3) - 1;
                            int branchDirZ = (gen() % 3) - 1;

                            for (int l = 1; l <= branchLength; ++l) {
                                int branchX = x + trunkOffsetX + branchDirX * l;
                                int branchZ = z + trunkOffsetZ + branchDirZ * l;
                                if (branchX >= 0 && branchX < CHUNK_SIZE && branchZ >= 0 && branchZ < CHUNK_SIZE) {
                                    chunk.voxels[branchX][height + h][branchZ].type = 5; // Wood
                                    chunk.voxels[branchX][height + h][branchZ].visible = true;
                                }
                            }
                        }
                    }

                    // Add leaves in a spherical pattern around the top
                    int leafRadius = 3;
                    int leafBaseHeight = height + treeHeight - 1;

                    for (int lx = -leafRadius; lx <= leafRadius; ++lx) {
                        for (int lz = -leafRadius; lz <= leafRadius; ++lz) {
                            for (int ly = -1; ly <= 2; ++ly) {
                                int nx = x + trunkOffsetX + lx;
                                int ny = leafBaseHeight + ly;
                                int nz = z + trunkOffsetZ + lz;

                                if (nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE_Y && nz >= 0 && nz < CHUNK_SIZE) {
                                    float distance = std::sqrt(lx * lx + lz * lz + ly * ly);
                                    if (distance <= leafRadius && randomChance(gen) < 0.8) { // Make leaves sparser towards the edges
                                        if (chunk.voxels[nx][ny][nz].type == 0) { // Only place leaves in empty blocks
                                            chunk.voxels[nx][ny][nz].type = 6; // Leaves
                                            chunk.voxels[nx][ny][nz].visible = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
                if (y <= sandThreshold && chunk.voxels[x][y][z].type == 0){
                    chunk.voxels[x][y][z].type = 9; // Water
                    chunk.voxels[x][y][z].visible = true;
                    chunk.voxels[x][y][z].isSource = true;
                }
            }
        }
    }


    chunk.isGenerated = true;
}

bool TerrainManager::isExposed(const Chunk& chunk, int x, int y, int z) {
    // Check neighboring voxels for exposure
    constexpr int neighbors[6][3] = {
        {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}
    };

    for (const auto& offset : neighbors) {
        int nx = x + offset[0];
        int ny = y + offset[1];
        int nz = z + offset[2];

        // Ensure neighbor is within chunk bounds
        if (nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE_Y && nz >= 0 && nz < CHUNK_SIZE) {
            if (chunk.voxels[nx][ny][nz].type == 0 || chunk.voxels[x][y][z].type != 9) {
                return true; // Block is exposed if neighbor is air
            }
        } else {
            return true; // Block is at the edge of the chunk
        }
    }

    return false; // Not exposed
}

int TerrainManager::getHighestBlock(const Voxel voxels[CHUNK_SIZE][CHUNK_SIZE_Y][CHUNK_SIZE], int x, int z) {
    for (int y = CHUNK_SIZE_Y - 1; y >= 0; --y) {
        if (voxels[x][y][z].type != 0 && voxels[x][y][z].type != 9 && voxels[x][y][z].type != 10) { // Check for a non-empty block
            return y;
        }
    }
    return 20; // Default to the highest point if no blocks are present
}

int TerrainManager::getClosestGroundWithClearance(const Voxel voxels[CHUNK_SIZE][CHUNK_SIZE_Y][CHUNK_SIZE], int localX, int localZ, int playerY) {
    // Start from the player's Y position and search downward
    for (int y = playerY; y >= 0; --y) {
        // Check if this is a solid block
        if (voxels[localX][y][localZ].type != 0 && voxels[localX][y][localZ].type != 9 && voxels[localX][y][localZ].type != 10) {
            // Ensure there are at least two air blocks above it for clearance
            if (y + 1 < CHUNK_SIZE_Y && y + 2 < CHUNK_SIZE_Y &&
                voxels[localX][y + 1][localZ].type == 0 &&
                voxels[localX][y + 2][localZ].type == 0) {
                return y; // Return this Y level as the ground level
            }
        }
    }

    // If no suitable ground level is found, return -1
    return -1;
}