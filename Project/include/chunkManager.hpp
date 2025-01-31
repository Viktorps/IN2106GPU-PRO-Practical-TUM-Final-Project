#pragma once
#include <unordered_map>
#include <utility>
#include "terrainManager.hpp"
#include <glm/gtx/string_cast.hpp>
#include <functional>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

struct Vec3Hasher {
    std::size_t operator()(const glm::ivec3& v) const noexcept {
        std::size_t h1 = std::hash<int>()(v.x);
        std::size_t h2 = std::hash<int>()(v.y);
        std::size_t h3 = std::hash<int>()(v.z);

        // Combine the hashes
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

struct MinedBlock{
    glm::vec3 pos;
    glm::vec3 orientation;
    glm::vec3 velocity;
    int type;
    int lifetime;
    bool tnt;
};

struct triggeredTNT{
    glm::vec3 pos;
    int currentWave;
    int radius;
    int delay;
};

class ChunkManager {
public:

    bool updateWaterForXZ(Chunk& chunk, int x, int z);
    glm::vec3 worldPosition(const Chunk& chunk, int localX, int localY, int localZ) const {
        glm::vec3 chunkBasePosition = chunk.position; // Position of the chunk in world space
        glm::vec3 localPosition = glm::vec3(localX, localY, localZ); // Local position within the chunk
        return chunkBasePosition + localPosition; // Combine to get world position
    }

    void updateTNT();
    void handleWaterBlock(const glm::vec3& position, Voxel& block);
    int getLowestFreeID() {
        int id;
        if (!freeIDs.empty()) {
            id = freeIDs.top(); // Get the lowest free ID
            freeIDs.pop(); // Remove it from the pool
        } else {
            id = nextID++; // Use the next sequential ID
        }
        return id;
    }

    void releaseID(int id) {
        freeIDs.push(id); // Add the ID back to the free pool
        activeFlows.erase(id);
        std::cout << "ID: " << id << " Was released!\n";
    }

    void addToRemoveQueue(int sourceID, const glm::vec3& pos) {
        // Add the source ID to the remove queue if it's not already present
        if (std::find(removeWaterQueue.begin(), removeWaterQueue.end(), sourceID) == removeWaterQueue.end()) {
            removeWaterQueue.push_back(sourceID);
        }

        // Ensure that waterBlocksByID is properly initialized for this sourceID
        auto it = waterBlocksByID.find(sourceID);
        if (it == waterBlocksByID.end()) {
            waterBlocksByID[sourceID] = {}; // Initialize an empty vector for this ID
        }

        // Add the position to the list of water blocks for this sourceID, avoiding duplicates
        if (std::find(waterBlocksByID[sourceID].begin(), waterBlocksByID[sourceID].end(), pos) == waterBlocksByID[sourceID].end()) {
            waterBlocksByID[sourceID].push_back(pos);
        }
    }

    void onWaterSourceAdded(const glm::vec3& position, int sourceID) {
        generateWaterQueue[position] = {sourceID, 0};
        activeFlows[sourceID] = 1;
    }

    void simulateWater(std::unordered_map<glm::vec3, std::pair<int, int>>& queue);
    void removeWater();

    void simulateSand(Chunk& chunk, int x, int y, int z);

    void incrementTick() {
        currentTick++;
    }
    void generateWorld(int width, int depth, PerlinNoise& perlin);
    void addChunk(ChunkKey key, Chunk chunk);
    void clear();

    void updateCombinedChunk(const glm::vec3& playerPosition, int radius);
    void updateVoxelVisibility(Voxel& voxel, Chunk& chunk, const glm::ivec3& localPos);
    void updateVoxelsAroundPlayer(const glm::vec3& playerPosition, int radius);
    void updateWaterVoxels();

    bool isPositionUnderwater(const glm::vec3& position) const;

    std::optional<glm::vec3> getNormal(
    const glm::vec3& playerPosition,
    const glm::vec3& viewDirection,
    const glm::vec3& targetBlockPos);

    bool hasExposedFace(Chunk& chunk, const glm::ivec3& localPos);
    void updateMinedBlocks(float deltaTime, const glm::vec3& playerPosition, std::vector<int>& minedBlocks);
    bool loadChunk(Chunk& chunk, int chunkX, int chunkZ);
    void saveChunk(const Chunk& chunk, int chunkX, int chunkZ);
    void updateChunks(const glm::mat4& viewProjectionMatrix, const glm::vec3& playerPosition, int viewDistance);
    std::optional<glm::vec3> getPlacementPosition(const glm::vec3& targetBlockPos, const glm::vec3& playerPosition, const glm::vec3& viewDirection);
    std::vector<Chunk*> getVisibleChunks(const glm::vec3& playerPosition, int viewDistance, const glm::mat4& viewProjectionMatrix);
    // Replace a block at the given world position
    bool placeBlock(const glm::vec3& placementPosition, int newBlockType, const glm::vec3& playerPosition, const glm::vec3& playerSize);

    std::optional<glm::vec3> getTargetBlock(const glm::vec3& playerPosition, const glm::vec3& viewDirection);

    bool isVoxelVisibleToPlayer(const Chunk& chunk, int x, int y, int z) const ;
    void calculateVisibility(Chunk& chunk, const glm::vec3& playerPosition, int viewDistance);
  
    void setUpdated(){
        updated = true;
    }

    bool shouldUpdate(){
        if(updated){
            updated = false;
            return true;
        } else
        return false;
    }

    bool mineBlock(const glm::vec3& position);

    const Chunk* getChunkAt(const glm::vec3& position) const; // Const version
    Chunk* getChunkAt(const glm::vec3& position);             // Non-const version

    const Voxel* getBlockAt(const glm::vec3& position) const; // Const version
    Voxel* getBlockAt(const glm::vec3& position);             // Non-const version

    const std::unordered_map<ChunkKey, Chunk, ChunkKeyHasher> getAllChunks() const;
    std::vector<MinedBlock> getMinedBlocks() { return activeMinedBlocks; }

    std::unordered_map<glm::ivec3, Voxel, Vec3Hasher> combinedChunk;
    std::priority_queue<int, std::vector<int>, std::greater<int>> freeIDs; // Min-heap to track free IDs
    int nextID = 0; // Tracks the next available ID if freeIDs is empty
    int tempIDStart = INT_MAX;
    std::unordered_map<int, int> activeFlows; // Map sourceID to the count of active flows
    std::unordered_map<glm::vec3, std::pair<int, int>> generateWaterQueue; 
    std::vector<int> removeWaterQueue;
    std::unordered_map<int, std::vector<glm::vec3>> waterBlocksByID;
    std::vector<std::pair<int, int>> checkWater;
    std::unordered_map<int, triggeredTNT> activeTNT;
    std::unordered_map<ChunkKey, Chunk, ChunkKeyHasher> chunks;
    std::unordered_map<ChunkKey, Chunk, ChunkKeyHasher> savedChunks;
    std::unordered_set<ChunkKey, ChunkKeyHasher> dirtyChunks; 
    int currentTick = 0;

private:

    bool updated;
    std::vector<MinedBlock> activeMinedBlocks;

};
