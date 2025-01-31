#include "interactionManager.hpp"
#include <iostream>
#include <ApplicationServices/ApplicationServices.h>


void InteractionManager::resolveCollisions(glm::vec3& playerPosition, ChunkManager& chunkManager) {
    glm::ivec3 testPosition = playerPosition;
    int groundY = INT_MIN;

    // Find the highest solid block below the player
    for (int y = testPosition.y - 1; y >= 0; --y) {
        testPosition.y = y;
        auto it = chunkManager.combinedChunk.find(testPosition);
        if (it != chunkManager.combinedChunk.end() && 
            it->second.type != 0 && it->second.type != 9) {
            groundY = y;
            break;
        }
    }

    // Adjust player position
    if (groundY != INT_MIN && playerPosition.y < groundY + 1.0f - 0.01f) { 
        playerPosition.y = static_cast<float>(groundY + 1.0f);
    }
}

void center(int centerX, int centerY) {
    CGPoint center = {
        .x = static_cast<CGFloat>(centerX),
        .y = static_cast<CGFloat>(centerY)
    };
    CGWarpMouseCursorPosition(center);
}

void InteractionManager::getLook(tga::Interface& tgai, tga::Window window, float& x, float& y, int centerX, int centerY, int& lookTimer) {
    static bool cursorJustReset = false; // To prevent immediate re-reset after centering
    static std::pair<int, int> lastMousePos = tgai.mousePosition(window);

    // Get current mouse position
    auto currentMousePos = tgai.mousePosition(window);

    // If the cursor was just reset, ignore movement this frame
    if (cursorJustReset) {
        lastMousePos = currentMousePos;
        cursorJustReset = false;
        return; // Skip processing to prevent double movement
    }

    // Calculate mouse movement delta
    float deltaX = static_cast<float>(currentMousePos.first - lastMousePos.first);
    float deltaY = static_cast<float>(currentMousePos.second - lastMousePos.second);

    // Update camera angles
    float sensitivity = 0.5f;
    x = glm::clamp(x - deltaY * sensitivity, -89.0f, 89.0f); // Pitch (vertical rotation)
    y += deltaX * sensitivity; // Yaw (horizontal rotation)

    if(currentMousePos.second < centerY -centerY || currentMousePos.second > centerY + centerY/2 
    || currentMousePos.first < centerX -centerX || currentMousePos.first > centerX + centerX/2){
        //center(centerX, centerY); // Reset cursor position
        //lastMousePos = tgai.mousePosition(window);    // Immediately update lastMousePos
        //cursorJustReset = true;               // Mark that the cursor was reset
        lookTimer = 0;                        // Reset the timer
    } else {
        lastMousePos = currentMousePos;       // Update lastMousePos for normal movement
    }

    lookTimer++;
}

glm::vec3 getMovement(tga::Interface& tgai, tga::Window window, glm::vec2 playerView, glm::vec3& forward){
    // Update forward vector based on the view angles
    forward = glm::normalize(glm::vec3(
        cos(glm::radians(playerView.y)) * cos(glm::radians(playerView.x)),
        sin(glm::radians(playerView.x)),
        sin(glm::radians(playerView.y)) * cos(glm::radians(playerView.x))
    ));

    glm::vec3 forwardXZ = glm::vec3(forward.x, 0, forward.z);
    glm::vec3 right = glm::normalize(glm::cross(forwardXZ, glm::vec3(0, 1, 0)));

    // Movement inputs
    glm::vec3 direction(0.0f);
    if (tgai.keyDown(window, tga::Key::W)) {
        direction += forwardXZ; // Forward
    }
    if (tgai.keyDown(window, tga::Key::S)) {
        direction -= forwardXZ; // Backward
    }
    if (tgai.keyDown(window, tga::Key::A)) {
        direction -= right; // Left
    }
    if (tgai.keyDown(window, tga::Key::D)) {
        direction += right; // Right
    }

    return direction;
}

