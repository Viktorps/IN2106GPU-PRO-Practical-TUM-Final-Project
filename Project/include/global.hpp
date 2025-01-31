#include <glm/glm.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "tga/tga_utils.hpp"
#include "tga/tga.hpp"
#include "tga/tga_createInfo_structs.hpp"
#include <vulkan/vulkan.h>
#include <tga/tga_vulkan/tga_vulkan_WSI.hpp>
#include "player.hpp"
#include <glm/gtx/string_cast.hpp>
#include <span>
#include <sstream> 
#include <ApplicationServices/ApplicationServices.h>
#include <vector>

void handleOptions(tga::Window& window, tga::Interface& tgai, int& cullTimer, bool& enableCull, int& viewDistance, int& distanceTimer, auto& camData){
    if (tgai.keyDown(window, tga::Key::P) && cullTimer <= 0) {
        enableCull = !enableCull;
        if(enableCull){
            std::cout << "Cull is on.\n";
        } else {
            std::cout << "Cull is off.\n";
        }
        cullTimer = 50;
    }
    if(cullTimer > 0){
        cullTimer--;
    }
    if (tgai.keyDown(window, tga::Key::Up) && viewDistance < 5 && distanceTimer <= 0) {
        viewDistance++;
        std::cout << "Current view distance is: " << viewDistance << " chunks.\n";
        distanceTimer = 40;
    }
    if (tgai.keyDown(window, tga::Key::Down) && viewDistance > 1 && distanceTimer <= 0) {
            viewDistance--;
            std::cout << "Current view distance is: " << viewDistance << " chunks.\n";
            distanceTimer = 40;
    }
    if(distanceTimer > 0){
            distanceTimer--;
    }

    if (tgai.keyDown(window, tga::Key::O)) {
            camData.fov = glm::clamp(camData.fov + 1.0f, 30.0f, 120.0f); // Clamp FOV between 30° and 120°
        }
        if (tgai.keyDown(window, tga::Key::L)) {
            camData.fov = glm::clamp(camData.fov - 1.0f, 30.0f, 120.0f);
        }
}

void createFinalTextureLayout(const std::string& topPath, const std::string& sidePath, const std::string& bottomPath, const std::string& outputPath) {
    int topWidth, topHeight, sideWidth, sideHeight, bottomWidth, bottomHeight, channels;
    unsigned char* topData = stbi_load(topPath.c_str(), &topWidth, &topHeight, &channels, 4);
    unsigned char* sideData = stbi_load(sidePath.c_str(), &sideWidth, &sideHeight, &channels, 4);
    unsigned char* bottomData = stbi_load(bottomPath.c_str(), &bottomWidth, &bottomHeight, &channels, 4);

    if (!topData || !sideData || !bottomData) {
        throw std::runtime_error("Failed to load one or more textures.");
    }

    // Ensure all textures are the same size
    if (topWidth != sideWidth || topHeight != sideHeight || bottomWidth != sideWidth || bottomHeight != sideHeight) {
        throw std::runtime_error("All textures must have the same dimensions.");
    }

    int combinedWidth = sideWidth * 3;  // Three textures horizontally
    int combinedHeight = sideHeight * 4; // Four textures vertically
    std::vector<unsigned char> combinedTexture(combinedWidth * combinedHeight * 4, 0);

    auto copyRegion = [&](unsigned char* source, int srcWidth, int srcHeight, int startX, int startY, bool rotate = false) {
        for (int y = 0; y < srcHeight; ++y) {
            for (int x = 0; x < srcWidth; ++x) {
                int srcIndex = (y * srcWidth + x) * 4;
                int dstX = rotate ? srcHeight - 1 - y + startX : x + startX;
                int dstY = rotate ? x + startY : y + startY;
                int dstIndex = (dstY * combinedWidth + dstX) * 4;
                for (int c = 0; c < 4; ++c) {
                    combinedTexture[dstIndex + c] = source[srcIndex + c];
                }
            }
        }
    };

    // Layout the textures
    // Top [Side]
    copyRegion(sideData, sideWidth, sideHeight, sideWidth, 0);

    // Middle [Side (rotated)], [Top], [Side (rotated)]
    copyRegion(sideData, sideWidth, sideHeight, 0, sideHeight, true);
    copyRegion(topData, topWidth, topHeight, sideWidth, sideHeight);
    copyRegion(sideData, sideWidth, sideHeight, sideWidth * 2, sideHeight, true);

    // Bottom [Side]
    copyRegion(sideData, sideWidth, sideHeight, sideWidth, sideHeight * 2);

    // Bottom row [Bottom]
    copyRegion(bottomData, bottomWidth, bottomHeight, sideWidth, sideHeight * 3);

    // Write the combined texture
    if (!stbi_write_png(outputPath.c_str(), combinedWidth, combinedHeight, 4, combinedTexture.data(), combinedWidth * 4)) {
        throw std::runtime_error("Failed to save combined texture.");
    }

    // Free the loaded textures
    stbi_image_free(topData);
    stbi_image_free(sideData);
    stbi_image_free(bottomData);
}

