#include "global.hpp"

int main() {

    std::cout << "Minecraft Clone\n";
    tga::Interface tgai;

    /*
    //////// GENERATE TEXTURES
    createFinalTextureLayout("models/water_flow.png","models/water_flow.png", "models/water_flow.png" , "models/water_flow.png");
    return 0;
    */

    auto windowWidth = tgai.screenResolution().first;
    auto windowHeight = tgai.screenResolution().second;
    tga::Window window = tgai.createWindow({windowWidth/6*5, windowHeight/6*5});
    int centerX = static_cast<int>(windowWidth/2);
    int centerY = static_cast<int>(windowHeight/2);
    
    tga::Obj cubeModel = tga::loadObj("models/Grass_Block.obj");
    std::vector<tga::Vertex> cubeVertices = cubeModel.vertexBuffer;
    std::vector<uint32_t> cubeIndices = cubeModel.indexBuffer;
    
    float scaleFactor = 0.5f; // 1.0f / 2.0f
    for (auto& vertex : cubeModel.vertexBuffer) {
        vertex.position *= scaleFactor;
    }

    glm::vec3 minBounds(FLT_MAX), maxBounds(-FLT_MAX);
    for (const auto& vertex : cubeModel.vertexBuffer) {
        minBounds = glm::min(minBounds, vertex.position);
        maxBounds = glm::max(maxBounds, vertex.position);
    }

    glm::vec3 dimensions = maxBounds - minBounds;
    std::cout << "Object dimensions: " << glm::to_string(dimensions) << "\n";

    // Generate a random seed using the current time
    ChunkManager chunkManager;
    chunkManager.setUpdated();
    glm::vec3 playerPosition = {0.0f, 0.0f, 0.0f}; // Start above the highest block

    // Generate the world (e.g., 20x20 chunks)
    int worldWidth = 25;
    int worldDepth = 25;

    size_t maxVoxels = worldWidth * worldDepth * CHUNK_SIZE * CHUNK_SIZE_Y * CHUNK_SIZE;
    size_t bufferSize = sizeof(Voxel) * maxVoxels;

    for (auto& vertex : cubeModel.vertexBuffer) {
        std::cout << "UV: " << vertex.uv.x << ", " << vertex.uv.y << "\n";
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Create Camera Buffer
    struct Camera {
        glm::mat4 view;
        glm::mat4 toWorld;
        glm::mat4 projection;
        glm::vec3 targetBlockPosition; // Add this field
        float fov;
    };
    auto [camData, camStaging, camSize] = stagingBufferOfType<Camera>(tgai);
    camData.projection = glm::perspective_vk(glm::radians(90.f), static_cast<float>(windowWidth) / windowHeight, 0.1f, 1000.f);
    camData.fov = 90;
    auto camBuffer = tgai.createBuffer({tga::BufferUsage::uniform, camSize, camStaging});


    // Struct needs proper alignment to conform to uniform buffer alignment requirement
    struct alignas(16) Light {
        glm::vec4 color_intensity; // (R, G, B, Intensity)
        glm::vec3 position;        // For point lights
        glm::vec3 direction;       // For directional light
        glm::mat4 lightSpaceMatrix;
    };

    // Create staging buffer for lights
    auto [lightData, lightStaging, lightSize] = stagingBufferOfType<std::array<Light, 1>>(tgai);

    // Initialize sunlight (central point in the sky)
    auto& sunlight = lightData[0];
    sunlight.color_intensity = glm::vec4(1.0f, 0.9f, 0.8f, 1.0f); // Warm sunlight
    sunlight.position = glm::vec3(0.0f, 70.0f, 0.0f); // Central position in the sky
    sunlight.direction = glm::normalize(glm::vec3(0.0f, -1.0f, 0.0f)); // Downward direction
    glm::mat4 lightProjection = glm::ortho(-50.f, 50.f, -50.f, 50.f, -100.f, 100.f);
    glm::mat4 lightView = glm::lookAt(sunlight.position, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)); 
    sunlight.lightSpaceMatrix = lightProjection * lightView;
    // Initialize other lights (optional)
    for (int i = 1; i < 1; ++i) {
        lightData[i] = {};
    }
    // Create buffer for lights
    auto lightBuffer = tgai.createBuffer({tga::BufferUsage::uniform, lightSize, lightStaging});

    //Initialize the texture (in global.hpp)
    initializeTextures(tgai);

    // Render Targets for 1. Pass
    auto albedoTex = tgai.createTexture({windowWidth, windowHeight, tga::Format::r8g8b8a8_unorm, tga::SamplerMode::linear});
    auto normalTex = tgai.createTexture({windowWidth, windowHeight, tga::Format::r16g16b16a16_sfloat, tga::SamplerMode::linear});
    auto positionTex =
        tgai.createTexture({windowWidth, windowHeight, tga::Format::r16g16b16a16_sfloat, tga::SamplerMode::linear});

    /*Note: use batched version here*/
    auto geometryVS =
        tga::loadShader("shaders/vs.spv", tga::ShaderType::vertex, tgai);
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
                                            tga::BindingType::storageBuffer, tga::BindingType::storageBuffer, {tga::BindingType::sampler, static_cast<uint32_t>(allTextures.size())}}}));

    // Render Target for 2. Pass
    auto hdrOutputTex =
        tgai.createTexture({windowWidth, windowHeight, tga::Format::r16g16b16a16_sfloat, tga::SamplerMode::linear});
    auto lightingVS = tga::loadShader("shaders/fullscreen_vert.spv", tga::ShaderType::vertex, tgai);
    auto lightingFS = tga::loadShader("shaders/light_frag.spv", tga::ShaderType::fragment, tgai);
    auto lightingPass = tgai.createRenderPass(
        tga::RenderPassInfo{lightingVS, lightingFS, hdrOutputTex}
            .setClearOperations(tga::ClearOperation::all)
            .setPerPixelOperations(tga::PerPixelOperations{}.setDepthCompareOp(tga::CompareOperation::lessEqual))
            .setInputLayout(
                {tga::SetLayout{tga::BindingType::uniformBuffer, tga::BindingType::uniformBuffer},
                 tga::SetLayout{tga::BindingType::sampler, tga::BindingType::sampler, tga::BindingType::sampler}}));
                 
    // 3rd Render Pass renders to window. Since it's also a fullscreen pass, the same vertex shader from before is used
    auto postProcessingFS = tga::loadShader("shaders/post_frag.spv", tga::ShaderType::fragment, tgai);
    auto postProcessingPass = tgai.createRenderPass(tga::RenderPassInfo{lightingVS, postProcessingFS, window}
                                                        .setClearOperations(tga::ClearOperation::all)
                                                        .setInputLayout({tga::SetLayout{tga::BindingType::sampler}}));
                                                        
    auto geometryCamInput = tgai.createInputSet({geometryInitPass, {tga::Binding{camBuffer, 0}}, 0});
    auto lightingCamLightInput =
        tgai.createInputSet({lightingPass, {tga::Binding{camBuffer, 0}, tga::Binding{lightBuffer, 1}}, 0});
    auto gBufferInput = tgai.createInputSet(
        {lightingPass, {tga::Binding{albedoTex, 0}, tga::Binding{normalTex, 1}, tga::Binding{positionTex, 2}}, 1});
    auto postProcessingInput = tgai.createInputSet({postProcessingPass, {tga::Binding{hdrOutputTex, 0}}, 0});

    // A Compute Pass to perform the culling
    auto cullPass = tgai.createComputePass(
        {tga::loadShader("../../shaders/glslSolution/frustum_culling_comp.spv", tga::ShaderType::compute,tgai),
         tga::InputLayout{tga::SetLayout{tga::BindingType::uniformBuffer, tga::BindingType::storageBuffer,
                                         tga::BindingType::storageBuffer, tga::BindingType::storageBuffer,
                                         tga::BindingType::storageBuffer,
                                         tga::BindingType::uniformBuffer, tga::BindingType::storageBuffer}}

        });

    // The communication path to report how many instances where drawn in total
    //auto numDrawnInstancesStaging = tgai.createStagingBuffer({sizeof(uint32_t)});
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
                .setPerPixelOperations(tga::PerPixelOperations{}.setDepthCompareOp(tga::CompareOperation::lessEqual))
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
    tga::CommandBuffer cmd;
    BatchRenderData renderData;
    lockAndHideCursor();
    int mouseTimer = 20;

    std::string currentSaveFileName;
    std::string saveFileName = "tryingWaterFlow";
    int viewDistance = 1; // Render chunks within "x" chunks of the player
    int distanceTimer = 0;


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
    moveCursorToCenter(centerX, centerY);
    int lookTimer = 0;

    float lightRadius = 1000.0f; // Radius of the circular path
    float lightSpeed = 0.001f; 
    float cycleTime = glm::two_pi<float>(); // Full cycle (2π radians)
    int currentCycle = 0; 

    //////////////////////////////// RAY TRACING

    //////////////////////////////////////////////////////////////


    // Render loop
    for (uint32_t frame{0}; !tgai.windowShouldClose(window); ++frame) {

        if (tgai.keyDown(window, tga::Key::Escape)) {
            std::cout << "Saving...\n";
            saveWorldToBinary(chunkManager, player.getPosition(), saveFileName);
            std::cout << "Exiting program.\n";
            return 0; // Exit the program
        }

        if (tgai.keyDown(window, tga::Key::P) && cullTimer <= 0) {
            enableCull = !enableCull;
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
        
        selectBlockType(tgai, window, blockType);
        player.update(chunkManager, dt, tgai, window, blockWorldPos, centerX, centerY, blockType, lookTimer);

        // Upload camera data to the GPU
        glm::vec3 forward = glm::normalize(player.getForward());

        // Increase or decrease FOV with keys
        if (tgai.keyDown(window, tga::Key::O)) {
            camData.fov = glm::clamp(camData.fov + 1.0f, 30.0f, 120.0f); // Clamp FOV between 30° and 120°
        }
        if (tgai.keyDown(window, tga::Key::L)) {
            camData.fov = glm::clamp(camData.fov - 1.0f, 30.0f, 120.0f);
        }

        camData.view = glm::lookAt(player.getPosition() + glm::vec3(-0.5f, 1.7f, -0.5f), player.getPosition() + glm::vec3(-0.5f, 1.7f, -0.5f) + forward, glm::vec3(0, 1, 0));
        camData.projection = glm::perspective_vk(glm::radians(camData.fov), 
                                            static_cast<float>(windowWidth) / windowHeight, 
                                            0.1f, 
                                            1000.0f);

        glm::mat4 viewProjectionMatrix = camData.projection * camData.view;
        glm::vec3 cameraPosition = player.getPosition() + glm::vec3(0,1.8,0);

        chunkManager.updateChunks(viewProjectionMatrix, cameraPosition, viewDistance);

        //if visuals needs updating
        if (chunkManager.shouldUpdate()) {
            // Get visible chunks
            std::vector<Chunk*> visibleChunks = chunkManager.getVisibleChunks(player.getPosition(), viewDistance,viewProjectionMatrix );

            if (batch.vertex.buffer) {
                batch.destroy(tgai);
            }
            // Regenerate batch only if chunks are updated
            batch = generateVoxelBatch(cubeModel, visibleChunks, tgai, player.getPosition(), viewDistance);
            rec.bufferUpload(batch.materialIDs.staging, batch.materialIDs.buffer, batch.materialIDs.count * sizeof(uint32_t));
            if (renderData.geometryPass) {
                renderData.destroy(tgai);
            }
            renderData = initBatchRenderData(batch);
        }

        // Calculate the angle for the current frame
        float angle = fmod(frame * lightSpeed, 2 * cycleTime); // Reset angle every full cycle (0 to 2π)

        // Determine if it's the sun or moon
        currentCycle = (angle < cycleTime) ? 0 : 1;

        // Adjust angle for the current half-cycle
        float adjustedAngle = (currentCycle == 0) ? angle : angle - cycleTime;

        // Update light color (sun: yellow, moon: white)
        sunlight.color_intensity = (currentCycle == 0)
            ? glm::vec4(1.0f, 0.9f, 0.6f, 1.0f) // Sun (warm yellow)
            : glm::vec4(0.8f, 0.8f, 1.0f, 1.0f); // Moon (cool white)

        // Calculate light position
        float x = lightRadius * glm::cos(adjustedAngle);      // East-to-west movement
        float y = lightRadius * glm::sin(adjustedAngle);      // Up and down movement
        float z = lightRadius * 0.5f * glm::sin(adjustedAngle); // Tilt toward the south

        sunlight.position = glm::vec3(x, y, z);

        // Update light direction (pointing toward the origin or another focus point)
        sunlight.direction = glm::normalize(glm::vec3(0.0f, 0.0f, 0.0f) - sunlight.position);
        float normalizedTime = glm::sin(cycleTime) * 0.5f + 0.5f; // Map angle to [0, 1]
        // Calculate background color
        
        // Update the light space matrix
        glm::mat4 lightProjection = glm::ortho(-50.f, 50.f, -50.f, 50.f, -100.f, 100.f);
        glm::mat4 lightView = glm::lookAt(
            sunlight.position,             // Light's position
            glm::vec3(0.0f),               // Target position (e.g., the center of the world)
            glm::vec3(0.0f, 1.0f, 0.0f)    // Up vector
        );
        sunlight.lightSpaceMatrix = lightProjection * lightView;

        // Upload the updated light data to the GPU
        rec.bufferUpload(lightStaging, lightBuffer, lightSize);
        rec.bufferUpload(camStaging, camBuffer, camSize);

        // Reset the drawn instance count
        const uint32_t zero{0};
        rec.inlineBufferUpdate(numDrawnInstancesBuffer, &zero, sizeof(zero));

        // Barrier to ensure uploads are finished before the compute pass
        rec.barrier(tga::PipelineStage::Transfer, tga::PipelineStage::ComputeShader);

        //
        if(enableCull){
        // Compute pass: frustum culling
        rec.setComputePass(cullPass);
        rec.bindInputSet(renderData.cullInput);
        constexpr uint32_t workGroupSize{256};
        rec.dispatch((batch.drawCommands.count + workGroupSize - 1) / workGroupSize, 1, 1);
        // Barrier to ensure culling is finished before rendering
        rec.barrier(tga::PipelineStage::ComputeShader, tga::PipelineStage::VertexInput);
        }

        // 1. G-Buffer Geometry Pass
        rec.setRenderPass(geometryInitPass, 0).bindInputSet(geometryCamInput);
        rec.bindVertexBuffer(batch.vertex.buffer).bindIndexBuffer(batch.index.buffer);
        rec.bindInputSet(renderData.geometryInput);
        rec.drawIndexedIndirect(batch.drawCommands.buffer, batch.drawCommands.count);

        // 2. Lighting Pass
        rec.setRenderPass(lightingPass, 0)
        .bindInputSet(lightingCamLightInput)
        .bindInputSet(gBufferInput)
        .draw(3, 0);

        // 3. Post-Processing Pass
        rec.setRenderPass(postProcessingPass, nextFrame)
        .bindInputSet(postProcessingInput)
        .draw(3, 0);
        
        std::ostringstream posStream;
        posStream << std::to_string((frame % 256) / smoothedTime) << ",         POS: X = " << player.getPosition().x << ", Y = " << player.getPosition().y << ", Z = " << player.getPosition().z 
        << ",             VIEW: X " << forward.x << " Z = " << forward.z;
        tgai.setWindowTitle(window, posStream.str());  // Display in the window title (or you can render it on-screen)

        cmd = rec.endRecording();
        tgai.execute(cmd);
        tgai.present(window, nextFrame);
    }
    unlockAndShowCursor();
    return 0;
}
