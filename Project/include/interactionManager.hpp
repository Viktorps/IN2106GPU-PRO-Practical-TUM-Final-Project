#pragma once
#include <tga/tga.hpp>
#include <tga/tga_vulkan/tga_vulkan_WSI.hpp>
#include <tga/tga_math.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include "chunkManager.hpp"

class InteractionManager {
public:
    static void handleAction(ChunkManager& chunkManager, const glm::vec3& playerPosition, 
                             const glm::vec3& viewDirection, bool isMining);

    static void resolveCollisions(glm::vec3& position, ChunkManager& chunkManager);
    

    static void handleMovement(ChunkManager& chunkManager,glm::vec3& blockWorldPos,  bool& flying, bool& inWater, int& buildTimer, int& flyTimer, int& mineTimer,glm::vec3& playerPosition,
                                        glm::vec3& velocity, glm::vec3& forward,
                                        glm::vec2& playerView, float dt, tga::Interface& tgai, tga::Window window, int centerX, int centerY, int blockType, int& lookTimer);

    static void checkProximityToChunks(const glm::vec3& playerPosition, ChunkManager& chunkManager);
    static void getLook(tga::Interface& tgai, tga::Window window, float& x, float& y, int centerX, int centerY, int& lookTimer);
private:
    static bool isPositionColliding(const glm::vec3& position, const ChunkManager& chunkManager);
    static bool isPositionCollidingY(const glm::vec3& position, const ChunkManager& chunkManager);
};
