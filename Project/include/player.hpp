#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "interactionManager.hpp"
#include "chunkManager.hpp"

class Player {
public:
    Player(const glm::vec3& startPosition);

    glm::vec3 getPosition() const;
    glm::vec3 getForward() const;
    glm::vec3 getRight() const;
    glm::vec2 getView() const;
    bool isFlying(){return flying;}

    void setY(float y){
        position.y = y;
    }
    
    void update(ChunkManager& chunkManager, float dt, tga::Interface& tgai, tga::Window window, glm::vec3& blockWorldPos, int centerX, int centerY, int blockType, int& lookTimer);

private:
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 forward; // Direction the player is facing
    glm::vec2 playerView;
    float yaw;         // Yaw angle for rotation
    float pitch;       // Pitch angle for rotation
    bool flying;
    int flyTimer;
    int mineTimer;
    int buildTimer;
};
