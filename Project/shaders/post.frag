#version 450

layout(location = 0) in FragData {
    vec2 uv;
} fragData;

layout(set = 0, binding = 0) uniform sampler2D hdr;
layout(set = 1, binding = 0) uniform BlockType {
    int selectedBlockType;
} blockType;
layout(set = 2, binding = 0) uniform sampler2D guiTexture;
layout(set = 3, binding = 0) uniform sampler2D blockTextures[9];

layout(set = 4, binding = 0) uniform WaterEffect {
    bool isUnderwater;
} waterEffect;

layout(location = 0) out vec4 color;

void main() {
    vec3 col = texture(hdr, fragData.uv).rgb;

    // Tone mapping using extended Reinhard method
    const float luminance = dot(vec3(1.64, 1.27, 0.99), vec3(0.2126, 0.7152, 0.0722));
    const float maxWhite = luminance * 128 * 2;
    col = (col * (vec3(1) + (col / vec3(maxWhite * maxWhite)))) / (col + vec3(1));

    // Apply blueish tint if underwater
    if (waterEffect.isUnderwater) {
        col = mix(col, vec3(0.2, 0.3, 0.8), 0.4); // Mix original color with blue tint
    }

    // Crosshair
    vec2 center = vec2(0.5, 0.5);
    vec2 crosshairSize = vec2(0.005, 0.002);

    if ((abs(fragData.uv.x - center.x) < crosshairSize.x && abs(fragData.uv.y - center.y) < crosshairSize.y) || 
        (abs(fragData.uv.y - center.y) < crosshairSize.x && abs(fragData.uv.x - center.x) < crosshairSize.y)) {
        col = vec3(1.0, 1.0, 1.0);
    }

    // Render hotbar
    float barWidth = 0.58; // 60% of screen width
    float barHeight = 0.09; // 8% of screen height
    vec2 barCenter = vec2(0.5, 0.92); // Bottom-center of the screen
    float slotWidth = barWidth / 9.0; // Divide bar into 9 slots
    float scaleFactor = 0.7; // Scale down block textures (80% of slot size)

    vec2 barStart = barCenter - vec2(barWidth / 2.0, barHeight / 2.0);
    vec2 barEnd = barCenter + vec2(barWidth / 2.0, barHeight / 2.0);

    // Check if the fragment lies within the hotbar area
    if (fragData.uv.x >= barStart.x && fragData.uv.x <= barEnd.x &&
        fragData.uv.y >= barStart.y && fragData.uv.y <= barEnd.y) {
        // Calculate slot index
        int slot = int((fragData.uv.x - barStart.x) / slotWidth);

        // Map to hotbar UV coordinates
        vec2 hotbarUV = vec2(
            (fragData.uv.x - barStart.x) / barWidth, // Horizontal UV for hotbar texture
            (fragData.uv.y - barStart.y) / barHeight // Vertical UV for hotbar texture
        );

        // Sample the GUI hotbar texture
        vec4 guiColor = texture(guiTexture, hotbarUV);

        // Render the block texture inside the slot
        vec2 slotStart = barStart + vec2(slot * slotWidth, 0.0);
        vec2 slotEnd = slotStart + vec2(slotWidth, barHeight);

        col = guiColor.rgb;
        if (fragData.uv.x >= slotStart.x && fragData.uv.x <= slotEnd.x &&
            fragData.uv.y >= slotStart.y && fragData.uv.y <= slotEnd.y) {
            vec2 slotCenter = (slotStart + slotEnd) * 0.5; // Center of the slot
            vec2 blockUV = vec2(
                (fragData.uv.x - slotCenter.x) / (slotWidth * scaleFactor) + 0.5,
                (fragData.uv.y - slotCenter.y) / (barHeight * scaleFactor) + 0.5
            );
            vec4 blockColor = texture(blockTextures[slot], blockUV);

            // Apply block color over the hotbar
            if (blockColor.a > 0.0) {
                col = blockColor.rgb;
            }
        } 

        if (slot == blockType.selectedBlockType - 1) {
            // Highlight the edges with a white border
            float borderThickness = 0.004; // Adjust border thickness as needed
            bool isBorder = (fragData.uv.x - slotStart.x < borderThickness || slotEnd.x - fragData.uv.x < borderThickness ||
                            fragData.uv.y - slotStart.y < borderThickness || slotEnd.y - fragData.uv.y < borderThickness);

            if (isBorder) {
                col = vec3(1.0, 1.0, 1.0); // White border
            }
        }
    }
    color = vec4(col, 1.0);
}