void mineOrPlaceBlocks(ChunkManager& chunkManager, int blockType, tga::Interface& tgai, tga::Window& window, int& buildTimer, int& mineTimer, glm::vec3& blockWorldPos, glm::vec3& playerPosition, const glm::vec3& forwardView){
    if(mineTimer < 10){
        mineTimer++;
    }
    if (tgai.keyDown(window, tga::Key::MouseLeft) && mineTimer == 10) {
            if(chunkManager.mineBlock(blockWorldPos)){
                //std::cout << "Block at: ( " << blockWorldPos.x << ", " << blockWorldPos.y << ", " << blockWorldPos.z << ") was removed\n";
                mineTimer = 0;
            } else {
            std::cout << "Could not remove block at: ( " << blockWorldPos.x << ", " << blockWorldPos.y << ", " << blockWorldPos.z << ")\n";
        }
    }
    
    if(buildTimer < 20){
        buildTimer++;
    }
    if (tgai.keyDown(window, tga::Key::MouseRight) && buildTimer == 20) {
        //std::cout << "Sending target block: ( " << glm::to_string(blockWorldPos) << ")\n";
        auto result = chunkManager.getPlacementPosition( playerPosition, forwardView, blockWorldPos);
        auto& block = *result;

        glm::vec3 playerSize = glm::vec3(0.5f, 1.7f, 0.5f);
        if(chunkManager.placeBlock(block, blockType, playerPosition, playerSize)){
                buildTimer = 0;
        } 
    }
}

