#include "global.hpp"

// Create Camera Buffer
struct Camera {
    glm::mat4 view;
    glm::mat4 toWorld;
    glm::mat4 projection;
    glm::vec3 targetBlockPosition; // Add this field
    float fov;
};

// Struct needs proper alignment to conform to uniform buffer alignment requirement
struct alignas(16) Light {
    glm::vec4 color_intensity; // (R, G, B, Intensity)
    glm::vec3 direction;       // For directional light
    float padding;  
    glm::mat4 lightSpaceMatrix;
};

struct BlockTypeUniform {
    int selectedBlockType;
};

struct WaterEffect {
    bool isUnderwater;
};

int main() {

    std::cout << "Minecraft Clone\n";
    tga::Interface tgai;

    /*
    //////// GENERATE TEXTURES
    createFinalTextureLayout("models/sun.png","models/sun.png", "models/sun.png" , "models/sun.png");
    return 0;
    */

    auto windowWidth = tgai.screenResolution().first;
    auto windowHeight = tgai.screenResolution().second;
    tga::Window window = tgai.createWindow({windowWidth/6*5, windowHeight/6*5});
    int centerX = static_cast<int>(windowWidth/2);
    int centerY = static_cast<int>(windowHeight/2);
    
    auto guiTexture = tga::loadTexture("images/hotbar.png",
                                   tga::Format::r8g8b8a8_srgb,
                                   tga::SamplerMode::linear,
                                   tgai);
    auto buttonTexture = tga::loadTexture("images/button.png",
                                   tga::Format::r8g8b8a8_srgb,
                                   tga::SamplerMode::linear,
                                   tgai);

    tga::Obj cubeModel = tga::loadObj("models/Grass_Block.obj");
    std::vector<tga::Vertex> cubeVertices = cubeModel.vertexBuffer;
    std::vector<uint32_t> cubeIndices = cubeModel.indexBuffer;
    
    float scaleFactor = 0.5f; // 1.0f / 2.0f
    for (auto& vertex : cubeModel.vertexBuffer) {
        vertex.position *= scaleFactor;
    }
    scaleFactor = 0.3;
    tga::Obj droppedBlocks = cubeModel;
    for (auto& vertex : droppedBlocks.vertexBuffer) {
        vertex.position *= scaleFactor;
    }

    // Generate a random seed using the current time
    ChunkManager chunkManager;
    chunkManager.setUpdated();
    glm::vec3 playerPosition = {0.0f, 0.0f, 0.0f}; // Start above the highest block

    // Generate the world (e.g., 20x20 chunks)
    int worldWidth = 25;
    int worldDepth = 25;

    size_t maxVoxels = worldWidth * worldDepth * CHUNK_SIZE * CHUNK_SIZE_Y * CHUNK_SIZE;
    size_t bufferSize = sizeof(Voxel) * maxVoxels;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    auto [camData, camStaging, camSize] = stagingBufferOfType<Camera>(tgai);
    camData.projection = glm::perspective_vk(glm::radians(90.f), static_cast<float>(windowWidth) / windowHeight, 0.1f, 1000.f);
    camData.fov = 70;
    auto camBuffer = tgai.createBuffer({tga::BufferUsage::uniform, camSize, camStaging});

    // Create staging buffer for lights
    auto [lightData, lightStaging, lightSize] = stagingBufferOfType<std::array<Light, 1>>(tgai);

    // Initialize sunlight (central point in the sky)
    auto& sunlight = lightData[0];
    sunlight.color_intensity = glm::vec4(1.0f, 0.9f, 0.8f, 1.0f); // Warm sunlight
    sunlight.direction = glm::normalize(glm::vec3(0.0f, -1.0f, 0.0f)); // Downward direction
    glm::mat4 lightProjection = glm::ortho(-100.f, 100.f, -100.f, 100.f, -200.f, 200.f);
    glm::mat4 lightView = glm::lookAt(glm::vec3(0.0f)- sunlight.direction * 100.0f, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)); 
    sunlight.lightSpaceMatrix = lightProjection * lightView;

    // Create buffer for lights
    auto lightBuffer = tgai.createBuffer({tga::BufferUsage::uniform, lightSize, lightStaging});

    //Initialize the texture (in global.hpp)
    initializeTextures(tgai);
    initializeImages(tgai);

    // Render Targets for 1. Pass
    auto albedoTex = tgai.createTexture({windowWidth, windowHeight, tga::Format::r8g8b8a8_unorm, tga::SamplerMode::linear});
    auto normalTex = tgai.createTexture({windowWidth, windowHeight, tga::Format::r16g16b16a16_sfloat, tga::SamplerMode::linear});
    auto positionTex = tgai.createTexture({windowWidth, windowHeight, tga::Format::r16g16b16a16_sfloat, tga::SamplerMode::linear});

    /*Note: use batched version here*/
    auto geometryVS = tga::loadShader("shaders/vs.spv", tga::ShaderType::vertex, tgai);
    auto geometryFS = tga::loadShader("shaders/fs.spv", tga::ShaderType::fragment, tgai);

    // Since every batch has their own pass, we need one pass to clear the targets and provide a template for the input
    // set
    auto geometryInitPass = tgai.createRenderPass(
        tga::RenderPassInfo{geometryVS, geometryFS, std::vector<tga::Texture>{albedoTex, normalTex, positionTex}}
            .setClearOperations(tga::ClearOperation::all)
            .setPerPixelOperations(tga::PerPixelOperations{}.setDepthCompareOp(tga::CompareOperation::lessEqual))
            .setVertexLayout(tga::Vertex::layout())
            .setInputLayout({tga::SetLayout{tga::BindingType::uniformBuffer},
                             tga::SetLayout{
                                            tga::BindingType::storageBuffer, 
                                            tga::BindingType::storageBuffer, 
                                            {tga::BindingType::sampler, static_cast<uint32_t>(allTextures.size())}}}));

    // Render Target for 2. Pass
    auto hdrOutputTex = tgai.createTexture({windowWidth, windowHeight, tga::Format::r16g16b16a16_sfloat, tga::SamplerMode::linear});
    auto lightingVS = tga::loadShader("shaders/fullscreen_vert.spv", tga::ShaderType::vertex, tgai);
    auto lightingFS = tga::loadShader("shaders/light_frag.spv", tga::ShaderType::fragment, tgai);
    auto lightingPass = tgai.createRenderPass(
        tga::RenderPassInfo{lightingVS, lightingFS, hdrOutputTex}
            .setClearOperations(tga::ClearOperation::all)
            .setPerPixelOperations(tga::PerPixelOperations{}.setDepthCompareOp(tga::CompareOperation::lessEqual))
            .setInputLayout(
                {tga::SetLayout{tga::BindingType::uniformBuffer, tga::BindingType::uniformBuffer},
                 tga::SetLayout{tga::BindingType::sampler, tga::BindingType::sampler, tga::BindingType::sampler}
                }));
                 
    // 3rd Render Pass renders to window. Since it's also a fullscreen pass, the same vertex shader from before is used
    auto postProcessingFS = tga::loadShader("shaders/post_frag.spv", tga::ShaderType::fragment, tgai);

    auto postProcessingPass = tgai.createRenderPass(
        tga::RenderPassInfo{lightingVS, postProcessingFS, window}
            .setClearOperations(tga::ClearOperation::all)
            .setPerPixelOperations(
                tga::PerPixelOperations{}
                    .setBlendEnabled(true)
                    .setSrcBlend(tga::BlendFactor::srcAlpha)
                    .setDstBlend(tga::BlendFactor::oneMinusSrcAlpha)
            )
            .setInputLayout({
                tga::SetLayout{tga::BindingType::sampler},      // HDR texture              
                tga::SetLayout{tga::BindingType::uniformBuffer}, // Block type uniform
                tga::SetLayout{tga::BindingType::sampler},      // GUI texture
                tga::SetLayout{{tga::BindingType::sampler, static_cast<uint32_t>(allImages.size())}},   
                tga::SetLayout{tga::BindingType::uniformBuffer}, //water effect
            })
    );

    auto [blockTypeData, blockTypeStaging, blockTypeSize] = stagingBufferOfType<BlockTypeUniform>(tgai);
    blockTypeData.selectedBlockType = 1;

    std::vector<tga::Binding> blockBindings;
    for (size_t i = 0; i < allImages.size(); ++i) {
        blockBindings.push_back({allImages[i], 0, static_cast<uint32_t>(i)});
    }

    std::cout << "BlockBinding size: " << blockBindings.size() << "\n";

    auto blockTextureSet = tgai.createInputSet({
        postProcessingPass,
            blockBindings,
        3 });

    auto blockTypeBuffer = tgai.createBuffer({
        tga::BufferUsage::uniform,
        blockTypeSize,
        blockTypeStaging  // Populate with initial value
    });
    tgai.free(blockTypeStaging);

    auto blockTypeInputSet = tgai.createInputSet({
        postProcessingPass, 
        {tga::Binding{blockTypeBuffer, 0}}, // Bind uniform buffer
        1 });

    auto [waterEffectData, waterEffectStaging, waterEffectSize] = stagingBufferOfType<WaterEffect>(tgai);
    waterEffectData.isUnderwater = false;

    auto guiInputSet = tgai.createInputSet({
        postProcessingPass,
        {tga::Binding{guiTexture, 0}},
        2 });

    auto waterEffectBuffer = tgai.createBuffer({
        tga::BufferUsage::uniform,
        waterEffectSize,
        waterEffectStaging
    });
    
    auto waterEffectInputSet = tgai.createInputSet({
        postProcessingPass,
        {tga::Binding{waterEffectBuffer, 0}},
        4 });
    tgai.free(waterEffectStaging);
                      
    auto geometryCamInput = tgai.createInputSet(
        {geometryInitPass, {tga::Binding{camBuffer, 0}}, 
                        0});
    auto lightingCamLightInput = tgai.createInputSet(
        {lightingPass, {tga::Binding{camBuffer, 0}, 
                        tga::Binding{lightBuffer, 1}}, 
                        0 });

    auto gBufferInput = tgai.createInputSet(
        {lightingPass, {tga::Binding{albedoTex, 0}, 
                        tga::Binding{normalTex, 1}, 
                        tga::Binding{positionTex, 2}}, 
                        1 });

    auto postProcessingInput = tgai.createInputSet(
        {postProcessingPass, {tga::Binding{hdrOutputTex, 0}}, 
                        0 });

    // A Compute Pass to perform the culling
    auto cullPass = tgai.createComputePass(
        {tga::loadShader("shaders/frustum_culling_comp.spv", tga::ShaderType::compute,tgai),
         tga::InputLayout{tga::SetLayout{tga::BindingType::uniformBuffer, tga::BindingType::storageBuffer,
                                         tga::BindingType::storageBuffer, tga::BindingType::storageBuffer,
                                         tga::BindingType::storageBuffer,
                                         tga::BindingType::uniformBuffer, tga::BindingType::storageBuffer}}
        });

    // The communication path to report how many instances where drawn in total
    auto numDrawnInstancesStaging = tgai.createStagingBuffer({sizeof(uint32_t)});
    auto numDrawnInstancesBuffer = tgai.createBuffer({tga::BufferUsage::storage, sizeof(uint32_t)});

    auto initBatchRenderData = [&](Batch& batch) {
        BatchRenderData rdata;
      
        std::vector<tga::Binding> batchBindings{tga::Binding{batch.modelMatrices.buffer, 0},
                                                    tga::Binding{batch.materialIDs.buffer, 1}};
        for (size_t i = 0; i < batch.textures.size(); ++i) {
            //std::cout << "Texture bound at index " << i << "\n";
            batchBindings.push_back({batch.textures[i], 2, static_cast<uint32_t>(i)});
        }

        rdata.geometryPass = tgai.createRenderPass(
            tga::RenderPassInfo{geometryVS, geometryFS, std::vector<tga::Texture>{albedoTex, normalTex, positionTex}}
                .setClearOperations(tga::ClearOperation::none)
                .setPerPixelOperations(
                tga::PerPixelOperations{}
                    .setBlendEnabled(true)
                    .setSrcBlend(tga::BlendFactor::srcAlpha)
                    .setDstBlend(tga::BlendFactor::oneMinusSrcAlpha)
            )
                .setVertexLayout(tga::Vertex::layout())
                .setInputLayout(
                    {tga::SetLayout{tga::BindingType::uniformBuffer},
                    /*Note: Set size of batches */
                    tga::SetLayout{tga::BindingType::storageBuffer,                    
                                    tga::BindingType::storageBuffer,
                                    {tga::BindingType::sampler, static_cast<uint32_t>(batch.textures.size())}}}));

        rdata.geometryInput = tgai.createInputSet({rdata.geometryPass, batchBindings, 1});

        // provide the number of objects in this batch
        auto elementCountStaging =
            tgai.createStagingBuffer({sizeof(uint32_t), tga::memoryAccess(batch.drawCommands.count)});
        rdata.elementCount = tgai.createBuffer({tga::BufferUsage::uniform, sizeof(uint32_t), elementCountStaging});
        tgai.free(elementCountStaging);

        rdata.cullInput = tgai.createInputSet(
            {cullPass,
            {tga::Binding{camBuffer, 0}, tga::Binding{batch.modelMatrices.buffer, 1},
            tga::Binding{batch.boundingBoxes.buffer, 2}, tga::Binding{batch.drawCommands.buffer, 3},
            tga::Binding{batch.materialIDs.buffer,4},
            tga::Binding{rdata.elementCount, 5}, tga::Binding(numDrawnInstancesBuffer, 6)}});

        return rdata;
    };
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    std::vector<tga::CommandBuffer> cmdBuffers(tgai.backbufferCount(window));
    int blockType = 1;

    double smoothedTime{0};
    auto prevTime = std::chrono::steady_clock::now();
    
    std::cout << "Start position is: ( " << playerPosition.x << ", " << playerPosition.y << ", " << playerPosition.z << ")\n";

    Batch batch;
    BatchRenderData renderData;

    Batch minedBatch;
    BatchRenderData minedRenderData;

    tga::CommandBuffer cmd;
    
    int mouseTimer = 20;

    std::string currentSaveFileName;
    std::string saveFileName = "moreDiamonds";
    int viewDistance = 1; // Render chunks within "x" chunks of the player
    int distanceTimer = 0;
    glm::vec3 sunPosition = glm::vec3(0, 100, 0);
    float sunRadius = 200.0f;

    ///////////// OPEN SAVED/CREATE NEW WORLD
    std::string savePath = "saves/" + saveFileName;
    bool enableCull = false;
    int cullTimer = 0;
    
    if(loadWorldFromBinary(chunkManager, saveFileName, playerPosition, CHUNK_SIZE)){
        std::cout << "World " << saveFileName << " loaded\n";
    } else {
        unsigned int randomSeed = static_cast<unsigned int>(std::time(nullptr));
        std::cout << "Random Seed is: " << randomSeed << "\n";
        PerlinNoise perlin(randomSeed);
        chunkManager.generateWorld(worldWidth, worldDepth, perlin);
        playerPosition.y = TerrainManager::getHighestBlock(chunkManager.getChunkAt(playerPosition)->voxels, 0, 0) + 2.f;
    }
    
    // Set the player's initial position
    Player player(playerPosition);    
    int lookTimer = 0;

    bool gameRunning = false;
    std::string newWorldName;
    auto& lastNumDrawnElements = *static_cast<uint32_t*>(tgai.getMapping(numDrawnInstancesStaging));

    // Render loop
    for (uint32_t frame{0}; !tgai.windowShouldClose(window); ++frame) {

        if (tgai.keyDown(window, tga::Key::Escape)) {
            std::cout << "Saving...\n";
            saveWorldToBinary(chunkManager, player.getPosition(), saveFileName);
            std::cout << "Exiting program.\n";
            return -1; // Exit the game to the menu
        }
        
        handleOptions(window,  tgai, cullTimer, enableCull, viewDistance, distanceTimer, camData);

        auto nextFrame = tgai.nextFrame(window);
        tga::CommandRecorder rec{tgai, cmd};
        
        auto time = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(time - prevTime).count();

        if (frame % 256 == 0) {
            smoothedTime = 0;
        }
        smoothedTime += dt;
        prevTime = time;
        
        // Calculate view direction
        glm::vec3 eyePosition = player.getPosition() + glm::vec3(0.0f, 1.7f, 0.0f); // Approximate eye height
        glm::vec3 viewDirection = glm::normalize(player.getForward()); // Player's forward direction, normalized

        // Get the target block
        auto targetBlock = chunkManager.getTargetBlock(eyePosition, viewDirection);
        glm::vec3 blockWorldPos = glm::vec3(0.f, 1000.f, 0.f);
        
        if (targetBlock.has_value()) {
            // Block found
            glm::vec3 target = *targetBlock;
            blockWorldPos = target;
            //std::cout << "Target Block: (" << blockWorldPos.x << ", " << blockWorldPos.y << ", " << blockWorldPos.z << ")\n";

            // Add logic to highlight or interact with the block
            camData.targetBlockPosition = blockWorldPos;
        } else {
            camData.targetBlockPosition = glm::vec3(-1.0f);
            //std::cout << "No target block in range.\n";
        }


        auto debugTime = std::chrono::steady_clock::now();
        selectBlockType(tgai, window, blockType);
        glm::vec3 cameraPosition = player.getPosition() + glm::vec3(-0.5f,1.7,-0.5f);
        player.update(chunkManager, dt, tgai, window, blockWorldPos, cameraPosition, centerX, centerY, blockType, lookTimer);
        chunkManager.updateMinedBlocks(dt, player.getPosition(), player.collectedBlocks);
        std::vector<MinedBlock> minedBlocks = chunkManager.getMinedBlocks();
        if (minedBatch.vertex.buffer) {
                minedBatch.destroy(tgai);
            }
        if (minedRenderData.geometryPass) {
            minedRenderData.destroy(tgai);
        }
        if (minedBlocks.size() > 0) {
            minedBatch = generateVoxelBatchMined(droppedBlocks, minedBlocks, tgai, player.getPosition(), viewDistance, sunPosition, sunRadius);
            rec.bufferUpload(minedBatch.materialIDs.staging, minedBatch.materialIDs.buffer, minedBatch.materialIDs.count * sizeof(uint32_t));
            rec.barrier(tga::PipelineStage::Transfer, tga::PipelineStage::VertexInput);
            minedRenderData = initBatchRenderData(minedBatch);
        }
        auto currentTime = std::chrono::steady_clock::now();
        float debug = std::chrono::duration<float>(currentTime - debugTime).count();

        //std::cout << "Time for player update and mined blocks: " << debug << "\n";

        bool isPlayerUnderwater = chunkManager.isPositionUnderwater(cameraPosition);
        waterEffectData.isUnderwater = isPlayerUnderwater;

        auto waterEffectUpdateStaging = tgai.createStagingBuffer({
            sizeof(WaterEffect), tga::memoryAccess(waterEffectData)
        });
        rec.bufferUpload(waterEffectUpdateStaging, waterEffectBuffer, sizeof(WaterEffect));
        rec.barrier(tga::PipelineStage::Transfer, tga::PipelineStage::FragmentShader);

        // Update the block type dynamically:
        blockTypeData.selectedBlockType = blockType > 3 ? blockType - 2 : blockType; // Adjust block type if necessary

        // Update the contents of the existing buffer directly
        auto blockTypeUpdateStaging = tgai.createStagingBuffer({sizeof(BlockTypeUniform), tga::memoryAccess(blockTypeData)});
        rec.bufferUpload(blockTypeUpdateStaging, blockTypeBuffer, sizeof(BlockTypeUniform));

        // Barrier to ensure upload is complete
        rec.barrier(tga::PipelineStage::Transfer, tga::PipelineStage::FragmentShader);
        
        // Upload camera data to the GPU
        glm::vec3 forward = glm::normalize(player.getForward());

        camData.view = glm::lookAt(cameraPosition, cameraPosition + forward, glm::vec3(0, 1, 0));
        camData.projection = glm::perspective_vk(glm::radians(camData.fov), 
                                            static_cast<float>(windowWidth) / windowHeight, 
                                            0.1f, 
                                            1000.0f);

        glm::mat4 viewProjectionMatrix = camData.projection * camData.view;
        
        debugTime = std::chrono::steady_clock::now();
        chunkManager.updateChunks(viewProjectionMatrix, cameraPosition, viewDistance);
        currentTime = std::chrono::steady_clock::now();
        debug = std::chrono::duration<float>(currentTime - debugTime).count();
        std::vector<Chunk*> visibleChunks = chunkManager.getVisibleChunks(player.getPosition(), viewDistance,viewProjectionMatrix );
       //std::cout << "Time for chunks update: " << debug << ", For chunks: " << visibleChunks.size() << "\n";

        //if visuals needs updating
        if (chunkManager.shouldUpdate()) {
            debugTime = std::chrono::steady_clock::now();
            // Get visible chunks
        
            //std::cout << "Visible chunks: " << visibleChunks.size() << "\n";
            if (batch.vertex.buffer) {
                batch.destroy(tgai);
            }
            // Regenerate batch only if chunks are updated
            batch = generateVoxelBatch(cubeModel, visibleChunks, tgai, player.getPosition(), viewDistance);
            currentTime = std::chrono::steady_clock::now();
            debug = std::chrono::duration<float>(currentTime - debugTime).count();
            std::cout << "Time for Batch update: " << debug << ", For chunks: " << visibleChunks.size() << "\n";
            debugTime = std::chrono::steady_clock::now();
            //updateVoxelBatch(batch, cubeModel, visibleChunks, tgai);
            rec.bufferUpload(batch.materialIDs.staging, batch.materialIDs.buffer, batch.materialIDs.count * sizeof(uint32_t));
            if (renderData.geometryPass) {
                renderData.destroy(tgai);
            }
            renderData = initBatchRenderData(batch);
            currentTime = std::chrono::steady_clock::now();
            debug = std::chrono::duration<float>(currentTime - debugTime).count();

            std::cout << "Time for renderData update: " << debug << ", For chunks: " << visibleChunks.size() << "\n";
        } 

        // Constants
        float circleRadius = 2500.0f; // Radius of the light circle
        float circleHeight = 1000.0f; // Height of the light above the origin
        float lightSpeed = 0.0001f; // Adjust speed of rotation

        // Calculate the angle for the current frame
        float angle = fmod(frame * lightSpeed, glm::two_pi<float>());

        // Update light direction
        sunlight.direction = glm::normalize(glm::vec3(
            glm::cos(angle) * circleRadius,
            circleHeight, // Constant downward light
            glm::sin(angle) * circleRadius
        ));

        // Update light-space matrix for shadows
        glm::mat4 lightView = glm::lookAt(
            -sunlight.direction * circleHeight, // Position far along the direction
            glm::vec3(0.0f),                    // Center of the world
            glm::vec3(0.0f, 1.0f, 0.0f)         // Up vector
        );
        sunlight.lightSpaceMatrix = lightProjection * lightView;

        // Update light color (constant warm sunlight)
        sunlight.color_intensity = glm::vec4(1.0f, 0.9f, 0.6f, 1.0f);

        // Upload the updated light data to the GPU
        rec.bufferUpload(lightStaging, lightBuffer, lightSize);
        rec.bufferUpload(camStaging, camBuffer, camSize);

        sunPosition = player.getPosition() + sunlight.direction * circleHeight;

        // Reset the drawn instance count
        const uint32_t zero{0};
        rec.inlineBufferUpdate(numDrawnInstancesBuffer, &zero, sizeof(zero));

        // Barrier to ensure uploads are finished before the compute pass
        rec.barrier(tga::PipelineStage::Transfer, tga::PipelineStage::ComputeShader);

        std::ostringstream posStream;

        debugTime = std::chrono::steady_clock::now();

        auto debugTimeRendering = std::chrono::steady_clock::now();

        //
        if(enableCull){
        // Compute pass: frustum culling
        rec.setComputePass(cullPass);
        rec.bindInputSet(renderData.cullInput);
        constexpr uint32_t workGroupSize{128};
        rec.dispatch((batch.drawCommands.count + workGroupSize - 1) / workGroupSize, 1, 1);
        // Barrier to ensure culling is finished before rendering
        rec.barrier(tga::PipelineStage::ComputeShader, tga::PipelineStage::Transfer);
        rec.bufferDownload(numDrawnInstancesBuffer, numDrawnInstancesStaging, sizeof(uint32_t));
        rec.barrier(tga::PipelineStage::ComputeShader, tga::PipelineStage::VertexInput);
        posStream << "Drawn Instances: " << lastNumDrawnElements << ",   " << std::to_string((frame % 256) / smoothedTime) << ",         POS: X = " << player.getPosition().x << ", Y = " << player.getPosition().y << ", Z = " << player.getPosition().z 
        << ",             VIEW: X " << forward.x << " Z = " << forward.z;
        } else {
        posStream << std::to_string((frame % 256) / smoothedTime) << ",         POS: X = " << player.getPosition().x << ", Y = " << player.getPosition().y << ", Z = " << player.getPosition().z 
        << ",             VIEW: X " << forward.x << " Z = " << forward.z;
        }
        auto currentTimeRendering = std::chrono::steady_clock::now();
        debug = std::chrono::duration<float>(currentTimeRendering - debugTimeRendering).count();

        //std::cout << "Time for render cullPass update: " << debug << ", For chunks: " << visibleChunks.size() << "\n";
      
        debugTimeRendering = std::chrono::steady_clock::now();
        // 1. G-Buffer Geometry Pass
        rec.setRenderPass(geometryInitPass, 0)
        .bindInputSet(geometryCamInput);
        rec.bindVertexBuffer(batch.vertex.buffer).bindIndexBuffer(batch.index.buffer);
        rec.bindInputSet(renderData.geometryInput);
        rec.drawIndexedIndirect(batch.drawCommands.buffer, batch.drawCommands.count);

        if(minedBlocks.size() > 0){
            rec.bindVertexBuffer(minedBatch.vertex.buffer).bindIndexBuffer(minedBatch.index.buffer);
            rec.bindInputSet(minedRenderData.geometryInput);
            rec.drawIndexedIndirect(minedBatch.drawCommands.buffer, minedBatch.drawCommands.count);
        }
        currentTimeRendering = std::chrono::steady_clock::now();
        debug = std::chrono::duration<float>(currentTimeRendering - debugTimeRendering).count();

        //std::cout << "Time for render Geometry pass update: " << debug << ", For chunks: " << visibleChunks.size() << "\n";

        debugTimeRendering = std::chrono::steady_clock::now();
        // 2. Lighting Pass
        rec.setRenderPass(lightingPass, 0)
        .bindInputSet(lightingCamLightInput)
        .bindInputSet(gBufferInput)
        .draw(3, 0);
        currentTimeRendering = std::chrono::steady_clock::now();
        debug = std::chrono::duration<float>(currentTimeRendering - debugTimeRendering).count();

        //std::cout << "Time for render Light pass update: " << debug << ", For chunks: " << visibleChunks.size() << "\n";

        debugTimeRendering = std::chrono::steady_clock::now();
        // 3. Post-Processing Pass
        rec.setRenderPass(postProcessingPass, nextFrame)
        .bindInputSet(postProcessingInput)  // HDR texture input
        .bindInputSet(blockTextureSet)      // Block textures input
        .bindInputSet(blockTypeInputSet)    // Block type input
        .bindInputSet(guiInputSet) 
        .bindInputSet(waterEffectInputSet)
        .draw(3, 0);
        currentTimeRendering = std::chrono::steady_clock::now();
        debug = std::chrono::duration<float>(currentTimeRendering - debugTimeRendering).count();

        //std::cout << "Time for render Post pass update: " << debug << ", For chunks: " << visibleChunks.size() << "\n";
        
        tgai.setWindowTitle(window, posStream.str());  // Display in the window title (or you can render it on-screen)

        cmd = rec.endRecording();
        tgai.execute(cmd);
        tgai.present(window, nextFrame);

        currentTime = std::chrono::steady_clock::now();
        debug = std::chrono::duration<float>(currentTime - debugTime).count();

        //std::cout << "Time for all rendering: " << debug << "\n";

        //free existing staging buffers
        tgai.free(blockTypeUpdateStaging);
        tgai.free(waterEffectUpdateStaging);
    }
    return 0;
}