template <typename T>
std::tuple<T&, tga::StagingBuffer, size_t> stagingBufferOfType(tga::Interface& tgai)
{
    auto stagingBuff = tgai.createStagingBuffer({sizeof(T)});
    return {*static_cast<T *>(tgai.getMapping(stagingBuff)), stagingBuff, sizeof(T)};
}

// Declare the global grass texture
tga::Texture greenTexture;
tga::Texture stoneTexture;
tga::Texture bedrockTexture;
tga::Texture woodTexture;
tga::Texture diamondTexture;
tga::Texture leavesTexture;
tga::Texture cobbleStoneTexture;
tga::Texture sandTexture;
tga::Texture waterTexture;
tga::Texture snowTexture;
tga::Texture tntTexture;
tga::Texture ironTexture;
tga::Texture coalTexture;
tga::Texture goldTexture;
tga::Texture sunTexture;

tga::Texture grassImage;
tga::Texture stoneImage;
tga::Texture bedrockImage;
tga::Texture woodImage;
tga::Texture diamondImage;
tga::Texture leavesImage;
tga::Texture cobbleStoneImage;
tga::Texture sandImage;
tga::Texture waterImage;
tga::Texture snowImage;
tga::Texture tntImage;

std::vector<tga::Texture> allTextures;
std::vector<tga::Texture> allImages;

