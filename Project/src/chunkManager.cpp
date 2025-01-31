#include "chunkManager.hpp"

void ChunkManager::generateWorld(int width, int depth, PerlinNoise& perlin) {
    for (int chunkX = -width / 2; chunkX <= width / 2; ++chunkX) {
        for (int chunkZ = -depth / 2; chunkZ <= depth / 2; ++chunkZ) {
            Chunk chunk;
            chunk.key = {chunkX, chunkZ};
            chunk.position = glm::vec3(chunkX * CHUNK_SIZE, 0, chunkZ * CHUNK_SIZE);
            chunk.isGenerated = false;
            chunk.isDirty = false;

            // Generate terrain for the chunk
            TerrainManager::generateTerrain(chunk, chunkX, chunkZ, perlin);

            // Store the chunk
            chunks[{chunkX, chunkZ}] = std::move(chunk);
        }
    }
}

void ChunkManager::addChunk(ChunkKey key, Chunk chunk) {
    chunks[key] = std::move(chunk);
}

void ChunkManager::updateTNT(){
    for(auto& it: activeTNT){
        if(it.second.delay <= 0){
        for(int dx = -it.second.currentWave; dx <= it.second.currentWave; dx++){
            for(int dy = -it.second.currentWave; dy <= it.second.currentWave; dy++){
                for(int dz = -it.second.currentWave; dz <= it.second.currentWave; dz++){
                    glm::ivec3 targetPos = glm::ivec3(it.second.pos.x + dx, it.second.pos.y + dy, it.second.pos.z + dz);

                    // Check if the point is inside the sphere of currentWave radius
                    if (glm::length(glm::vec3(dx, dy, dz)) <= it.second.currentWave) {
                        // Handle the block at the position (e.g., destroy it or set it to air)
                        Chunk* chunk = getChunkAt(targetPos);
                            if (chunk) {
                                // Convert the target position to the chunk's local coordinates
                                glm::ivec3 localPos = glm::ivec3(
                                    targetPos.x % CHUNK_SIZE,
                                    targetPos.y % CHUNK_SIZE,
                                    targetPos.z % CHUNK_SIZE
                                );

                                // Handle negative wrapping for local coordinates
                                if (localPos.x < 0) localPos.x += CHUNK_SIZE;
                                if (localPos.y < 0) localPos.y += CHUNK_SIZE;
                                if (localPos.z < 0) localPos.z += CHUNK_SIZE;

                                // Check if the block exists and is not air
                                Voxel& block = chunk->voxels[localPos.x][localPos.y][localPos.z];
                                if (block.type != 0) { // Skip air blocks
                                    mineBlock(targetPos); // Set to air
                                }
                            }
                        it.second.delay = 2;
                    }
                }
            }
        }

        if(it.second.currentWave == it.second.radius){
            activeTNT.erase(it.first);
        }
        it.second.currentWave++;
        } else {
            it.second.delay--;
        }
    }
}

void ChunkManager::clear() {
    chunks.clear();
    savedChunks.clear();
    combinedChunk.clear();
    dirtyChunks.clear();
}

auto mod = [](int value, int mod) -> int {
    return (value % mod + mod) % mod;
};

void ChunkManager::updateVoxelsAroundPlayer(const glm::vec3& playerPosition, int radius) {
    glm::ivec3 playerBlockPos = glm::floor(playerPosition);
    
    // Calculate the bounding box for the update radius
    int startX = playerBlockPos.x - radius;
    int endX = playerBlockPos.x + radius;
    int startY = std::max(playerBlockPos.y - radius, 0); // Ensure y is within valid range
    int endY = std::min(playerBlockPos.y + radius, CHUNK_SIZE_Y - 1);
    int startZ = playerBlockPos.z - radius;
    int endZ = playerBlockPos.z + radius;

    // Iterate through the voxels within the radius
    for (int x = startX; x <= endX; ++x) {
        for (int y = startY; y <= endY; ++y) {
            for (int z = startZ; z <= endZ; ++z) {
                glm::ivec3 worldPos = {x, y, z};
                
                // Get the chunk and local coordinates for the voxel
                Chunk* chunk = getChunkAt(worldPos);
                //if (!chunk) continue;

                glm::ivec3 localPos = {
                    mod(x, CHUNK_SIZE),
                    y,
                    mod(z, CHUNK_SIZE)
                };

                Voxel& voxel = chunk->voxels[localPos.x][localPos.y][localPos.z];
                if(voxel.type != 9){
                // Perform visibility or type updates as needed
                updateVoxelVisibility(voxel, *chunk, localPos);
                }
            }
        }
    }
}

void ChunkManager::updateWaterVoxels() {
    // Iterate through all loaded chunks
    for (auto& [chunkPos, chunk] : chunks) { // Assuming chunks is a map with chunk position as the key
        // Iterate through all voxels in the chunk
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    // Access the voxel
                    Voxel& voxel = chunk.voxels[x][y][z];

                    // Check if the voxel is a water voxel (e.g., type == 9)
                    if (voxel.type == 9) {
                        // Perform updates (e.g., visibility updates)
                        updateVoxelVisibility(voxel, chunk, glm::ivec3{x, y, z});
                    }
                }
            }
        }
    }
}

