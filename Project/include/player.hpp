#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "interactionManager.hpp"
#include "chunkManager.hpp"

class Player {
public:
    Player(const glm::vec3& startPosition);

    void reset() {
        position = glm::vec3(0.0f, 0.0f, 0.0f); // Reset to origin or default spawn point
        velocity = glm::vec3(0.0f);             // Clear movement velocity
        forward = glm::vec3(0.0f, 0.0f, -1.0f); // Reset direction to face forward
        playerView = glm::vec2(0.0f);           // Reset view angles
        yaw = 0.0f;
        pitch = 0.0f;
        flying = false;                         // Reset flying state
        flyTimer = 0;
        mineTimer = 0;
        buildTimer = 0;
        collectedBlocks.clear();               // Clear inventory
    }


    glm::vec3 getPosition() const;
    glm::vec3 getForward() const;
    glm::vec3 getRight() const;
    glm::vec2 getView() const;
    bool isFlying(){return flying;}

    void setY(float y){
        position.y = y;
    }
    
    void update(ChunkManager& chunkManager, float dt, tga::Interface& tgai, tga::Window window, glm::vec3& blockWorldPos, glm::vec3& cameraPosition, int centerX, int centerY, int blockType, int& lookTimer);
    std::vector<int> collectedBlocks;
private:
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 forward; // Direction the player is facing
    glm::vec2 playerView;
    glm::vec3 wiggleOffset;
    float yaw;         // Yaw angle for rotation
    float pitch;       // Pitch angle for rotation
    bool flying;
    int flyTimer;
    int mineTimer;
    int buildTimer;
    
};
