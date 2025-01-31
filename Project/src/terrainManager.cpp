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
        return; // Skip already generated chunks
    }

    const int waterLevel = 14;  // Sea level
    const int maxTerrainHeight = CHUNK_SIZE_Y - 1;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> randomChance(0.0, 1.0);

    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            int worldX = chunkX * CHUNK_SIZE + x;
            int worldZ = chunkZ * CHUNK_SIZE + z;

            // Base terrain height using Perlin noise
            double baseHeight = perlin.noise(worldX * 0.03, 0.0, worldZ * 0.03) * 15.0 + 20.0;
            baseHeight += perlin.noise(worldX * 0.1, 0.0, worldZ * 0.1) * 5.0;  // Add smaller details
            int height = std::clamp(static_cast<int>(baseHeight), 1, maxTerrainHeight);

            // Determine biome using low-frequency noise
            double biomeNoise = perlin.noise(worldX * 0.005, 0.0, worldZ * 0.005);
            enum Biome { Desert, Grassland, Mountain };
            Biome biome = (biomeNoise < 0.2) ? Grassland
                        : (biomeNoise < 0.3) ? Desert
                        : Mountain;

            // Adjust height and block types based on biome
            double biomeBlendFactor = glm::clamp((biomeNoise - 0.3) / 0.2, 0.0, 1.0);
            if (biome == Mountain) {
                height += biomeBlendFactor * (perlin.noise(worldX * 0.02, 0.0, worldZ * 0.02) * 50.0 + 15.0);
            } else if (biome == Desert) {
                height += perlin.noise(worldX * 0.1, 0.0, worldZ * 0.1) * 4.0;  // Rolling dunes
            } else if (biome == Grassland) {
                height += perlin.noise(worldX * 0.05, 0.0, worldZ * 0.05) * 6.0;  // Rolling hills
            }

            height = std::clamp(height, 1, maxTerrainHeight);

            // Generate terrain column
            for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
                if (y == 0) {
                    chunk.voxels[x][y][z].type = 3;  // Bedrock
                    chunk.voxels[x][y][z].sourceID = -1;
                } else if (y < height - 3) {
                    chunk.voxels[x][y][z].type = 2;  // Stone
                    chunk.voxels[x][y][z].sourceID = -1;
                } else if (y < height) {
                    double blendNoise = perlin.noise(worldX * 0.1, y * 0.1, worldZ * 0.1);
                    if (biome == Desert) {
                        chunk.voxels[x][y][z].type = 8;  // Sand
                        chunk.voxels[x][y][z].sourceID = -1;
                    } else if (biome == Grassland) {
                        chunk.voxels[x][y][z].type = 1;  // Grass
                        chunk.voxels[x][y][z].sourceID = -1;
                    } else if (biome == Mountain) {
                        if (y < height - 5) {
                            chunk.voxels[x][y][z].type = (blendNoise > 0.5) ? 2 : 7;  // stone and Cobblestone
                            chunk.voxels[x][y][z].sourceID = -1;
                        } else if (y < height - 1) {
                            chunk.voxels[x][y][z].type = (blendNoise > 0.3) ? 1 : 2;  // Grass and ston
                            chunk.voxels[x][y][z].sourceID = -1;
                        } else if (y == height - 1 && y > waterLevel + 15) {
                            chunk.voxels[x][y][z].type = 10;  // Snow
                            chunk.voxels[x][y][z].sourceID = -1;
                        }
                    }
                } else if (y <= waterLevel) {
                    chunk.voxels[x][y][z].type = 9;  // Water
                    chunk.voxels[x][y][z].isSource = true;
                    chunk.voxels[x][y][z].sourceID = -1;
                } else {
                    chunk.voxels[x][y][z].type = 0;  // Air
                    chunk.voxels[x][y][z].sourceID = -1;
                }
                chunk.voxels[x][y][z].visible = false;
            }

            // Add caves
            for (int y = 1; y < height - 5; ++y) {
                double caveNoise = perlin.noise(worldX * 0.1, y * 0.1, worldZ * 0.1) +
                                   perlin.noise(worldX * 0.05, y * 0.05, worldZ * 0.05) * 0.5;
                if (caveNoise > 0.6) {
                    chunk.voxels[x][y][z].type = 0;  // Carve out caves
                    chunk.voxels[x][y][z].visible = false;
                    chunk.voxels[x][y][z].sourceID = -1;
                }
            }

            // Add diamond/coal/iron ores underground
            for (int y = 2; y < height - 5; ++y) {
                double oreNoise = perlin.noise(worldX * 0.15, y * 0.15, worldZ * 0.15);

                if (oreNoise > 0.72 && chunk.voxels[x][y][z].type == 2 && y < height - 20) {  // Rare chance at low depth
                    chunk.voxels[x][y][z].type = 4;  // Diamond
                    chunk.voxels[x][y][z].sourceID = -1;
                    chunk.voxels[x][y][z].visible = true;
                } else  if (oreNoise > 0.60 && oreNoise <= 0.72 && chunk.voxels[x][y][z].type == 2 && y < height - 10 && !isMineralNearby(chunk, x, y, z, 2, 14)) {  // Rare chance
                    chunk.voxels[x][y][z].type = 14;  // gold
                    chunk.voxels[x][y][z].sourceID = -1;
                    chunk.voxels[x][y][z].visible = true;
                    continue;
                } else  if (oreNoise > 0.53 && oreNoise <= 0.60 && chunk.voxels[x][y][z].type == 2 && !isMineralNearby(chunk, x, y, z, 2, 14)) {  // Rare chance
                    chunk.voxels[x][y][z].type = 12;  // iron
                    chunk.voxels[x][y][z].sourceID = -1;
                    chunk.voxels[x][y][z].visible = true;
                    continue;
                } else  if (oreNoise > 0.45 && oreNoise <= 0.53 && chunk.voxels[x][y][z].type == 2 && !isMineralNearby(chunk, x, y, z, 1, 12)) {  // Rare chance
                    chunk.voxels[x][y][z].type = 13;  // coal
                    chunk.voxels[x][y][z].sourceID = -1;
                    chunk.voxels[x][y][z].visible = true;
                }
            }

            // Add trees in grassland
            if (biome == Grassland && randomChance(gen) < 0.03 && height > waterLevel + 2) {
                int treeHeight = 5 + (gen() % 3);  // Tree height between 5 and 7
                for (int h = 0; h < treeHeight; ++h) {
                    if (height + h >= CHUNK_SIZE_Y) break;
                    chunk.voxels[x][height + h][z].type = 5;  // Wood
                    chunk.voxels[x][height + h][z].visible = true;
                    chunk.voxels[x][height + h][z].sourceID = -1;
                }

                // Add leaves in a spherical pattern
                int leafRadius = 2;
                for (int lx = -leafRadius; lx <= leafRadius; ++lx) {
                    for (int lz = -leafRadius; lz <= leafRadius; ++lz) {
                        for (int ly = -1; ly <= 1; ++ly) {
                            int nx = x + lx;
                            int ny = height + treeHeight + ly - 1;
                            int nz = z + lz;

                            if (nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE_Y && nz >= 0 && nz < CHUNK_SIZE) {
                                float dist = std::sqrt(lx * lx + lz * lz + ly * ly);
                                if (dist <= leafRadius && randomChance(gen) < 0.8) {
                                    chunk.voxels[nx][ny][nz].type = 6;  // Leaves
                                    chunk.voxels[nx][ny][nz].visible = true;
                                    chunk.voxels[nx][ny][nz].sourceID = -1;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    // Determine visibility for all voxels in the chunk
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                if (chunk.voxels[x][y][z].type != 0) { // Only check non-air blocks
                    if(chunk.voxels[x][y][z].type == 4 || chunk.voxels[x][y][z].type == 14){
                        chunk.voxels[x][y][z].visible = true;
                    } else {
                    chunk.voxels[x][y][z].visible = isExposed(chunk, x, y, z);
                    }
                }
            }
        }
    }
    chunk.isGenerated = true;
}

bool TerrainManager::isMineralNearby(const Chunk& chunk, int x, int y, int z, int radius, int currentMineralType) {
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dz = -radius; dz <= radius; ++dz) {
                int nx = x + dx;
                int ny = y + dy;
                int nz = z + dz;

                // Ensure neighbor is within chunk bounds
                if (nx >= 0 && nx < CHUNK_SIZE &&
                    ny >= 0 && ny < CHUNK_SIZE_Y &&
                    nz >= 0 && nz < CHUNK_SIZE) {
                    // Check if there's a different mineral
                    int neighborType = chunk.voxels[nx][ny][nz].type;
                    if (neighborType != 0 && neighborType != 2 && neighborType != currentMineralType) {
                        return true; // Found a different mineral nearby
                    }
                }
            }
        }
    }
    return false; // No conflicting minerals found
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
            if (chunk.voxels[nx][ny][nz].type == 0 || chunk.voxels[x][y][z].type == 9) {
                return true; // Block is exposed if neighbor is air
            }
        }
    }

    return false; // Not exposed
}

int TerrainManager::getHighestBlock(const Voxel voxels[CHUNK_SIZE][CHUNK_SIZE_Y][CHUNK_SIZE], int x, int z) {
    for (int y = CHUNK_SIZE_Y - 1; y >= 0; --y) {
        if (voxels[x][y][z].type != 0 && voxels[x][y][z].type != 9) { // Check for a non-empty block
            return y;
        }
    }
    return 20; // Default to the highest point if no blocks are present
}

int TerrainManager::getClosestGroundWithClearance(const Voxel voxels[CHUNK_SIZE][CHUNK_SIZE_Y][CHUNK_SIZE], int localX, int localZ, int playerY) {
    // Start from the player's Y position and search downward
    for (int y = playerY; y >= 0; --y) {
        // Check if this is a solid block
        if (voxels[localX][y][localZ].type != 0 && voxels[localX][y][localZ].type != 9) {
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