#include "player.hpp"

Player::Player(const glm::vec3& startPosition)
    : position(startPosition), 
    velocity(0.0f) , 
    playerView({0.0f, 0.0f}), 
    flying(false), 
    flyTimer(0) , 
    mineTimer(0), 
    buildTimer(0), 
    collectedBlocks{std::vector<int>(15, 0)} {}

glm::vec3 Player::getPosition() const {
    return position;
}

glm::vec3 Player::getForward() const {
    return forward;
}

glm::vec3 Player::getRight() const {
    return glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
}

void Player::update(ChunkManager& chunkManager, float dt, tga::Interface& tgai, tga::Window window, glm::vec3& blockWorldPos, glm::vec3& cameraPosition, int centerX, int centerY, int blockType, int& lookTimer) {
    static glm::vec3 previousPosition = position; // Store the position from the last frame
    static glm::vec3 wiggleOffset = glm::vec3(0.0f);
    static float wiggleTime = 0.0f;

    glm::vec3 oldOffset = wiggleOffset;
    bool inWater = false;
        glm::ivec3 playerBlockPos = glm::floor(position);
        auto waterCheck = chunkManager.combinedChunk.find(playerBlockPos);
        if (waterCheck != chunkManager.combinedChunk.end() && 
            (waterCheck->second.type == 9)) {
            inWater = true;
        }

    // Call movement logic
    InteractionManager::handleMovement(chunkManager, blockWorldPos, flying, inWater, buildTimer, flyTimer,  mineTimer, position, velocity, forward, playerView, dt, tgai, window, centerX, centerY, blockType, lookTimer);

    // Calculate horizontal movement delta
    glm::vec3 movementDelta = position - previousPosition;
    movementDelta.y = 0.0f; // Ignore vertical movement for wiggle calculation
    

    if (!flying && !inWater &&  glm::length(movementDelta) > 0.0f && velocity.y == 0) {
        float wiggleSpeed = 15.0f; // Base wiggle speed
        float wiggleAmount = 0.15f; // Adjust for desired intensity
        if (tgai.keyDown(window, tga::Key::Shift_Left)) {
            wiggleSpeed *= 1.5f; // Increase wiggle speed when sprinting
            wiggleAmount = 0.2f;
        }

        // Update wiggle time based on movement speed
        wiggleTime += dt * wiggleSpeed;

        // Calculate wiggle offsets
        
        float verticalWiggle = sin(wiggleTime) * wiggleAmount;
        float horizontalWiggle = cos(wiggleTime * 0.5f) * wiggleAmount * 0.5f;

        // Smoothly interpolate to the target offset
        glm::vec3 targetOffset = glm::vec3(horizontalWiggle, verticalWiggle, 0.0f);
        wiggleOffset = glm::mix(wiggleOffset, targetOffset, dt * 10.0f); // Smooth transition
    } else {
        // Smoothly reset the wiggle offset when not moving
        wiggleOffset = glm::mix(wiggleOffset, glm::vec3(0.0f), dt * 10.0f);
    }

    // Apply the wiggle offset to the camera position
    cameraPosition += wiggleOffset;

    // Update the previous position for the next frame
    previousPosition = position;
}


