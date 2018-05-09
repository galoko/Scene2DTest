cbuffer VertexPersistent : register(b0)
{
    float4x4 FinalMatrix;
};

cbuffer VertexPerDraw : register(b1)
{
    float4x4 WorldMatrix;
}

cbuffer PixelPersistent : register(b0)
{
    float4x4 Placeholder1;
}

cbuffer PixelPerDraw : register(b1)
{
    float4x4 Placeholder2;
}

SamplerState SmoothSampler : register(s0);
SamplerState PointSampler  : register(s1);

Texture2D MainTexture : register(t0);

//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float3 Pos : POSITION;
    float2 Tex : TEXCOORD;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    
    output.Pos = mul(mul(float4(input.Pos, 1.0f), WorldMatrix), FinalMatrix);      
    output.Tex = input.Tex;
    
    return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

float4 PS(PS_INPUT input) : SV_Target
{
    float4 Color = MainTexture.Sample(PointSampler, input.Tex);

    return Color;
}