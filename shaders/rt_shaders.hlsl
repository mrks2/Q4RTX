// Q4RTX ray tracing shaders - one-bounce GI + AO

RWTexture2D<float4> OutputUAV : register(u0);
RaytracingAccelerationStructure SceneTLAS : register(t0);

struct Vertex {
    float3 position;
    float3 normal;
};
StructuredBuffer<Vertex> VertexBuffer : register(t1);
Buffer<uint> IndexBuffer : register(t2);

struct Light {
    float3 pos;
    float  radius;
    float3 color;
    float  intensity;
};
StructuredBuffer<Light> LightBuffer : register(t3);

cbuffer CameraConstants : register(b0) {
    float4 g_CameraPos;
    float4 g_CameraRight;
    float4 g_CameraUp;
    float4 g_CameraForward;
    float  g_FovY;
    float  g_Aspect;
    uint   g_LightCount;
};

cbuffer SkyboxConstants : register(b1) {
    float4 g_ZenithColor;
    float4 g_HorizonColor;
    float4 g_GroundColor;
    float4 g_SunDir;
    float4 g_SunColor;
    float  g_SunSize;
    float  g_SunFalloff;
    float  g_AtmosphereDensity;
};

struct RayPayload {
    float3 color;
    float  hit;
    uint   depth; // 0 = primary, 1 = GI bounce
};

