#pragma once
#include "chunk.hpp"


class TerrainManager {
public:
    static void generateTerrain(Chunk& chunk, int chunkX, int chunkZ, PerlinNoise& perlin);
    static void calculateVisibility(Chunk& chunk);
    static bool isExposed(const Chunk& chunk, int x, int y, int z);
    static int getHighestBlock(const Voxel voxels[CHUNK_SIZE][CHUNK_SIZE_Y][CHUNK_SIZE], int x, int z);
    static bool isMineralNearby(const Chunk& chunk, int x, int y, int z, int radius, int currentMineralType);
    static int getClosestGroundWithClearance(const Voxel voxels[CHUNK_SIZE][CHUNK_SIZE_Y][CHUNK_SIZE], int localX, int localZ, int playerY);
};