void ChunkManager::updateCombinedChunk(const glm::vec3& playerPosition, int radius) {
    glm::ivec3 playerBlockPos = glm::floor(playerPosition);
    int startX = playerBlockPos.x - radius;
    int endX = playerBlockPos.x + radius;
    int startY = std::max(playerBlockPos.y - radius, 0);
    int endY = std::min(playerBlockPos.y + radius, CHUNK_SIZE_Y - 1);
    int startZ = playerBlockPos.z - radius;
    int endZ = playerBlockPos.z + radius;

    for (int x = startX; x <= endX; ++x) {
        for (int y = startY; y <= endY; ++y) {
            for (int z = startZ; z <= endZ; ++z) {
                glm::ivec3 worldPos = {x, y, z};
                Chunk* chunk = getChunkAt(worldPos);
                if (!chunk) continue;

                glm::ivec3 localPos = {
                    mod(x, CHUNK_SIZE),
                    y,
                    mod(z, CHUNK_SIZE)
                };
                const Voxel& voxel = chunk->voxels[localPos.x][localPos.y][localPos.z];
                if (voxel.visible) {
                    combinedChunk[worldPos] = voxel;
                } else {
                    combinedChunk.erase(worldPos); // Remove non-visible voxels
                }
            }
        }
    }
}

bool ChunkManager::hasExposedFace(Chunk& chunk, const glm::ivec3& localPos) {
    static const glm::ivec3 directions[] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };


    for (const auto& dir : directions) {
        glm::ivec3 neighborPos = localPos + dir;

        // Handle chunk edges
        if (neighborPos.x < 0 || neighborPos.y < 0 || neighborPos.z < 0 ||
            neighborPos.x >= CHUNK_SIZE || neighborPos.y >= CHUNK_SIZE_Y || neighborPos.z >= CHUNK_SIZE) {
                if(neighborPos.y < 0){
                    continue;
                }
            glm::vec3 worldNeighborPos = worldPosition(chunk, neighborPos.x, neighborPos.y, neighborPos.z);
            Chunk* neighborChunk = getChunkAt(worldNeighborPos);
            if(!neighborChunk){
                continue;
            }
            // Convert to local coordinates in the neighboring chunk
            glm::ivec3 wrappedPos = {
                mod(neighborPos.x, CHUNK_SIZE),
                neighborPos.y,
                mod(neighborPos.z, CHUNK_SIZE)
            };
            const Voxel& neighborVoxel = neighborChunk->voxels[wrappedPos.x][wrappedPos.y][wrappedPos.z];
            if (neighborVoxel.type == 0 || neighborVoxel.type == 9) {
                return true; // Exposed to air or fluid
            }
        } else {
            // Neighbor within the same chunk
            const Voxel& neighborVoxel = chunk.voxels[neighborPos.x][neighborPos.y][neighborPos.z];
            if (neighborVoxel.type == 0 || neighborVoxel.type == 9) {
                return true; // Exposed to air or fluid
            }
        }
    }

    return false; // Fully enclosed
}

void ChunkManager::updateVoxelVisibility(Voxel& voxel, Chunk& chunk, const glm::ivec3& localPos) {
    // If the block is air, it's not visible
    if (voxel.type == 0) {
        voxel.visible = false;
        return;
    }

    if (voxel.type == 4 || voxel.type == 14) {
        voxel.visible = true;
        return;
    }

    // For non-water blocks, visibility depends on exposed faces
    if (voxel.type != 9) {
        voxel.visible = hasExposedFace(chunk, localPos);
        return;
    }

    // For water blocks, calculate visibility face by face
    static const glm::ivec3 directions[] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };

    for (int i = 0; i < 6; ++i) {
        glm::ivec3 neighborPos = localPos + directions[i];
        bool isFaceVisible = false;

        // Handle chunk edges
        if (neighborPos.x < 0 || neighborPos.y < 0 || neighborPos.z < 0 ||
            neighborPos.x >= CHUNK_SIZE || neighborPos.y >= CHUNK_SIZE_Y || neighborPos.z >= CHUNK_SIZE) {
            if(neighborPos.y < 0){
                continue;
            }
            Chunk* neighborChunk = getChunkAt(worldPosition(chunk, neighborPos.x, neighborPos.y, neighborPos.z));
            if(!neighborChunk){
                continue;
            }
            glm::ivec3 wrappedPos = {
                mod(neighborPos.x, CHUNK_SIZE),
                neighborPos.y,
                mod(neighborPos.z, CHUNK_SIZE)
            };
            const Voxel& neighborVoxel = neighborChunk->voxels[wrappedPos.x][wrappedPos.y][wrappedPos.z];
            isFaceVisible = (neighborVoxel.type == 0); // Exposed to air
        } else {
            // Neighbor within the same chunk
            const Voxel& neighborVoxel = chunk.voxels[neighborPos.x][neighborPos.y][neighborPos.z];
            isFaceVisible = (neighborVoxel.type == 0); // Exposed to air
        }
        voxel.faceVisible[i] = isFaceVisible;
    }
    voxel.visible = true; // Water always active for world actions
}

