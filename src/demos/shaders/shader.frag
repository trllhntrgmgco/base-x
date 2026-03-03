#version 450

// ── Uniform Buffer (per-frame) ──
layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    vec4 lightDir;
    vec4 cameraPos;
    float time;
} ubo;

// ── Inputs from Vertex Shader ──
layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragWorldPos;
layout(location = 2) in vec4 fragDROColor;
layout(location = 3) in float fragBase;
layout(location = 4) in float fragLOD;

// ── Output ──
layout(location = 0) out vec4 outColor;

void main() {
    // ── Lighting ──
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(ubo.lightDir.xyz);
    float NdotL = max(dot(N, L), 0.0);
    
    // DRO base normalized (0 = Base-4, 1 = Base-64)
    float baseNorm = (fragBase - 4.0) / 60.0;
    
    // ── Base Color from DRO ──
    vec3 baseColor = fragDROColor.rgb;
    
    // ── Ambient (boosted at low LOD to keep visibility) ──
    vec3 ambient = baseColor * mix(0.25, 0.15, baseNorm);
    
    // ── Diffuse ──
    vec3 diffuse = baseColor * NdotL * 0.65;
    
    // ── Specular (reduced at low Base — less precision = less shininess) ──
    vec3 V = normalize(ubo.cameraPos.xyz - fragWorldPos);
    vec3 H = normalize(L + V);
    float shininess = mix(8.0, 64.0, baseNorm);  // Sharper highlight at high base
    float spec = pow(max(dot(N, H), 0.0), shininess) * baseNorm;
    vec3 specular = vec3(1.0) * spec * 0.4;
    
    // ── Edge Glow (pulsing with DRO state) ──
    float edge = 1.0 - abs(dot(N, V));
    edge = pow(edge, 3.0);
    float pulse = fragDROColor.a;
    vec3 glow = baseColor * edge * pulse * 0.6;
    
    // ── Combine ──
    vec3 color = ambient + diffuse + specular + glow;
    
    // ── Quantization Effect at Low Bases ──
    // Objects at low precision visually show "stepping" in their color
    if (fragBase < 20.0) {
        float steps = fragBase * 2.0;
        color = floor(color * steps + 0.5) / steps;
    }
    
    // ── Wireframe hint at lowest LOD ──
    // At LOD-4, add subtle edge darkening to suggest low-poly
    if (fragLOD >= 3.5) {
        float wireEdge = pow(edge, 1.5);
        color = mix(color, vec3(0.0), wireEdge * 0.3);
    }
    
    // ── Slight desaturation at distance (atmospheric) ──
    float dist = length(ubo.cameraPos.xyz - fragWorldPos);
    float fog = clamp(dist / 200.0, 0.0, 0.7);
    vec3 fogColor = vec3(0.02, 0.02, 0.05);
    color = mix(color, fogColor, fog);
    
    outColor = vec4(color, 1.0);
}
