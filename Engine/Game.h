#pragma once

#include "DXCore.h"
#include "Mesh.h"
#include "GameEntity.h"
#include "Transform.h"
#include "Camera.h"
#include "Lights.h"

#include "Physics.h"
#include <DirectXMath.h>
#include <wrl/client.h> // Used for ComPtr - a smart pointer for COM objects
#include <vector>
#include <memory>

class Game 
	: public DXCore
{

public:
	Game(HINSTANCE hInstance);
	~Game();

	// Overridden setup and game loop methods, which
	// will be called automatically
	void Init();
	void OnResize();
	void Update(float deltaTime, float totalTime);
	void Draw(float deltaTime, float totalTime);

	void RenderGBuffer();
	void RenderLighting();

	float Lerp(float a, float b, float f);

	Physics* physics;

private:

	// Initialization helper methods - feel free to customize, combine, etc.
	void CreateRootSigAndPipelineState();
	void CreateBasicGeometry();
	void GenerateLights();
	
	// Overall pipeline and rendering requirements
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignatureGBuffer;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateGBuffer;

	// Overall pipeline and rendering requirements
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignatureLighting;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateLighting;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignaturePointLight;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStatePointLight;


	D3D12_CPU_DESCRIPTOR_HANDLE targets[4];
	D3D12_CPU_DESCRIPTOR_HANDLE lightTarget;

	std::shared_ptr<Mesh> sphere3;

	// Scene
	int lightCount;
	std::vector<Light> lights;
	std::shared_ptr<Camera> camera;
	std::vector<std::shared_ptr<GameEntity>> entities;

	std::vector<DirectX::XMFLOAT3> targetPositions;
	std::vector<DirectX::XMFLOAT3> originalPositions;

	float count = 0.0f;


	bool forwardDest = false;

};