void ChunkManager::updateChunks(const glm::mat4& viewProjectionMatrix, const glm::vec3& playerPosition, int viewDistance) {
    int playerChunkX = static_cast<int>(std::floor(playerPosition.x / CHUNK_SIZE));
    int playerChunkZ = static_cast<int>(std::floor(playerPosition.z / CHUNK_SIZE));

    if (updated) {
        updated = false;
    }
    incrementTick();

    std::unordered_set<ChunkKey, ChunkKeyHasher> requiredChunks;

    // Determine chunks that should remain loaded
    for (int dx = -viewDistance; dx <= viewDistance; ++dx) {
        for (int dz = -viewDistance; dz <= viewDistance; ++dz) {
            requiredChunks.insert({playerChunkX + dx, playerChunkZ + dz});
        }
    }

    // Unload chunks not in view and save them
    for (auto it = chunks.begin(); it != chunks.end();) {
        if (requiredChunks.find(it->first) == requiredChunks.end()) {
            savedChunks[it->first] = std::move(it->second); // Save chunk before unloading
            it = chunks.erase(it); // Unload chunk
            updated = true;
        } else {
            ++it;
        }
    }

    // Load or enqueue required chunks
    for (const auto& key : requiredChunks) {
        if (chunks.find(key) == chunks.end()) {
            // Load from savedChunks if available
            if (savedChunks.find(key) != savedChunks.end()) {
                chunks[key] = std::move(savedChunks[key]);
                chunks[key].isDirty = true;
                savedChunks.erase(key);
            } 
        }
    }
    
    // Simulate water in the visible range
    for (const auto& key : requiredChunks) {
    auto& chunk = chunks[key];
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    if (chunk.voxels[x][y][z].type == 8 && y > 1 && (chunk.voxels[x][y-1][z].type == 0 || chunk.voxels[x][y-1][z].type == 9)) {
                        simulateSand(chunk, x, y, z);
                        }
                    }
                }
            }
        }
    if(currentTick >= 90){
        if (!generateWaterQueue.empty()) {
            simulateWater(generateWaterQueue);
            currentTick = 0;
        }
        if (!removeWaterQueue.empty()) {
            removeWater();
            currentTick = 0;
        }
    }
    updateTNT();
    updateVoxelsAroundPlayer(playerPosition, 8); // Update voxels in a 6-block radius
    updateWaterVoxels();
    updateCombinedChunk(playerPosition, 3);

    // Combine chunks if the current player's chunk is dirty
    for(const auto& key : requiredChunks){
    if (chunks[key].isDirty) {
        chunks[key].isDirty = false;
        updated = true;
        }
    }
}

bool ChunkManager::isPositionUnderwater(const glm::vec3& position) const {
    if(getBlockAt(position)){
    return (getBlockAt(position)->type == 9); // Assuming 9 is the water block type
    } else return false;
}