// Initialize the grass texture
void initializeTextures(tga::Interface& tgai) {
    greenTexture = tga::loadTexture("models/Grass_Block_TEX.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::linear,
                                    tgai);
    std::cout << "Texture created successfully: " << (greenTexture != nullptr) << "\n";

    stoneTexture = tga::loadTexture("models/stone_texture.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);

    bedrockTexture = tga::loadTexture("models/bedrock.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);
    
    woodTexture = tga::loadTexture("models/wood_texture.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);

    diamondTexture = tga::loadTexture("models/diamond.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);
    
    leavesTexture = tga::loadTexture("models/leaves.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);
    
    cobbleStoneTexture = tga::loadTexture("models/cobbleStone.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);

    sandTexture = tga::loadTexture("models/sand.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);
                                    
    waterTexture = tga::loadTexture("models/water.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);

   snowTexture = tga::loadTexture("models/snow.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);
    
    tntTexture = tga::loadTexture("models/tnt.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);

    ironTexture = tga::loadTexture("models/iron.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);
    
    coalTexture = tga::loadTexture("models/coal.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);

    goldTexture = tga::loadTexture("models/gold.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);
    sunTexture = tga::loadTexture("models/sun.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);

    allTextures = {greenTexture, stoneTexture, bedrockTexture, diamondTexture, woodTexture, leavesTexture, cobbleStoneTexture, sandTexture, waterTexture, snowTexture, tntTexture, ironTexture, coalTexture, goldTexture,sunTexture};

}

void initializeImages(tga::Interface& tgai) {
    grassImage = tga::loadTexture("images/grass.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::linear,
                                    tgai);

    stoneImage = tga::loadTexture("images/stone.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);

    bedrockImage = tga::loadTexture("images/bedrock.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);
    
    woodImage = tga::loadTexture("images/logs.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);

    diamondImage = tga::loadTexture("images/diamond.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);
    
    leavesImage = tga::loadTexture("images/leaves.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);
    
    cobbleStoneImage = tga::loadTexture("images/cobblestone.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);

    sandImage = tga::loadTexture("images/sand.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);
                                    
    waterImage = tga::loadTexture("images/water.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);

   snowImage = tga::loadTexture("images/snow.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);
    
    tntImage = tga::loadTexture("images/tnt.png",
                                    tga::Format::r8g8b8a8_srgb,
                                    tga::SamplerMode::nearest,
                                    tgai);

    allImages = {grassImage, stoneImage, woodImage, leavesImage, cobbleStoneImage, sandImage, waterImage, snowImage, tntImage};

}

void saveWorldToBinary(const ChunkManager& chunkManager, const glm::vec3& playerPosition, const std::string& fileName) {
    std::string saveDir = "saves/";
    if (!std::filesystem::exists(saveDir)) {
        std::filesystem::create_directory(saveDir); // Create the saves folder if it doesn't exist
    }

    std::string fullFilePath = saveDir + fileName;
    std::ofstream outFile(fullFilePath, std::ios::binary);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open file for writing: " << fullFilePath << "\n";
        return;
    }

    // Save chunk size
    outFile.write(reinterpret_cast<const char*>(&CHUNK_SIZE), sizeof(CHUNK_SIZE));

    // Save player position
    outFile.write(reinterpret_cast<const char*>(&playerPosition), sizeof(playerPosition));

    // Save nextID and tempIDStart
    outFile.write(reinterpret_cast<const char*>(&chunkManager.nextID), sizeof(chunkManager.nextID));
    outFile.write(reinterpret_cast<const char*>(&chunkManager.tempIDStart), sizeof(chunkManager.tempIDStart));

    // Save freeIDs (priority queue)
    std::vector<int> freeIDsVec;
    auto tempQueue = chunkManager.freeIDs;
    while (!tempQueue.empty()) {
        freeIDsVec.push_back(tempQueue.top());
        tempQueue.pop();
    }
    uint32_t freeIDsCount = freeIDsVec.size();
    outFile.write(reinterpret_cast<const char*>(&freeIDsCount), sizeof(freeIDsCount));
    for (const auto& id : freeIDsVec) {
        outFile.write(reinterpret_cast<const char*>(&id), sizeof(id));
    }

    // Save activeFlows
    uint32_t activeFlowsCount = chunkManager.activeFlows.size();
    outFile.write(reinterpret_cast<const char*>(&activeFlowsCount), sizeof(activeFlowsCount));
    for (const auto& [sourceID, count] : chunkManager.activeFlows) {
        outFile.write(reinterpret_cast<const char*>(&sourceID), sizeof(sourceID));
        outFile.write(reinterpret_cast<const char*>(&count), sizeof(count));
    }

    // Save generateWaterQueue
    uint32_t generateWaterQueueCount = chunkManager.generateWaterQueue.size();
    outFile.write(reinterpret_cast<const char*>(&generateWaterQueueCount), sizeof(generateWaterQueueCount));
    for (const auto& [pos, data] : chunkManager.generateWaterQueue) {
        const int sourceID = data.first;
        const int travelDistance = data.second;
        outFile.write(reinterpret_cast<const char*>(&pos), sizeof(pos));          // Save position
        outFile.write(reinterpret_cast<const char*>(&sourceID), sizeof(sourceID)); // Save sourceID
        outFile.write(reinterpret_cast<const char*>(&travelDistance), sizeof(travelDistance)); // Save travelDistance
    }


    // Save removeWaterQueue
    uint32_t removeWaterQueueCount = chunkManager.removeWaterQueue.size();
    outFile.write(reinterpret_cast<const char*>(&removeWaterQueueCount), sizeof(removeWaterQueueCount));
    for (const auto& sourceID : chunkManager.removeWaterQueue) {
        outFile.write(reinterpret_cast<const char*>(&sourceID), sizeof(sourceID));
    }

    // Save waterBlocksByID
    uint32_t waterBlocksByIDCount = chunkManager.waterBlocksByID.size();
    outFile.write(reinterpret_cast<const char*>(&waterBlocksByIDCount), sizeof(waterBlocksByIDCount));
    for (const auto& [sourceID, blocks] : chunkManager.waterBlocksByID) {
        outFile.write(reinterpret_cast<const char*>(&sourceID), sizeof(sourceID));
        uint32_t blockCount = blocks.size();
        outFile.write(reinterpret_cast<const char*>(&blockCount), sizeof(blockCount));
        for (const auto& blockPos : blocks) {
            outFile.write(reinterpret_cast<const char*>(&blockPos), sizeof(blockPos));
        }
    }

    uint32_t chunkCount = chunkManager.getAllChunks().size();
    outFile.write(reinterpret_cast<const char*>(&chunkCount), sizeof(chunkCount));


    // Save chunks
    for (const auto& [key, chunk] : chunkManager.getAllChunks()) {
        outFile.write(reinterpret_cast<const char*>(&key), sizeof(key));
        outFile.write(reinterpret_cast<const char*>(&chunk.position), sizeof(chunk.position));
        // Write the number of non-zero voxels in the chunk
        int voxelCount = 0;
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    const Voxel& voxel = chunk.voxels[x][y][z];
                    if (voxel.type != 0) {
                        voxelCount++;
                    }
                }
            }
        }
        outFile.write(reinterpret_cast<const char*>(&voxelCount), sizeof(voxelCount));

        int legitBlocks = 0;
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    const Voxel& voxel = chunk.voxels[x][y][z];
                    if (voxel.type != 0) {
                        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE_Y || z < 0 || z >= CHUNK_SIZE) {
                            continue;
                        }
                        outFile.write(reinterpret_cast<const char*>(&x), sizeof(x));
                        outFile.write(reinterpret_cast<const char*>(&y), sizeof(y));
                        outFile.write(reinterpret_cast<const char*>(&z), sizeof(z));
                        outFile.write(reinterpret_cast<const char*>(&voxel.type), sizeof(voxel.type));
                        outFile.write(reinterpret_cast<const char*>(&voxel.isSource), sizeof(voxel.isSource));
                        outFile.write(reinterpret_cast<const char*>(&voxel.sourceID), sizeof(voxel.sourceID));
                        outFile.write(reinterpret_cast<const char*>(&voxel.visible), sizeof(voxel.visible));
                        legitBlocks++;
                    }
                }
            }
        }
        //std::cout << voxelCount << " voxel count in a chunk\n"; 
        //std::cout << legitBlocks << " legit blocks in a chunk\n"; 
    }
    
    outFile.close();
    std::cout << "World saved to: " << fileName << "\n";
}

bool loadWorldFromBinary(ChunkManager& chunkManager, const std::string& fileName, glm::vec3& playerPosition, int expectedChunkSize) {
    std::string savePath = "saves/" + fileName;

    std::ifstream inFile(savePath, std::ios::binary);
    if (!inFile.is_open()) {
        std::cerr << "Failed to open save file: " << savePath << "\n";
        return false;
    }

    // Read and check chunk size
    int fileChunkSize;
    inFile.read(reinterpret_cast<char*>(&fileChunkSize), sizeof(fileChunkSize));
    if (fileChunkSize != expectedChunkSize) {
        std::cerr << "Incompatible chunk size: Expected " << expectedChunkSize
                  << ", but found " << fileChunkSize << " in file. Load aborted.\n";
        return false;
    }
    std::cout << "Chunk size is compatible: " << fileChunkSize << "\n";

    // Clear the current world state
    chunkManager.clear();

    // Load player position
    inFile.read(reinterpret_cast<char*>(&playerPosition), sizeof(playerPosition));
    std::cout << "Loaded player position: " 
              << playerPosition.x << ", " 
              << playerPosition.y << ", " 
              << playerPosition.z << "\n";



    // Load nextID and tempIDStart
    inFile.read(reinterpret_cast<char*>(&chunkManager.nextID), sizeof(chunkManager.nextID));
    inFile.read(reinterpret_cast<char*>(&chunkManager.tempIDStart), sizeof(chunkManager.tempIDStart));

    // Load freeIDs (priority queue)
    uint32_t freeIDsCount;
    inFile.read(reinterpret_cast<char*>(&freeIDsCount), sizeof(freeIDsCount));
    for (uint32_t i = 0; i < freeIDsCount; ++i) {
        int id;
        inFile.read(reinterpret_cast<char*>(&id), sizeof(id));
        chunkManager.freeIDs.push(id);
    }

    // Load activeFlows
    uint32_t activeFlowsCount;
    inFile.read(reinterpret_cast<char*>(&activeFlowsCount), sizeof(activeFlowsCount));
    for (uint32_t i = 0; i < activeFlowsCount; ++i) {
        int sourceID, count;
        inFile.read(reinterpret_cast<char*>(&sourceID), sizeof(sourceID));
        inFile.read(reinterpret_cast<char*>(&count), sizeof(count));
        chunkManager.activeFlows[sourceID] = count;
    }

    // Load generateWaterQueue
    uint32_t generateWaterQueueCount;
    inFile.read(reinterpret_cast<char*>(&generateWaterQueueCount), sizeof(generateWaterQueueCount));
    for (uint32_t i = 0; i < generateWaterQueueCount; ++i) {
        glm::vec3 pos;
        int sourceID;
        int travelDistance;
        inFile.read(reinterpret_cast<char*>(&pos), sizeof(pos));                // Load position
        inFile.read(reinterpret_cast<char*>(&sourceID), sizeof(sourceID));      // Load sourceID
        inFile.read(reinterpret_cast<char*>(&travelDistance), sizeof(travelDistance)); // Load travelDistance
        chunkManager.generateWaterQueue[pos] = {sourceID, travelDistance};       // Add to queue
    }

    // Load removeWaterQueue
    uint32_t removeWaterQueueCount;
    inFile.read(reinterpret_cast<char*>(&removeWaterQueueCount), sizeof(removeWaterQueueCount));
    chunkManager.removeWaterQueue.resize(removeWaterQueueCount);
    for (uint32_t i = 0; i < removeWaterQueueCount; ++i) {
        inFile.read(reinterpret_cast<char*>(&chunkManager.removeWaterQueue[i]), sizeof(chunkManager.removeWaterQueue[i]));
    }

    // Load waterBlocksByID
    uint32_t waterBlocksByIDCount;
    inFile.read(reinterpret_cast<char*>(&waterBlocksByIDCount), sizeof(waterBlocksByIDCount));
    for (uint32_t i = 0; i < waterBlocksByIDCount; ++i) {
        int sourceID;
        uint32_t blockCount;
        inFile.read(reinterpret_cast<char*>(&sourceID), sizeof(sourceID));
        inFile.read(reinterpret_cast<char*>(&blockCount), sizeof(blockCount));
        std::vector<glm::vec3> blocks(blockCount);
        for (uint32_t j = 0; j < blockCount; ++j) {
            inFile.read(reinterpret_cast<char*>(&blocks[j]), sizeof(blocks[j]));
        }
        chunkManager.waterBlocksByID[sourceID] = std::move(blocks);
    }

    // Read the total number of chunks
    uint32_t chunkCount;
    inFile.read(reinterpret_cast<char*>(&chunkCount), sizeof(chunkCount));
    std::cout << "Loading " << chunkCount << " chunks...\n";

    for (uint32_t i = 0; i < chunkCount; ++i) {
        ChunkKey key;
        glm::vec3 position;
        int voxelCount;

        // Read chunk key, position, and voxel count
        inFile.read(reinterpret_cast<char*>(&key), sizeof(key));
        inFile.read(reinterpret_cast<char*>(&position), sizeof(position));
        inFile.read(reinterpret_cast<char*>(&voxelCount), sizeof(voxelCount));

        // Create the chunk
        Chunk chunk;
        chunk.key = key;
        chunk.position = position;
        chunk.isGenerated = true;
        chunk.isDirty = true;

        
        // Initialize all voxels to type 0 and visible false
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    chunk.voxels[x][y][z].type = 0;
                    chunk.voxels[x][y][z].visible = false;
                }
            }
        }

        // Read and overwrite non-zero voxels
        for (uint32_t j = 0; j < voxelCount; ++j) {
            int x, y, z;
            int type;
            bool isSource;
            int sourceID;
            bool visible;

            // Read voxel position and type
            inFile.read(reinterpret_cast<char*>(&x), sizeof(x));
            inFile.read(reinterpret_cast<char*>(&y), sizeof(y));
            inFile.read(reinterpret_cast<char*>(&z), sizeof(z));
            inFile.read(reinterpret_cast<char*>(&type), sizeof(type));
            inFile.read(reinterpret_cast<char*>(&isSource), sizeof(isSource));
            inFile.read(reinterpret_cast<char*>(&sourceID), sizeof(sourceID));
            inFile.read(reinterpret_cast<char*>(&visible), sizeof(visible));

            // Validate coordinates
            if (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE_Y && z >= 0 && z < CHUNK_SIZE) {
                chunk.voxels[x][y][z].type = type;
                chunk.voxels[x][y][z].visible = visible;
                chunk.voxels[x][y][z].isSource = isSource;
                chunk.voxels[x][y][z].sourceID = sourceID;
            }
        }

        // Add the chunk to the manager
        chunkManager.addChunk(key, std::move(chunk));
        std::cout << "Chunk " << i + 1 << "/" << chunkCount << " loaded with " << voxelCount << " blocks.\n";
    }


    inFile.close();
    std::cout << "World loaded from: " << savePath << "\n";
    return true;
}

void selectBlockType(tga::Interface& tgai, tga::Window& window, int& blockType){
    if (tgai.keyDown(window, tga::Key::n1)) {blockType = 1;} //grass
    if (tgai.keyDown(window, tga::Key::n2)) {blockType = 2;} //stone
    if (tgai.keyDown(window, tga::Key::n3)) {blockType = 5;} //diamond
    if (tgai.keyDown(window, tga::Key::n4)) {blockType = 6;} //wood
    if (tgai.keyDown(window, tga::Key::n5)) {blockType = 7;} //leaves
    if (tgai.keyDown(window, tga::Key::n6)) {blockType = 8;} //cobblestone
    if (tgai.keyDown(window, tga::Key::n7)) {blockType = 9;} //sand
    if (tgai.keyDown(window, tga::Key::n8)) {blockType = 10;} //Water stationary
    if (tgai.keyDown(window, tga::Key::n9)) {blockType = 11;} //snow
}

void moveCursorToCenter(int centerX, int centerY) {
    
    CGPoint center = {
        .x = static_cast<CGFloat>(centerX),
        .y = static_cast<CGFloat>(centerY)
    };
    CGWarpMouseCursorPosition(center);
}


struct Batch {
    struct Element {
        tga::StagingBuffer staging;
        tga::Buffer buffer;
        size_t capacity{0};
        uint32_t count{0};
        
        void destroy(tga::Interface& tgai) {
            if (staging) tgai.free(staging);
            if (buffer) tgai.free(buffer);
        }
    };
    bool updated = false;
    Element vertex;
    Element index;
    Element modelMatrices;
    Element drawCommands;
    Element blockTypes;
    Element boundingBoxes;
    Element materialIDs;
    std::vector<tga::Texture> textures;

    void destroy(tga::Interface& tgai) {
        vertex.destroy(tgai);
        index.destroy(tgai);
        modelMatrices.destroy(tgai);
        drawCommands.destroy(tgai);
        blockTypes.destroy(tgai);
        boundingBoxes.destroy(tgai);
        materialIDs.destroy(tgai);
    }
};

struct BatchRenderData {
    tga::RenderPass geometryPass;
    tga::InputSet geometryInput;
    tga::Buffer elementCount;
    tga::InputSet cullInput;

    void destroy(tga::Interface& tgai) {
        if (geometryPass) tgai.free(geometryPass);
        if (geometryInput) tgai.free(geometryInput);
        if (elementCount) tgai.free(elementCount);
        if (cullInput) tgai.free(cullInput);
    }
};

struct AABB {
    glm::vec4 min; //using vec4 for alignment reasons
    glm::vec4 max; //using vec4 for alignment reasons
};


Batch generateVoxelBatch(tga::Obj cubeModel, const std::vector<Chunk*>& visibleChunks, tga::Interface& tgai, const glm::vec3& playerPosition, int viewDistance) {
    Batch batch;

    batch.textures = allTextures;

    size_t maxVoxels = visibleChunks.size() * CHUNK_SIZE * CHUNK_SIZE_Y * CHUNK_SIZE;

    // Initialize staging buffers
    auto initStaging = [&](Batch::Element& element, size_t elementSize, size_t maxSize) {
        element.capacity = maxSize;
        element.staging = tgai.createStagingBuffer({maxSize * elementSize});
    };

    initStaging(batch.vertex, sizeof(tga::Vertex), maxVoxels * cubeModel.vertexBuffer.size()); 
    initStaging(batch.index, sizeof(uint32_t), maxVoxels * cubeModel.indexBuffer.size());   
    initStaging(batch.modelMatrices, sizeof(glm::mat4), maxVoxels);                         
    initStaging(batch.drawCommands, sizeof(tga::DrawIndexedIndirectCommand), maxVoxels);     
    initStaging(batch.materialIDs, sizeof(uint32_t), maxVoxels);                           
    initStaging(batch.boundingBoxes, sizeof(AABB), maxVoxels);   
    
    auto appendData = [&]<typename T>(Batch::Element& elem, std::span<T> span) {
                        if (elem.count + span.size() > elem.capacity) {
                            throw std::runtime_error("Buffer overflow in appendData");
                        }

                        void* mappedMemory = tgai.getMapping(elem.staging);
                        if (!mappedMemory) {
                            throw std::runtime_error("Failed to map staging buffer");
                        }

                        auto writePos = static_cast<T*>(mappedMemory) + elem.count;
                        std::memcpy(writePos, span.data(), span.size_bytes());
                        elem.count += span.size();
                    };                              // Add one for the sun

    // Process visible chunks
    for (auto* chunk : visibleChunks) {
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    const Voxel& voxel = chunk->voxels[x][y][z];
                    if (!voxel.visible) continue;

                    glm::vec3 worldPosition = chunk->position + glm::vec3(x, y, z);

                    

                    appendData(batch.vertex, std::span<tga::Vertex>{cubeModel.vertexBuffer});
                    appendData(batch.index, std::span<uint32_t>{cubeModel.indexBuffer});

                    glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), worldPosition);
                    appendData(batch.modelMatrices, std::span<glm::mat4>{&modelMatrix, 1});

                    tga::DrawIndexedIndirectCommand drawCmd = {
                        .indexCount = static_cast<uint32_t>(cubeModel.indexBuffer.size()),
                        .instanceCount = 1,
                        .firstIndex = batch.index.count,
                        .vertexOffset = static_cast<int32_t>(batch.vertex.count),
                        .firstInstance = batch.modelMatrices.count - 1
                    };
                    appendData(batch.drawCommands, std::span<tga::DrawIndexedIndirectCommand>{&drawCmd, 1});

                    uint32_t materialID = voxel.type - 1;
                    appendData(batch.materialIDs, std::span<uint32_t>{&materialID, 1});

                    AABB boundingBox = {glm::vec4(worldPosition, 0), glm::vec4(worldPosition, 0) + glm::vec4(1.0f)};
                    appendData(batch.boundingBoxes, std::span<AABB>{&boundingBox, 1});
                }
            }
        }
    }

    // Finalize buffers
    auto initBuffer = [&](Batch::Element& element, tga::BufferUsage usage, size_t elementSize) {
        element.buffer = tgai.createBuffer({usage, elementSize * element.count, element.staging});
    };

    initBuffer(batch.vertex, tga::BufferUsage::vertex, sizeof(tga::Vertex));
    initBuffer(batch.index, tga::BufferUsage::index, sizeof(uint32_t));
    initBuffer(batch.drawCommands, tga::BufferUsage::indirect | tga::BufferUsage::storage, sizeof(tga::DrawIndexedIndirectCommand));
    initBuffer(batch.modelMatrices, tga::BufferUsage::storage, sizeof(glm::mat4));
    initBuffer(batch.materialIDs, tga::BufferUsage::storage, sizeof(uint32_t));
    initBuffer(batch.boundingBoxes, tga::BufferUsage::storage, sizeof(AABB));

    return batch;
}

Batch generateVoxelBatchMined(tga::Obj cubeModel, const std::vector<MinedBlock>& activeMinedBlocks, tga::Interface& tgai, const glm::vec3& playerPosition, int viewDistance, glm::vec3 sunPos, float sunRadius) {
    Batch batch;
    
    batch.textures = allTextures;
    
    // Initialize staging buffers with capacity based on the expected number of voxels
    size_t maxVoxels = activeMinedBlocks.size() + 1;

    // Calculate max vertices and indices based on the cube model
    size_t maxVertices =  maxVoxels * cubeModel.vertexBuffer.size();
    size_t maxIndices = maxVoxels * cubeModel.indexBuffer.size();

    // Initialize staging buffers
    auto initStaging = [&](Batch::Element& element, size_t elementSize, size_t maxSize) {
        element.capacity = maxSize; // Set capacity
        element.staging = tgai.createStagingBuffer({maxSize * elementSize});
    };

    // Adjust vertex and index buffer sizes
    initStaging(batch.vertex, sizeof(tga::Vertex), maxVertices);
    initStaging(batch.index, sizeof(uint32_t), maxIndices);
    initStaging(batch.modelMatrices, sizeof(glm::mat4), maxVoxels); // Unchanged
    initStaging(batch.drawCommands, sizeof(tga::DrawIndexedIndirectCommand), maxVoxels); // Unchanged
    initStaging(batch.materialIDs, sizeof(uint32_t), maxVoxels); // Unchanged
    initStaging(batch.boundingBoxes, sizeof(AABB), maxVoxels); // Unchanged

    // Append geometry from the predefined object
        auto appendData = [&]<typename T>(Batch::Element& elem, std::span<T> span) {
            if (elem.count + span.size() > elem.capacity) {
                throw std::runtime_error("Buffer overflow in appendData");
            }

            void* mappedMemory = tgai.getMapping(elem.staging);
            if (!mappedMemory) {
                throw std::runtime_error("Failed to map staging buffer");
            }

            auto writePos = static_cast<T*>(mappedMemory) + elem.count;
            std::memcpy(writePos, span.data(), span.size_bytes());
            elem.count += span.size();
        };

    for (auto& block : activeMinedBlocks) {
        glm::vec3 worldPosition = block.pos;
        int scale = 1;
        if(block.tnt){
            scale = 2;
        }

        // Append vertex and index data
        appendData(batch.vertex, std::span<tga::Vertex>{cubeModel.vertexBuffer});
        appendData(batch.index, std::span<uint32_t>{cubeModel.indexBuffer});

        // Compute the model matrix with position and orientation
        glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), worldPosition);
        modelMatrix = glm::scale(modelMatrix, glm::vec3(scale)); 

        // Apply rotation based on the block's orientation
        modelMatrix *= glm::rotate(glm::mat4(1.0f), block.orientation.x, glm::vec3(1, 0, 0));
        modelMatrix *= glm::rotate(glm::mat4(1.0f), block.orientation.y, glm::vec3(0, 1, 0));
        modelMatrix *= glm::rotate(glm::mat4(1.0f), block.orientation.z, glm::vec3(0, 0, 1));

        // Append model matrix for this voxel
        appendData(batch.modelMatrices, std::span<glm::mat4>{&modelMatrix, 1});

        // Append draw command for this voxel
        tga::DrawIndexedIndirectCommand drawCmd = {
            .indexCount = static_cast<uint32_t>(cubeModel.indexBuffer.size()),
            .instanceCount = 1,
            .firstIndex = static_cast<uint32_t>(batch.index.count),
            .vertexOffset = static_cast<int32_t>(batch.vertex.count),
            .firstInstance = batch.modelMatrices.count
        };
        appendData(batch.drawCommands, std::span<tga::DrawIndexedIndirectCommand>{&drawCmd, 1});

        // Append material ID for this voxel
        uint32_t materialID = block.type - 1; // Use voxel type (1 = grass, 2 = stone, 3 = bedrock)
        appendData(batch.materialIDs, std::span<uint32_t>{&materialID, 1});

        // Append bounding box for this voxel
        AABB boundingBox = {glm::vec4(worldPosition, 0), glm::vec4(worldPosition, 0) + glm::vec4(1.0f)};
        appendData(batch.boundingBoxes, std::span<AABB>{&boundingBox, 1});
    }

    appendData(batch.vertex, std::span<tga::Vertex>{cubeModel.vertexBuffer});
    appendData(batch.index, std::span<uint32_t>{cubeModel.indexBuffer});

    // Compute the direction vector from the sun to the origin
    glm::vec3 direction = glm::normalize(-sunPos);

    // Define the fixed "up" vector
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    // Compute the right vector (cross product of up and direction)
    glm::vec3 right = glm::normalize(glm::cross(up, direction));

    // Recompute the corrected "up" vector to ensure orthogonality
    glm::vec3 correctedUp = glm::cross(direction, right);

    // Construct the rotation matrix manually
    glm::mat4 rotationMatrix = glm::mat4(
        glm::vec4(right, 0.0f),
        glm::vec4(correctedUp, 0.0f),
        glm::vec4(-direction, 0.0f),
        glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
    );

    // Create the sun model matrix
    glm::mat4 sunModelMatrix = glm::translate(glm::mat4(1.0f), sunPos);
    sunModelMatrix *= rotationMatrix; // Apply the rotation
    sunModelMatrix = glm::scale(sunModelMatrix, glm::vec3(sunRadius)); // Apply scaling


    appendData(batch.modelMatrices, std::span<glm::mat4>{&sunModelMatrix, 1});

    tga::DrawIndexedIndirectCommand sunDrawCmd = {
        .indexCount = static_cast<uint32_t>(cubeModel.indexBuffer.size()),
        .instanceCount = 1,
        .firstIndex = batch.index.count,
        .vertexOffset = static_cast<int32_t>(batch.vertex.count),
        .firstInstance = batch.modelMatrices.count - 1
    };
    appendData(batch.drawCommands, std::span<tga::DrawIndexedIndirectCommand>{&sunDrawCmd, 1});

    uint32_t sunMaterialID = 14; // Unique texture ID for the sun
    appendData(batch.materialIDs, std::span<uint32_t>{&sunMaterialID, 1});

    AABB sunBoundingBox = {glm::vec4(sunPos - glm::vec3(sunRadius), 0), glm::vec4(sunPos + glm::vec3(sunRadius), 0)};
    appendData(batch.boundingBoxes, std::span<AABB>{&sunBoundingBox, 1});

    // Finalize staging buffers
    auto initBuffer = [&](Batch::Element& element, tga::BufferUsage usage, size_t elementSize) {
        element.buffer = tgai.createBuffer({usage, elementSize * element.count, element.staging});
    };

    initBuffer(batch.vertex, tga::BufferUsage::vertex, sizeof(tga::Vertex));
    initBuffer(batch.index, tga::BufferUsage::index, sizeof(uint32_t));
    initBuffer(batch.drawCommands, tga::BufferUsage::indirect | tga::BufferUsage::storage, sizeof(tga::DrawIndexedIndirectCommand));
    initBuffer(batch.modelMatrices, tga::BufferUsage::storage, sizeof(glm::mat4));
    initBuffer(batch.materialIDs, tga::BufferUsage::storage, sizeof(uint32_t));
    initBuffer(batch.boundingBoxes, tga::BufferUsage::storage, sizeof(AABB));
    return batch;
}