uint WangHash(uint seed) {
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

float HashToFloat(uint h) {
    return float(h & 0xFFFFFF) / float(0xFFFFFF);
}

void BuildBasis(float3 N, out float3 T, out float3 B) {
    float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    T = normalize(cross(up, N));
    B = cross(N, T);
}

float3 CosineHemisphere(float u1, float u2) {
    float r = sqrt(u1);
    float phi = 6.28318530718 * u2;
    return float3(r * cos(phi), r * sin(phi), sqrt(max(0.0, 1.0 - u1)));
}

// Direct lighting, used by both primary and bounce hits
float3 ComputeDirectLighting(float3 hitPos, float3 normal, float3 baseColor, bool castShadows) {
    float3 color = float3(0, 0, 0);

    uint numLights = min(g_LightCount, 128u);
    uint maxShadowLights = castShadows ? min(numLights, 8u) : 0u;

    for (uint i = 0; i < numLights; i++) {
        Light light = LightBuffer[i];
        float3 toLight = light.pos - hitPos;
        float dist = length(toLight);
        float3 lightDir = toLight / max(dist, 0.001);

        float NdotL = dot(normal, lightDir);
        if (NdotL <= 0.0) continue;

        float atten = saturate(1.0 - dist / max(light.radius, 1.0));
        atten *= atten;
        if (atten <= 0.001) continue;

        float shadow = 1.0;
        if (i < maxShadowLights) {
            RayDesc shadowRay;
            shadowRay.Origin    = hitPos + normal * 0.5;
            shadowRay.Direction = lightDir;
            shadowRay.TMin      = 0.1;
            shadowRay.TMax      = dist - 0.5;

            RayPayload shadowPayload;
            shadowPayload.color = float3(0, 0, 0);
            shadowPayload.hit   = 1.0;
            shadowPayload.depth = 99;

            TraceRay(SceneTLAS,
                     RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
                     0xFF, 0, 0, 0, shadowRay, shadowPayload);

            shadow = 1.0 - shadowPayload.hit;
        }

        color += baseColor * light.color * (light.intensity * 0.15) * NdotL * atten * shadow;
    }

    // Fallback directional light when no game lights are available
    if (numLights == 0) {
        color += baseColor * 0.12; // ambient base

        float3 keyDir = normalize(float3(0.5, 0.3, 0.8));
        float keyNdotL = saturate(dot(normal, keyDir));

        float shadowFactor = 1.0;
        if (castShadows && keyNdotL > 0.001) {
            RayDesc shadowRay;
            shadowRay.Origin    = hitPos + normal * 0.5;
            shadowRay.Direction = keyDir;
            shadowRay.TMin      = 0.1;
            shadowRay.TMax      = 10000.0;

            RayPayload shadowPayload;
            shadowPayload.color = float3(0, 0, 0);
            shadowPayload.hit   = 1.0;
            shadowPayload.depth = 99;

            TraceRay(SceneTLAS,
                     RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
                     0xFF, 0, 0, 0, shadowRay, shadowPayload);

            shadowFactor = 1.0 - shadowPayload.hit * 0.85;
        }

        color += baseColor * 0.50 * keyNdotL * shadowFactor * float3(1.0, 0.95, 0.85);

        // Fill from opposite side so nothing is totally dark
        float3 fillDir = normalize(float3(-0.5, -0.3, -0.4));
        color += baseColor * 0.25 * saturate(dot(normal, fillDir)) * float3(0.7, 0.8, 1.0);

        // Top-down fill for floors
        float3 topDir = normalize(float3(0.0, 0.0, 1.0));
        color += baseColor * 0.10 * saturate(dot(normal, topDir)) * float3(0.9, 0.9, 1.0);
    }

    return color;
}

[shader("raygeneration")]
void RayGen() {
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;

    float2 uv = (float2(launchIndex) + 0.5) / float2(launchDim);
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;

    float halfH = tan(g_FovY * 0.5);
    float halfW = halfH * g_Aspect;

    float3 origin = g_CameraPos.xyz;
    float3 dir = normalize(g_CameraForward.xyz
                           + ndc.x * halfW * g_CameraRight.xyz
                           + ndc.y * halfH * g_CameraUp.xyz);

    RayDesc ray;
    ray.Origin    = origin;
    ray.Direction = dir;
    ray.TMin      = 1.0;
    ray.TMax      = 200000.0;

    RayPayload payload;
    payload.color = float3(0, 0, 0);
    payload.hit   = 0;
    payload.depth = 0;

    TraceRay(SceneTLAS, RAY_FLAG_FORCE_OPAQUE,
             0xFF, 0, 0, 0, ray, payload);

    OutputUAV[launchIndex] = float4(payload.color, 1.0);
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr) {
    float3 rayDir = WorldRayDirection();
    float t = RayTCurrent();

    uint primIdx = PrimitiveIndex();
    uint i0 = IndexBuffer[primIdx * 3 + 0];
    uint i1 = IndexBuffer[primIdx * 3 + 1];
    uint i2 = IndexBuffer[primIdx * 3 + 2];

    float3 p0 = VertexBuffer[i0].position;
    float3 p1 = VertexBuffer[i1].position;
    float3 p2 = VertexBuffer[i2].position;

    // Interpolate vertex normals for smoother shading
    float3 n0 = VertexBuffer[i0].normal;
    float3 n1 = VertexBuffer[i1].normal;
    float3 n2 = VertexBuffer[i2].normal;
    float3 baryWeights = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y,
                                 attr.barycentrics.x, attr.barycentrics.y);
    float3 interpNormal = normalize(n0 * baryWeights.x + n1 * baryWeights.y + n2 * baryWeights.z);

    // Fall back to geometric normal if vertex normals are degenerate
    float3 edge1 = p1 - p0;
    float3 edge2 = p2 - p0;
    float3 geoNormal = normalize(cross(edge1, edge2));
    float3 normal = length(interpNormal) > 0.5 ? interpNormal : geoNormal;

    if (dot(normal, rayDir) > 0.0)
        normal = -normal;

    float3 hitPos = WorldRayOrigin() + t * rayDir;

    float3 baseColor = float3(0.85, 0.82, 0.75);

    // GI bounce: direct lighting only, no further bounces
    if (payload.depth >= 1) {
        float hemi = normal.z * 0.5 + 0.5;
        float3 ambient = baseColor * lerp(float3(0.08, 0.07, 0.06), float3(0.15, 0.15, 0.18), hemi);
        float3 direct = ComputeDirectLighting(hitPos, normal, baseColor, true);
        payload.color = ambient + direct;
        payload.hit = 1.0;
        return;
    }

    // Primary ray: full shading with GI
    float3 indirect = float3(0, 0, 0);
    float ao = 0.0;
    {
        float3 T, B;
        BuildBasis(normal, T, B);

        uint2 px = DispatchRaysIndex().xy;
        uint seed = WangHash(px.x + px.y * 1920 + primIdx * 7919);

        const uint GI_SAMPLES = 4;
        const float GI_RADIUS = 512.0;
        float3 giOrigin = hitPos + normal * 1.5;

        for (uint s = 0; s < GI_SAMPLES; s++) {
            uint h0 = WangHash(seed + s * 2);
            uint h1 = WangHash(seed + s * 2 + 1);
            float u1 = HashToFloat(h0);
            float u2 = HashToFloat(h1);

            float3 localDir = CosineHemisphere(u1, u2);
            float3 worldDir = localDir.x * T + localDir.y * B + localDir.z * normal;

            RayDesc giRay;
            giRay.Origin    = giOrigin;
            giRay.Direction = worldDir;
            giRay.TMin      = 2.0;
            giRay.TMax      = GI_RADIUS;

            RayPayload giPayload;
            giPayload.color = float3(0, 0, 0);
            giPayload.hit   = 0.0;
            giPayload.depth = 1;

            TraceRay(SceneTLAS, RAY_FLAG_FORCE_OPAQUE,
                     0xFF, 0, 0, 0, giRay, giPayload);

            indirect += giPayload.color;
            ao += giPayload.hit;
        }
        indirect /= float(GI_SAMPLES);
        ao = 1.0 - (ao / float(GI_SAMPLES)) * 0.35;
    }

    // Hemisphere ambient (modulated by AO)
    float hemi = normal.z * 0.5 + 0.5;
    float3 color = baseColor * lerp(float3(0.08, 0.07, 0.06), float3(0.15, 0.15, 0.18), hemi) * ao;

    // Indirect illumination (color bleeding from GI bounce)
    color += baseColor * indirect * 0.35;

    color += ComputeDirectLighting(hitPos, normal, baseColor, true);

    // Distance fog
    float fog = saturate(1.0 - t / 10000.0);
    color *= fog;

    // Reinhard tonemap
    color = color / (1.0 + color);

    payload.color = color;
    payload.hit = 1.0;
}

// Miss - procedural skybox

float3 SampleSky(float3 dir) {
    float up = dir.z; // z-up

    float3 sky;
    if (up < 0.0) {
        float t = saturate(-up * g_AtmosphereDensity);
        sky = lerp(g_HorizonColor.rgb, g_GroundColor.rgb, t);
    } else {
        float t = saturate(pow(up, 1.0 / max(g_AtmosphereDensity, 0.1)));
        sky = lerp(g_HorizonColor.rgb, g_ZenithColor.rgb, t);
    }

    // Sun disk + glow
    float sunDot = dot(dir, g_SunDir.xyz);
    if (sunDot > 0.0) {
        if (sunDot > g_SunSize)
            sky += g_SunColor.rgb;

        float glow = saturate(sunDot);
        glow = pow(glow, g_SunFalloff);
        sky += g_SunColor.rgb * glow * 0.3;
    }

    return sky;
}

[shader("miss")]
void Miss(inout RayPayload payload) {
    float3 dir = normalize(WorldRayDirection());
    payload.color = SampleSky(dir);
    payload.hit = 0.0;
}
