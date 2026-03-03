#version 450

// ── Uniform Buffer (per-frame) ──
layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    vec4 lightDir;
    vec4 cameraPos;
    float time;
} ubo;

// ── Push Constants (per-object — DRO state) ──
layout(push_constant) uniform Push {
    mat4 model;
    vec4 droColor;    // RGB = LOD color, A = pulse intensity
    float droBase;    // Current base (4-64)
    float lodTier;    // 0-4
} push;

// ── Vertex Inputs ──
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

// ── Outputs to Fragment Shader ──
layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragWorldPos;
layout(location = 2) out vec4 fragDROColor;
layout(location = 3) out float fragBase;
layout(location = 4) out float fragLOD;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;
    
    fragNormal = mat3(push.model) * inNormal;
    fragWorldPos = worldPos.xyz;
    fragDROColor = push.droColor;
    fragBase = push.droBase;
    fragLOD = push.lodTier;
}
