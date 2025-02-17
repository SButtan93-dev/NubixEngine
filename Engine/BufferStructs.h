#pragma once

#include "Lights.h"
#include <DirectXMath.h>

// Must match vertex shader definition!
struct VertexShaderExternalData
{
	DirectX::XMFLOAT4X4 world;
	DirectX::XMFLOAT4X4 worldInverseTranspose;
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 projection;
};

// Must match pixel shader definition!
struct PixelShaderExternalData
{
	DirectX::XMFLOAT2 uvScale;
	DirectX::XMFLOAT2 uvOffset;
	DirectX::XMFLOAT3 cameraPosition;
	int lightCount;
	Light lights[MAX_LIGHTS];
};

struct GBufferPixelShaderExternalData
{
	DirectX::XMFLOAT4 color;
};

struct PerFrameData 
{
	DirectX::XMFLOAT4X4 InvViewProj;
	DirectX::XMFLOAT3 CameraPosition;
};

struct PerLightData 
{
	Light ThisLight;
};

// Must match vertex shader definition!
struct VertexShaderPointLightData
{
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 projection;
	DirectX::XMFLOAT3 cameraPosition;
};

struct PerLight
{
	DirectX::XMFLOAT4X4 world;
};

struct PerFramePointLight
{	
	DirectX::XMFLOAT4X4 InvViewProj;
	DirectX::XMFLOAT3 CameraPosition;
	float WindowWidth;
	float WindowHeight;
};