void InteractionManager::handleMovement(ChunkManager& chunkManager, glm::vec3& blockWorldPos, bool& flying, bool& inWater, int& buildTimer, int& flyTimer,int& mineTimer, glm::vec3& playerPosition,
                                        glm::vec3& velocity, glm::vec3& forward,
                                        glm::vec2& playerView, float dt, tga::Interface& tgai, tga::Window window, int centerX, int centerY, int blockType, int& lookTimer) {
    float moveSpeed = 4.0f * dt;
    glm::vec3 oldPos = playerPosition;
    if(flyTimer > 0){
        flyTimer--;
    }


    if (tgai.keyDown(window, tga::Key::F) && flyTimer <= 0) {
        flyTimer = 30;
        if(!flying){
            flying = true;
            std::cout << "Flight mode is on\n";
        } else {
            flying = false;

            std::cout << "Flight mode is off\n";
        }
    }

    if(flying){
        if (tgai.keyDown(window, tga::Key::Shift_Left)) {
            moveSpeed *= 3.0f; // Sprinting
        }
        getLook(tgai, window, playerView.x, playerView.y, centerX, centerY, lookTimer);
        glm::vec3 direction = getMovement(tgai, window, playerView, forward);
        if (tgai.keyDown(window, tga::Key::Space)) playerPosition.y += dt * moveSpeed * 70;
        if (tgai.keyDown(window, tga::Key::C)) playerPosition.y -= dt * moveSpeed * 70;
        playerPosition += direction * moveSpeed;
    } 
    if(!flying){
        if (tgai.keyDown(window, tga::Key::Shift_Left)) {
            moveSpeed *= 1.4f; // Sprinting
        }

    if(inWater){
        moveSpeed *= 0.5f; // Slower movement in water

        // Swimming mechanics
        if (tgai.keyDown(window, tga::Key::Space)) {
            velocity.y = 3.0f; // Swim upwards
        } else if (velocity.y > -2.0f) {
            velocity.y -= 4.0f * dt; // Gentle downward motion
        }
        getLook(tgai, window, playerView.x, playerView.y, centerX, centerY, lookTimer);
        glm::vec3 direction = getMovement(tgai, window, playerView, forward);

        // Normalize direction and apply speed
        if (glm::length(direction) > 0.0f) {
            direction = glm::normalize(direction);
            glm::vec3 newPosition = playerPosition + direction * moveSpeed;

            // Handle collisions on X and Z axes
            glm::vec3 testPositionX = {newPosition.x, playerPosition.y, playerPosition.z};
            if (!isPositionColliding(testPositionX, chunkManager)) {
                playerPosition.x = testPositionX.x; // Move along X-axis if no collision
            }

            glm::vec3 testPositionZ = {playerPosition.x, playerPosition.y, newPosition.z};
            if (!isPositionColliding(testPositionZ, chunkManager)) {
                playerPosition.z = testPositionZ.z; // Move along Z-axis if no collision
            }
        }
        
        glm::vec3 newPositionY = {playerPosition.x, playerPosition.y + velocity.y * dt, playerPosition.z};

        // Handle collisions on the Y axis (floors and ceilings)
        if (isPositionColliding(newPositionY, chunkManager)) {
            // Collision detected, resolve it and reset velocity
            resolveCollisions(playerPosition, chunkManager);
            velocity.y = 0.0f;
        } else {
            int localX = static_cast<int>(std::floor(playerPosition.x)) % CHUNK_SIZE;
            int localZ = static_cast<int>(std::floor(playerPosition.z)) % CHUNK_SIZE;

            // Handle negative wrapping for local coordinates
            if (localX < 0) localX += CHUNK_SIZE;
            if (localZ < 0) localZ += CHUNK_SIZE;
            // Check if the player is falling below the current highest block
            int highestBlockY = TerrainManager::getClosestGroundWithClearance(chunkManager.getChunkAt(playerPosition)->voxels, localX, localZ,playerPosition.y);

            if (newPositionY.y < highestBlockY + 1.0f) {
                playerPosition.y = static_cast<float>(highestBlockY + 1.0f); // Clamp to the highest block's surface
                velocity.y = 0.0f;  // Stop downward motion
            } else {
                // If no collision and above ground, update the player's vertical position
                playerPosition.y = newPositionY.y;
            }
        }

        
    } else {
        getLook(tgai, window, playerView.x, playerView.y, centerX, centerY, lookTimer);
        glm::vec3 direction = getMovement(tgai, window, playerView, forward);

        // Normalize direction and apply speed
        if (glm::length(direction) > 0.0f) {
            direction = glm::normalize(direction);
            glm::vec3 newPosition = playerPosition + direction * moveSpeed;

            // Handle collisions on X and Z axes
            glm::vec3 testPositionX = {newPosition.x, playerPosition.y, playerPosition.z};
            if (!isPositionColliding(testPositionX, chunkManager)) {
                playerPosition.x = testPositionX.x; // Move along X-axis if no collision
            }

            glm::vec3 testPositionZ = {playerPosition.x, playerPosition.y, newPosition.z};
            if (!isPositionColliding(testPositionZ, chunkManager)) {
                playerPosition.z = testPositionZ.z; // Move along Z-axis if no collision
            }
        }

        // Jump input
        if (tgai.keyDown(window, tga::Key::Space) && velocity.y == 0.0f) {
            velocity.y = 8.35f; // Jump strength
        }

        // Apply gravity
        velocity.y -= 9.8f * dt * 3; // Gravity acceleration
        glm::vec3 newPositionY = {playerPosition.x, playerPosition.y + velocity.y * dt, playerPosition.z};

    
        // Handle collisions on the Y axis (floors and ceilings)
        if (isPositionColliding(newPositionY, chunkManager)) {
            // Collision detected, resolve it and reset velocity
            resolveCollisions(playerPosition, chunkManager);
            velocity.y = 0.0f;
        } else {
            int localX = static_cast<int>(std::floor(playerPosition.x)) % CHUNK_SIZE;
            int localZ = static_cast<int>(std::floor(playerPosition.z)) % CHUNK_SIZE;

            // Handle negative wrapping for local coordinates
            if (localX < 0) localX += CHUNK_SIZE;
            if (localZ < 0) localZ += CHUNK_SIZE;
            // Check if the player is falling below the current highest block
            int highestBlockY = TerrainManager::getClosestGroundWithClearance(chunkManager.getChunkAt(playerPosition)->voxels, localX, localZ,playerPosition.y);

            if (newPositionY.y < highestBlockY + 1.0f) {
                playerPosition.y = static_cast<float>(highestBlockY + 1.0f); // Clamp to the highest block's surface
                velocity.y = 0.0f;  // Stop downward motion
            } else {
                // If no collision and above ground, update the player's vertical position
                playerPosition.y = newPositionY.y;
            }
        }
        //std::cout << "Player view angles: Pitch = " << playerView.x << ", Yaw = " << playerView.y << "\n";
        }
    }
    mineOrPlaceBlocks(chunkManager,  blockType, tgai,  window,buildTimer, mineTimer, blockWorldPos,  playerPosition, glm::normalize(forward));

}

bool InteractionManager::isPositionColliding(const glm::vec3& position, const ChunkManager& chunkManager) {
    const float playerWidth = 0.5f; // Increase width to avoid walking inside blocks
    const float playerHeight = 2.0f;

    // Iterate over a bounding box around the player's position
    for (float y = 0.0f; y < playerHeight; y += 0.5f) {
        for (float x = -playerWidth * 0.5f; x <= playerWidth * 0.5f; x += 0.5f) {
            for (float z = -playerWidth * 0.5f; z <= playerWidth * 0.5f; z += 0.5f) {
                glm::ivec3 testPosition = glm::floor(position + glm::vec3(x, y, z)); // Check expanded area

                auto it = chunkManager.combinedChunk.find(testPosition);
                if (it != chunkManager.combinedChunk.end()) {
                    if (it->second.type != 0 && it->second.type != 9) {
                        return true; // Collision detected (solid block, not water)
                    }
                }
            }
        }
    }

    return false; // No collision
}




