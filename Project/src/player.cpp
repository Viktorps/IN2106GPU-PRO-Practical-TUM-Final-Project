#include "player.hpp"

Player::Player(const glm::vec3& startPosition)
    : position(startPosition), velocity(0.0f) , playerView({0.0f, 0.0f}), flying(false), flyTimer(20) , mineTimer(20), buildTimer(20) {}

glm::vec3 Player::getPosition() const {
    return position;
}

glm::vec3 Player::getForward() const {
    return forward;
}

glm::vec3 Player::getRight() const {
    return glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
}

void Player::update(ChunkManager& chunkManager, float dt, tga::Interface& tgai, tga::Window window, glm::vec3& blockWorldPos, int centerX, int centerY, int blockType, int& lookTimer) {
    InteractionManager::handleMovement(chunkManager, blockWorldPos, flying, buildTimer, flyTimer, mineTimer, position, velocity, forward, playerView, dt, tgai, window, centerX, centerY, blockType, lookTimer);
}