void ChunkManager::simulateWater(std::unordered_map<glm::vec3, std::pair<int, int>>& queue) {
    std::unordered_map<glm::vec3, std::pair<int, int>> newQueue; // Store new positions for the next simulation step

    for (auto& [pos, data] : queue) {
        int sourceID = data.first;
        int travelDistance = data.second;

        Chunk* chunk = getChunkAt(pos);
        if (!chunk) {
            continue; // Skip if chunk is not loaded
        }

        int localX = mod(static_cast<int>(pos.x), CHUNK_SIZE);
        int localY = static_cast<int>(pos.y);
        int localZ = mod(static_cast<int>(pos.z), CHUNK_SIZE);

        Voxel& voxel = chunk->voxels[localX][localY][localZ];

        // Check downward flow
        glm::vec3 belowPos = pos + glm::vec3(0, -1, 0);
        Voxel* below = getBlockAt(belowPos);
        // Stop spreading if it reaches other water
        if (below && below->type == 9 && travelDistance > 0) {
            continue; // Terminate this water flow
        }
        if (below && below->type == 0) { // Check if below is air
            below->type = 9; // Flowing water
            below->sourceID = sourceID;
            below->updated = true;

            // Update waterBlocksByID
            waterBlocksByID[sourceID].push_back(belowPos);

            chunk->isDirty = true;
            newQueue[belowPos] = {sourceID, 0}; // Reset travel distance for downward flow
            continue; // Skip further checks for this element
        }

        // Check sideways flow (only if downward is not possible)
        if (travelDistance < 3) { // Limit to 3 blocks horizontally
            static const glm::ivec3 directions[] = {
                {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};

            for (const auto& dir : directions) {
                glm::vec3 neighborPos = pos + glm::vec3(dir);
                Voxel* neighbor = getBlockAt(neighborPos);

                // Check if neighbor is air and either unassigned (-1) or matches the current source
                if (neighbor && neighbor->type == 0) { // Spread only to air blocks
                    neighbor->type = 9; // Flowing water
                    if (neighbor->sourceID == -1 || neighbor->sourceID == sourceID) { // Assign sourceID only if it matches or is unassigned
                        neighbor->sourceID = sourceID;

                        // Update waterBlocksByID
                        waterBlocksByID[sourceID].push_back(neighborPos);

                        chunk->isDirty = true;
                        newQueue[neighborPos] = {sourceID, travelDistance + 1};
                    }
                }
            }
        }
    }

    // Replace the current queue with the new queue for the next tick
    queue = std::move(newQueue);
}

void ChunkManager::removeWater() {
    if (removeWaterQueue.empty()) return; // No elements to process

    // Process the first sourceID in the queue
    int sourceID = removeWaterQueue.front();
    auto it = waterBlocksByID.find(sourceID);

    if (it == waterBlocksByID.end()) {
        // If no water blocks are found for this ID, remove it from the queue
        removeWaterQueue.erase(removeWaterQueue.begin());
        releaseID(sourceID); // Call releaseID to reclaim the source ID
        return;
    }

    auto& positions = it->second;
    bool removedAny = false;

    // Process blocks level-by-level
    for (int y = CHUNK_SIZE_Y - 1; y >= 0; --y) {
        std::vector<glm::vec3> remainingPositions;

        for (const auto& pos : positions) {
            if (static_cast<int>(pos.y) == y) {
                Chunk* chunk = getChunkAt(pos);
                if (chunk) {
                    int localX = mod(static_cast<int>(pos.x), CHUNK_SIZE);
                    int localY = static_cast<int>(pos.y);
                    int localZ = mod(static_cast<int>(pos.z), CHUNK_SIZE);

                    Voxel& voxel = chunk->voxels[localX][localY][localZ];
                    if (voxel.type == 9 && voxel.sourceID == sourceID) {
                        voxel.type = 0; // Remove the water block
                        voxel.sourceID = -1; // Reset sourceID
                        voxel.updated = true;
                        chunk->isDirty = true; // Mark chunk as dirty
                        removedAny = true;
                    }
                }
            } else {
                remainingPositions.push_back(pos);
            }
        }

        // Update the positions for this sourceID
        if (removedAny) {
            waterBlocksByID[sourceID] = std::move(remainingPositions);
            return; // Stop after processing one level
        }
    }

    // If no blocks remain, clean up the source ID
    waterBlocksByID.erase(sourceID);
    removeWaterQueue.erase(removeWaterQueue.begin());
    releaseID(sourceID); // Ensure releaseID is called
}

const Chunk* ChunkManager::getChunkAt(const glm::vec3& position) const {
    int chunkX = static_cast<int>(floor(position.x / CHUNK_SIZE));
    int chunkZ = static_cast<int>(floor(position.z / CHUNK_SIZE));

    ChunkKey key{chunkX, chunkZ};
    auto it = chunks.find(key);
    if (it != chunks.end()) {
        return &it->second; // Return const pointer
    }
    return nullptr;
}

Chunk* ChunkManager::getChunkAt(const glm::vec3& position) {
    int chunkX = static_cast<int>(floor(position.x / CHUNK_SIZE));
    int chunkZ = static_cast<int>(floor(position.z / CHUNK_SIZE));

    ChunkKey key{chunkX, chunkZ};
    auto it = chunks.find(key);
    if (it != chunks.end()) {
        return &it->second; // Return non-const pointer
    }
    return nullptr;
}

const Voxel* ChunkManager::getBlockAt(const glm::vec3& position) const {
    const Chunk* chunk = getChunkAt(position);
    if (!chunk) return nullptr;

    int localX = static_cast<int>(position.x) % CHUNK_SIZE;
    int localY = static_cast<int>(position.y);
    int localZ = static_cast<int>(position.z) % CHUNK_SIZE;

    if (localX < 0) localX += CHUNK_SIZE;
    if (localZ < 0) localZ += CHUNK_SIZE;

    if (localY >= 0 && localY < CHUNK_SIZE_Y) {
        return &chunk->voxels[localX][localY][localZ]; // Return const Voxel pointer
    }
    return nullptr;
}

Voxel* ChunkManager::getBlockAt(const glm::vec3& position) {
    // Determine the chunk the position belongs to
    int chunkX = static_cast<int>(std::floor(position.x / CHUNK_SIZE));
    int chunkZ = static_cast<int>(std::floor(position.z / CHUNK_SIZE)); 

    ChunkKey key{chunkX, chunkZ};
    auto it = chunks.find(key);
    if (it == chunks.end()) {
        return nullptr; // Chunk is not loaded
    }

    Chunk* chunk = &it->second;

    // Determine the block's local coordinates within the chunk
    int localX = static_cast<int>(std::floor(position.x)) % CHUNK_SIZE;
    int localY = static_cast<int>(position.y);
    int localZ = static_cast<int>(std::floor(position.z)) % CHUNK_SIZE;

    // Handle negative indices for wrapping
    if (localX < 0) localX += CHUNK_SIZE;
    if (localZ < 0) localZ += CHUNK_SIZE;

    // Ensure the local position is valid
    if (localY >= 0 && localY < CHUNK_SIZE_Y) {
        return &chunk->voxels[localX][localY][localZ];
    }

    return nullptr; // Invalid block position
}

std::optional<glm::vec3> ChunkManager::getPlacementPosition(
    const glm::vec3& playerPosition,
    const glm::vec3& viewDirection,
    const glm::vec3& targetBlockPos)
{
    glm::vec3 blockMin = targetBlockPos;               // Min corner of the block
    glm::vec3 blockMax = blockMin + glm::vec3(1); // Max corner of the block
    glm::vec3 rayOrigin = playerPosition + glm::vec3(0.0f, 1.7f, 0.0f); // Approximate player's eye height
    glm::vec3 rayDir = glm::normalize(viewDirection);

    float tMin = 0.0f, tMax = 1000.0f; // Range of valid t values for the ray

    glm::vec3 faceNormal(0.0f); // The normal of the face hit by the ray

    //std::cout << "Recieving target Block: " << glm::to_string(targetBlockPos) << "\n";
    // Check each axis (X, Y, Z)
    for (int i = 0; i < 3; ++i) {
        if (std::abs(rayDir[i]) < 1e-6) {
            // Ray is parallel to the planes perpendicular to this axis
            //std::cout << "Ray is parallel to axis " << i << "\n";
            if (rayOrigin[i] < blockMin[i] || rayOrigin[i] > blockMax[i]) {
                return std::nullopt; // Ray misses the block
            }
        } else {
            // Compute intersection distances (t) for the slabs
            float t1 = (blockMin[i] - rayOrigin[i]) / rayDir[i];
            float t2 = (blockMax[i] - rayOrigin[i]) / rayDir[i];
            if (t1 > t2) std::swap(t1, t2); // Ensure t1 is the entry, t2 is the exit
            if (t1 > tMin) {
                tMin = t1;
                faceNormal = glm::vec3(0.0f);
                faceNormal[i] = (rayDir[i] > 0) ? -1.0f : 1.0f; // Determine face normal
            }
            tMax = std::min(tMax, t2);
        }
    }

    std::cout << "Placement Position: " << glm::to_string(glm::floor(targetBlockPos) + faceNormal) << "\n";

    return glm::floor(targetBlockPos) + faceNormal;
}

std::optional<glm::vec3> ChunkManager::getNormal(
    const glm::vec3& playerPosition,
    const glm::vec3& viewDirection,
    const glm::vec3& targetBlockPos)
{
    glm::vec3 blockMin = targetBlockPos;               // Min corner of the block
    glm::vec3 blockMax = blockMin + glm::vec3(1); // Max corner of the block
    glm::vec3 rayOrigin = playerPosition + glm::vec3(0.0f, 1.7f, 0.0f); // Approximate player's eye height
    glm::vec3 rayDir = glm::normalize(viewDirection);

    float tMin = 0.0f, tMax = 1000.0f; // Range of valid t values for the ray

    glm::vec3 faceNormal(0.0f); // The normal of the face hit by the ray

    //std::cout << "Recieving target Block: " << glm::to_string(targetBlockPos) << "\n";
    // Check each axis (X, Y, Z)
    for (int i = 0; i < 3; ++i) {
        if (std::abs(rayDir[i]) < 1e-6) {
            // Ray is parallel to the planes perpendicular to this axis
            std::cout << "Ray is parallel to axis " << i << "\n";
            if (rayOrigin[i] < blockMin[i] || rayOrigin[i] > blockMax[i]) {
                return std::nullopt; // Ray misses the block
            }
        } else {
            // Compute intersection distances (t) for the slabs
            float t1 = (blockMin[i] - rayOrigin[i]) / rayDir[i];
            float t2 = (blockMax[i] - rayOrigin[i]) / rayDir[i];
            if (t1 > t2) std::swap(t1, t2); // Ensure t1 is the entry, t2 is the exit
            if (t1 > tMin) {
                tMin = t1;
                faceNormal = glm::vec3(0.0f);
                faceNormal[i] = (rayDir[i] > 0) ? -1.0f : 1.0f; // Determine face normal
            }
            tMax = std::min(tMax, t2);
        }
    }

    //std::cout << "Placement Position: " << glm::to_string(glm::floor(targetBlockPos) + faceNormal) << "\n";

    return faceNormal;
}

void ChunkManager::simulateSand(Chunk& chunk, int x, int y, int z) {
    Voxel& voxel = chunk.voxels[x][y][z];

    // Skip if not a sand block
    if (voxel.type != 8) {
        return;
    }

    // Tick interval for sand
    int tickInterval = 10;
    if (currentTick - voxel.tickCounter < tickInterval) {
        return;
    }
    voxel.tickCounter = currentTick;
    Voxel& below = chunk.voxels[x][y - 1][z];
    if (below.type != 0 && below.type != 9) {
        return;
    }
    // Check the block below
    if (y > 0) { // Ensure not at the bottom of the chunk
        //std::cout << "found either water or air under the sand block\n";

        if(below.type == 9 && below.isSource){
            std::cout << "Sand found water source with ID: " << below.sourceID << "\n";
            addToRemoveQueue(below.sourceID, glm::vec3(x, y-1,z));;
            below.sourceID = -1;
        } 

        voxel.updated = true;
        voxel.type = 0;
        voxel.visible = false;
        chunk.isDirty = true;
        voxel.sourceID = -1;
        voxel.isSource = false;
        if(below.type == 9 && below.isSource && below.sourceID == -1){
            bool found = false;
            glm::vec3 worldPos = worldPosition(chunk, x, y, z);
            for (int dx = -1; dx <= 1; ++dx) {
                        if(found) continue;
                        for (int dz = -1; dz <= 1; ++dz) {
                            if(found) continue;
                            if (abs(dx) + abs(dz) != 1) continue; // Only direct neighbors
                            glm::vec3 neighborPos = worldPos + glm::vec3(dx, 0, dz);
                            Voxel* neighbor = getBlockAt(neighborPos);
                            //std::cout << "Checking pos: " << glm::to_string(neighborPos) << ", Block is: " << neighbor->type << ", source is: " << neighbor->isSource << "\n";
                            if (neighbor && (neighbor->type == 9 && neighbor->isSource)) {
                                neighbor->sourceID = -1;
                                voxel.type = 9;
                                voxel.isSource = true;
                                voxel.sourceID = -1;
                                voxel.visible = true;
                                //std::cout << "Found neighboring water with source ID: " << neighbor->sourceID << "\n";
                                found = true;
                    }
                }
            }
        }

        // Make the current block air
        
        std::cout << "Changing value of sand\n";
    
        // Move the sand block down
        below.type = 8;
        below.updated = true;
        below.visible = true;
        below.tickCounter = currentTick; // Reset tick for the moved block
        chunk.isDirty = true;
        simulateSand(chunk, x, y-1, z);
    }
}

bool ChunkManager::placeBlock(const glm::vec3& placementPosition, int newBlockType, const glm::vec3& playerPosition, const glm::vec3& playerSize) {
    if (newBlockType == 0) {
        //std::cout << "Cannot place air block.\n";
        return false;
    }

    // Check if the placement position is within the player's bounding box
    glm::vec3 playerMin = playerPosition - playerSize * 0.5f;
    glm::vec3 playerMax = playerPosition + playerSize * 0.5f;
    if (placementPosition.x + 0.4f > playerMin.x && placementPosition.x < playerMax.x &&
        placementPosition.y + 0.3f > playerMin.y && placementPosition.y < playerMax.y &&
        placementPosition.z + 0.3f > playerMin.z && placementPosition.z < playerMax.z) {
        std::cout << "Cannot place block inside or near the player.\n";
        return false;
    }

    // Retrieve the block at the placement position
    Voxel* block = getBlockAt(placementPosition);
    if (block && (block->type == 0 || block->type == 9)) { // Ensure the block is empty (air or water)
        Chunk* chunk = getChunkAt(placementPosition);

        if(block->type == 9 && block->type != newBlockType && block->isSource){
            addToRemoveQueue(block->sourceID, placementPosition);
            std::cout << "id: " << block->sourceID << " added to remove queue\n";
            block->sourceID = -1;
        }

        block->type = newBlockType; // Place the new block
        block->updated = true;
        if (block->type == 9) { // Water block
            block->isSource = true; // Mark as a source block
            int id = getLowestFreeID();
            block->sourceID = id;
            onWaterSourceAdded(placementPosition, id);
            std::cout << "id: " << block->sourceID << " added to generate queue\n";
        }

        glm::ivec3 blockPos = glm::floor(placementPosition);
        auto it = combinedChunk.find(blockPos);
        if (it != combinedChunk.end()) {
            combinedChunk[blockPos].type = newBlockType;
        }

        
        if (chunk) {
            chunk->isGenerated = true; // Mark chunk as dirty
            chunk->isDirty = true;
        }

        if (newBlockType == 8) { // Sand block
            block->tickCounter = currentTick; // Delay the simulation for one tick interval
        }

        //std::cout << "Block placed successfully at: " << glm::to_string(placementPosition) << "\n";
        return true;
    } else {
        //std::cout << "Failed to place block: Block is not empty or position is invalid.\n";
    }

    return false;
}

void ChunkManager::updateMinedBlocks(float deltaTime, const glm::vec3& playerPosition, std::vector<int>& minedBlocks) {
    const float blockScale = 0.3f;             // Scale of the blocks
    const float blockRadius = blockScale * 0.5f; // Effective radius of a block
    const float repelThreshold = blockRadius * 2.0f; // Threshold distance for repulsion
    const float repelForceFactor = 3.0f;       // Strength of the repulsion force
    const float airResistance = 0.5f;         // Air resistance factor (higher = faster slowdown)
    const float playerAttractRadius = 2.5f;   // Radius around the player for attraction
    const float playerAttractStrength = 0.5f; // Strength of attraction to the player
    const float velocityDamping = 0.98f;      // Damping factor to reduce sliding

    if(activeMinedBlocks.size() <= 0){
        activeMinedBlocks.push_back({
            .pos = glm::vec3(0.0f, -2, 0.0f), // Centered position
            .orientation = glm::vec3(0.0f),                 // Initial orientation
            .velocity = glm::vec3(0,0,0), // Random x, z velocity
            .type = 1,                            // Type of the mined block
            .lifetime = 10000,
            .tnt = false                                // Lifetime in ticks
        });
    }


    auto it = activeMinedBlocks.begin();
    while (it != activeMinedBlocks.end()) {
        // Reduce lifetime
        it->lifetime -= 1;

        // Apply gravity
        it->velocity.y -= 9.8f * deltaTime;

        // Apply air resistance to x and z velocities
        it->velocity.x *= std::max(0.0f, 1.0f - airResistance * deltaTime);
        it->velocity.z *= std::max(0.0f, 1.0f - airResistance * deltaTime);

        // Predict next position
        glm::vec3 nextPos = it->pos + it->velocity * deltaTime;

        // Check if there's ground beneath
        glm::ivec3 groundBlockPos = glm::floor(nextPos - glm::vec3(0.0f, blockRadius, 0.0f));
        const Voxel* groundBlock = getBlockAt(groundBlockPos);

        if (groundBlock && groundBlock->type != 0) {
            // Stop falling if there's ground beneath
            if (it->velocity.y < 0.0f) {
                it->velocity.y = -it->velocity.y * 0.5f; // Reverse and reduce vertical velocity for bouncing
                if (std::abs(it->velocity.y) < 1.0f) {
                    it->velocity.y = 0.0f; // Stop bouncing if velocity is too small
                }
            }
            nextPos.y = std::floor(it->pos.y); // Snap to the top of the ground block
        }

        // Resolve collisions and repulsion with other blocks
        glm::vec3 repulsionForce(0.0f);
        for (const auto& otherBlock : activeMinedBlocks) {
            if (&otherBlock == &(*it)) continue; // Skip self
            glm::vec3 toOther = otherBlock.pos - it->pos;
            float dist = glm::length(glm::vec2(toOther.x, toOther.z)); // Only consider x/z distance
            if (dist < repelThreshold && dist > 0.001f) { // Avoid division by zero
                glm::vec2 repelDir = glm::normalize(glm::vec2(toOther.x, toOther.z));
                float overlap = repelThreshold - dist; // Calculate the overlap
                glm::vec2 repelForce2D = -repelDir * overlap * repelForceFactor; // Apply repulsion force
                repulsionForce.x += repelForce2D.x;
                repulsionForce.z += repelForce2D.y;
            }
        }

        // Apply repulsion force
        it->velocity += repulsionForce * deltaTime;

        // Prevent movement into existing blocks
        glm::ivec3 nextBlockPos = glm::floor(nextPos);
        const Voxel* nextBlock = getBlockAt(nextBlockPos);
        if (nextBlock && nextBlock->type != 0) {
            nextPos = it->pos; // Prevent position update
            it->velocity = glm::vec3(0.0f); // Stop the block's velocity
        }

        if (!it->tnt) { // Skip attraction for TNT blocks
            // Calculate the camera's target position
            glm::vec3 cameraTargetPosition = playerPosition + glm::vec3(-0.5f, 0.7f, -0.5f);

            // Attract blocks toward the camera's target position
            glm::vec3 toCamera = cameraTargetPosition - it->pos;
            float distToCamera = glm::length(toCamera);

            if (distToCamera < playerAttractRadius) {
                // Stronger attraction force with non-linear scaling
                float scaledStrength = playerAttractStrength * (1.0f + 25.0f * (playerAttractRadius - distToCamera) / playerAttractRadius);
                glm::vec3 attractForce = glm::normalize(toCamera) * scaledStrength;
                attractForce.y *= 6;
                it->velocity += attractForce * deltaTime;
            }
        }

        // Apply velocity damping to reduce residual sliding
        it->velocity *= velocityDamping;

        // Update position
        it->pos += it->velocity * deltaTime;

        // Spin the block unless it's TNT
        if (!it->tnt) {
            it->orientation += glm::vec3(0.0f, 0.5f, 0.0f) * deltaTime;
        }

        // TNT blocks are not picked up by the player
        if (!it->tnt && glm::distance(it->pos, playerPosition + glm::vec3(-0.5f, 0.7f, -0.5f)) < 0.8f) {
            // Remove the mined block and add to inventory
            minedBlocks[it->type]++;
            it = activeMinedBlocks.erase(it);
        } else if (it->lifetime <= 0) {
            // Remove the block if its lifetime has expired
            it = activeMinedBlocks.erase(it);
        } else {
            ++it;
        }
    }
}

bool ChunkManager::mineBlock(const glm::vec3& position) {
    Voxel* block = getBlockAt(position);
    if (block && block->type != 0 && block->type != 3 && block->type != 9) {
        //std::cout << "Before Mining: Block Type = " << block->type << " at " << position.x << ", " << position.y << ", " << position.z << "\n";
        bool tnt = false;
        int lifeTime = 1000;
        if (block->type == 11) {
            // Add TNT to activeTNT map
            glm::ivec3 blockPos = glm::floor(position);
            int tntID = static_cast<int>(activeTNT.size()); // Assign a unique ID based on current size
            activeTNT[tntID] = triggeredTNT{
                .pos = blockPos, // Store the position of the TNT
                .radius = 5,          // Set the explosion radius (adjust as needed)
                .currentWave = 0,      // Start from the first wave
                .delay = 200
            };
            tnt = true;
            lifeTime = 200;
            std::cout << "TNT block triggered at position: " << glm::to_string(blockPos) << "\n";
        }

        // Spawn a mined block
        // Create a random device and generator
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> randomVelocity(-0.2f, 0.2f);

        // Add the mined block with random velocity in x and z
        activeMinedBlocks.push_back({
            .pos = position + glm::vec3(0.0f, 1.0f, 0.0f), // Centered position
            .orientation = glm::vec3(0.0f),                 // Initial orientation
            .velocity = glm::vec3(randomVelocity(gen), 1.0f, randomVelocity(gen)), // Random x, z velocity
            .type = block->type,                            // Type of the mined block
            .lifetime = lifeTime,
            .tnt = tnt                                // Lifetime in ticks
        });

        block->type = 0;
        block->isSource = false;
        block->sourceID = -1;
        block->updated = true;

        glm::ivec3 blockPos = glm::floor(position);
        auto it = combinedChunk.find(blockPos);
        if (it != combinedChunk.end()) {
            combinedChunk[blockPos].type = 0;
        }
        
        Chunk* chunk = getChunkAt(position);
        if (!chunk) {
            return false; // If the chunk is not loaded, return
        }
        if (chunk) {
            chunk->isGenerated = true; // Mark chunk as dirty
            chunk->isDirty = true;
            // Check neighboring blocks for water
            bool found = false;
            for (int dx = -1; dx <= 1; ++dx) {
                if(found) continue;
                for (int dy = -1; dy <= 1; ++dy) {
                    if(found) continue;
                    for (int dz = -1; dz <= 1; ++dz) {
                        if(found) continue;
                        if (abs(dx) + abs(dy) + abs(dz) != 1) continue; // Only direct neighbors
                        glm::vec3 neighborPos = position + glm::vec3(dx, dy, dz);
                        Voxel* neighbor = getBlockAt(neighborPos);
                        //std::cout << "Checking pos: " << glm::to_string(neighborPos) << ", Block is: " << neighbor->type << ", source is: " << neighbor->isSource << "\n";
                        if (neighbor && (neighbor->type == 9 && neighbor->isSource)) {
                            //std::cout << "Found neighboring water with source ID: " << neighbor->sourceID << "\n";
                            handleWaterBlock(neighborPos, *neighbor);
                        }
                    }
                }
            }
            // Check for sand above the removed block
            if (blockPos.y + 1 < CHUNK_SIZE_Y) {
                Voxel& above = chunk->voxels[blockPos.x][blockPos.y + 1][blockPos.z];
                if (above.type && above.type == 8) { // Sand block
                    simulateSand(*chunk, blockPos.x, blockPos.y + 1, blockPos.z);
                }
            }
        }
        //std::cout << "After Mining: Block Type = " << block->type << " at " << position.x << ", " << position.y << ", " << position.z << "\n";
        return true;
    }
    return false;
}

void ChunkManager::handleWaterBlock(const glm::vec3& position, Voxel& block) {
    // If the block is a source block
    if (block.sourceID >= 0) {
        onWaterSourceAdded(position, block.sourceID);
    } else {
        // Assign a temporary ID for untracked source blocks
        int tempID = tempIDStart--;
        block.sourceID = tempID;
        generateWaterQueue[position] = {tempID, 0};
    }
}

bool ChunkManager::isVoxelVisibleToPlayer(const Chunk& chunk, int x, int y, int z) const {
    // Check if the voxel is at the edge of the chunk or has air as a neighbor
    if (chunk.voxels[x - 1][y][z].type == 0) return true; // Left neighbor
    if (chunk.voxels[x + 1][y][z].type == 0) return true; // Right neighbor
    if (chunk.voxels[x][y - 1][z].type == 0) return true; // Bottom neighbor
    if (chunk.voxels[x][y + 1][z].type == 0) return true; // Top neighbor
    if (chunk.voxels[x][y][z - 1].type == 0) return true; // Front neighbor
    if (chunk.voxels[x][y][z + 1].type == 0) return true; // Back neighbor
    

    return false; // Not visible
}

std::vector<Chunk*> ChunkManager::getVisibleChunks(const glm::vec3& playerPosition, int viewDistance, const glm::mat4& viewProjectionMatrix) {
    std::vector<Chunk*> visibleChunks;
    for (auto& [key, chunk] : chunks) {
            visibleChunks.push_back(&chunk);
        
    }
    return visibleChunks;
}

std::optional<glm::vec3> ChunkManager::getTargetBlock(const glm::vec3& playerPosition, const glm::vec3& viewDirection) {
    // Normalize the view direction
    glm::vec3 rayDir = glm::normalize(viewDirection);
    glm::vec3 rayPos = playerPosition;

    float maxDistance = 5.0f; // Maximum distance to check
    float stepSize = 0.1f;    // Step size for ray increments

    // Iterate along the ray
    for (float t = 0; t <= maxDistance; t += stepSize) {
        // Calculate the current position along the ray
        glm::vec3 currentPos = rayPos + t * rayDir;

        // Round to the nearest block position
        glm::ivec3 blockPos = glm::floor(currentPos);

        // Get the block at this position
        const Voxel* block = getBlockAt(blockPos);

        // Check if the block is non-empty
        if (block && block->type != 0 && block->type != 9) {
            return blockPos; // Return the world position of the target block
        }
    }

    // No block found within the ray distance
    return std::nullopt;
}

const std::unordered_map<ChunkKey, Chunk, ChunkKeyHasher> ChunkManager::getAllChunks() const {
    // Create a combined map to hold all chunks
    std::unordered_map<ChunkKey, Chunk, ChunkKeyHasher> allChunks;

    // Add loaded chunks
    for (const auto& [key, chunk] : chunks) {
        allChunks[key] = chunk;
    }

    // Add saved (unloaded) chunks
    for (const auto& [key, chunk] : savedChunks) {
        if (allChunks.find(key) == allChunks.end()) {
            allChunks[key] = chunk; // Add only if not already in allChunks
        }
    }
    return allChunks;
}
