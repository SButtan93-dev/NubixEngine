#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "BufferStructs.h"
#include "DX12Helper.h"
#include "Material.h"
#include "Helpers.h"

#include <stdlib.h>     // For seeding random and rand()
#include <time.h>       // For grabbing time (to seed random)

// Needed for a helper function to read compiled shader files from the hard drive
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// For the DirectX Math library
using namespace DirectX;

// Helper macro for getting a float between min and max
#define RandomRange(min, max) (float)rand() / RAND_MAX * (max - min) + min

// --------------------------------------------------------
// Constructor
//
// DXCore (base class) constructor will set up underlying fields.
// DirectX itself, and our window, are not ready yet!
//
// hInstance - the application's OS-level handle (unique ID)
// --------------------------------------------------------
Game::Game(HINSTANCE hInstance)
	: DXCore(
		hInstance,		// The application's handle
		L"DirectX Game",// Text for the window's title bar
		1280,			// Width of the window's client area
		720,			// Height of the window's client area
		false,			// Sync the framerate to the monitor refresh? (lock framerate)
		true),			// Show extra stats (fps) in title bar?
	lightCount(32)
{

#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.\n");
#endif

}

// --------------------------------------------------------
// Destructor - Clean up anything our game has created:
//  - Release all DirectX objects created here
//  - Delete any objects to prevent memory leaks
// --------------------------------------------------------
Game::~Game()
{
	// Note: Since we're using smart pointers (ComPtr),
	// we don't need to explicitly clean up those DirectX objects
	// - If we weren't using smart pointers, we'd need
	//   to call Release() on each DirectX object created in Game
	
	// However, we DO need to wait here until the GPU
	// is actually done with its work

	

	DX12Helper::GetInstance().WaitForGPU();

	//physics->ShutDown();
	delete physics;
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	// Seed random
	srand((unsigned int)time(0));

	physics = new Physics();

	physics->InitPhysics();

	// Helper methods for loading shaders, creating some basic
	// geometry to draw and some simple camera matrices.
	//  - You'll be expanding and/or replacing these later
	CreateRootSigAndPipelineState();
	CreateBasicGeometry();
	GenerateLights();

	camera = std::make_shared<Camera>(
		XMFLOAT3(0.0f, 0.0f, -35.0f),	// Position
		5.0f,							// Move speed
		0.002f,							// Look speed
		XM_PIDIV4,						// Field of view
		windowWidth / (float)windowHeight);	// Aspect ratio
}


// --------------------------------------------------------
// Loads the two basic shaders, then creates the root signature 
// and pipeline state object for our very basic demo.
// --------------------------------------------------------
void Game::CreateRootSigAndPipelineState()
{
	// Blobs to hold raw shader byte code used in several steps below
	Microsoft::WRL::ComPtr<ID3DBlob> gBufferVertexShaderByteCode;
	//Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderByteCode;
	Microsoft::WRL::ComPtr<ID3DBlob> gBufferPixelShaderByteCode;


	// Blobs to hold raw shader byte code used in several steps below
	Microsoft::WRL::ComPtr<ID3DBlob> lightingVertexShaderByteCode;
	//Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderByteCode;
	Microsoft::WRL::ComPtr<ID3DBlob> lightingPixelShaderByteCode;

	// Blobs to hold raw shader byte code used in several steps below
	Microsoft::WRL::ComPtr<ID3DBlob> pointLightingVertexShaderByteCode;
	//Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderByteCode;
	Microsoft::WRL::ComPtr<ID3DBlob> pointLightingPixelShaderByteCode;


	// Load shaders
	{
		// Read our compiled vertex shader code into a blob
		// - Essentially just "open the file and plop its contents here"
		D3DReadFileToBlob(FixPath(L"VertexShader.cso").c_str(), gBufferVertexShaderByteCode.GetAddressOf());

		// Load G-buffer pixel shader
		D3DReadFileToBlob(FixPath(L"GBuffer.cso").c_str(), gBufferPixelShaderByteCode.GetAddressOf());

		// Deferred Directional Light
		D3DReadFileToBlob(FixPath(L"DeferredDirectionalLightVS.cso").c_str(), lightingVertexShaderByteCode.GetAddressOf());
		D3DReadFileToBlob(FixPath(L"DeferredDirectionalLightPS.cso").c_str(), lightingPixelShaderByteCode.GetAddressOf());
	
		// Deferred Point Light
		D3DReadFileToBlob(FixPath(L"DeferredPointLightVS.cso").c_str(), pointLightingVertexShaderByteCode.GetAddressOf());
		D3DReadFileToBlob(FixPath(L"DeferredPointLightPS.cso").c_str(), pointLightingPixelShaderByteCode.GetAddressOf());
	}

	// Input layout
	const unsigned int inputElementCount = 4;
	D3D12_INPUT_ELEMENT_DESC inputElements[inputElementCount] = {};
	{
		// Create an input layout that describes the vertex format
		// used by the vertex shader we're using
		//  - This is used by the pipeline to know how to interpret the raw data
		//     sitting inside a vertex buffer

		// Set up the first element - a position, which is 3 float values
		inputElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT; // How far into the vertex is this?  Assume it's after the previous element
		inputElements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;		// Most formats are described as color channels, really it just means "Three 32-bit floats"
		inputElements[0].SemanticName = "POSITION";					// This is "POSITION" - needs to match the semantics in our vertex shader input!
		inputElements[0].SemanticIndex = 0;							// This is the 0th position (there could be more)

		// Set up the second element - a UV, which is 2 more float values
		inputElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;	// After the previous element
		inputElements[1].Format = DXGI_FORMAT_R32G32_FLOAT;			// 2x 32-bit floats
		inputElements[1].SemanticName = "TEXCOORD";					// Match our vertex shader input!
		inputElements[1].SemanticIndex = 0;							// This is the 0th uv (there could be more)

		// Set up the third element - a normal, which is 3 more float values
		inputElements[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;	// After the previous element
		inputElements[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;		// 3x 32-bit floats
		inputElements[2].SemanticName = "NORMAL";					// Match our vertex shader input!
		inputElements[2].SemanticIndex = 0;							// This is the 0th normal (there could be more)

		// Set up the fourth element - a tangent, which is 2 more float values
		inputElements[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;	// After the previous element
		inputElements[3].Format = DXGI_FORMAT_R32G32B32_FLOAT;		// 3x 32-bit floats
		inputElements[3].SemanticName = "TANGENT";					// Match our vertex shader input!
		inputElements[3].SemanticIndex = 0;							// This is the 0th tangent (there could be more)
	}

	// --------------------------------------------
	// Root Signature for GBuffer
	// --------------------------------------------
	{
		// Describe the range of CBVs needed for the vertex shader
		D3D12_DESCRIPTOR_RANGE cbvRangeVS = {};
		cbvRangeVS.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRangeVS.NumDescriptors = 1;
		cbvRangeVS.BaseShaderRegister = 0;
		cbvRangeVS.RegisterSpace = 0;
		cbvRangeVS.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		// Describe the range of CBVs needed for the G-buffer pixel shader
		D3D12_DESCRIPTOR_RANGE cbvRangeGBufferPS = {};
		cbvRangeGBufferPS.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRangeGBufferPS.NumDescriptors = 1;
		cbvRangeGBufferPS.BaseShaderRegister = 1;  // Assuming the G-buffer CBV is registered at b1
		cbvRangeGBufferPS.RegisterSpace = 0;
		cbvRangeGBufferPS.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		// Create a range of SRV's for textures
		D3D12_DESCRIPTOR_RANGE srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = 4;		// Set to max number of textures at once (match pixel shader!)
		srvRange.BaseShaderRegister = 0;	// Starts at s0 (match pixel shader!)
		srvRange.RegisterSpace = 0;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		// Create the root parameters
		D3D12_ROOT_PARAMETER rootParams[3] = {};

		// CBV table param for vertex shader
		rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[0].DescriptorTable.pDescriptorRanges = &cbvRangeVS;


		// CBV table param for G-buffer pixel shader
		rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[1].DescriptorTable.pDescriptorRanges = &cbvRangeGBufferPS;

		// SRV table param
		rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[2].DescriptorTable.pDescriptorRanges = &srvRange;

		// Create a single static sampler (available to all pixel shaders at the same slot)
		// Note: This is in lieu of having materials have their own samplers for this demo
		D3D12_STATIC_SAMPLER_DESC anisoWrap = {};
		anisoWrap.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.Filter = D3D12_FILTER_ANISOTROPIC;
		anisoWrap.MaxAnisotropy = 16;
		anisoWrap.MaxLOD = D3D12_FLOAT32_MAX;
		anisoWrap.ShaderRegister = 0;  // register(s0)
		anisoWrap.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		
		D3D12_STATIC_SAMPLER_DESC samplers[] = { anisoWrap };

		// Describe and serialize the root signature
		D3D12_ROOT_SIGNATURE_DESC rootSig = {};
		rootSig.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSig.NumParameters = ARRAYSIZE(rootParams);
		rootSig.pParameters = rootParams;
		rootSig.NumStaticSamplers = ARRAYSIZE(samplers);
		rootSig.pStaticSamplers = samplers;

		ID3DBlob* serializedRootSig = 0;
		ID3DBlob* errors = 0;

		D3D12SerializeRootSignature(
			&rootSig,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&serializedRootSig,
			&errors);

		// Check for errors during serialization
		if (errors != 0)
		{
			OutputDebugString((wchar_t*)errors->GetBufferPointer());
		}

		// Actually create the root sig
		device->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(rootSignatureGBuffer.GetAddressOf()));
	}

	// -------------------------------------------
	// Root Signature for Directional Light Pass
	// --------------------------------------------
	{
		// Describe the range of CBVs needed for the lighting shader
		// Assuming perFrame data is at b0 and perLight data is at b1
		D3D12_DESCRIPTOR_RANGE cbvRanges[2] = {};
		cbvRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRanges[0].NumDescriptors = 1; // One for perFrame
		cbvRanges[0].BaseShaderRegister = 0; // b0
		cbvRanges[0].RegisterSpace = 0;
		cbvRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		cbvRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRanges[1].NumDescriptors = 1; // One for perLight
		cbvRanges[1].BaseShaderRegister = 1; // b1
		cbvRanges[1].RegisterSpace = 0;
		cbvRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		// Describe the range of SRVs for the GBuffer textures
		D3D12_DESCRIPTOR_RANGE srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = 4; // Assuming 4 GBuffer textures
		srvRange.BaseShaderRegister = 0; // t0, t1, t2, t3
		srvRange.RegisterSpace = 0;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		// Create the root parameters
		D3D12_ROOT_PARAMETER rootParams[3] = {};

		// CBV table param for perFrame data
		rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[0].DescriptorTable.pDescriptorRanges = &cbvRanges[0];

		// CBV table param for perLight data
		rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[1].DescriptorTable.pDescriptorRanges = &cbvRanges[1];

		// SRV table param for GBuffer textures
		rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[2].DescriptorTable.pDescriptorRanges = &srvRange;

		// Describe and serialize the root signature
		D3D12_ROOT_SIGNATURE_DESC rootSig = {};
		rootSig.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSig.NumParameters = ARRAYSIZE(rootParams);
		rootSig.pParameters = rootParams;
		rootSig.NumStaticSamplers = 0;
		rootSig.pStaticSamplers = nullptr;

		// Serialization and creation of the root signature
		ID3DBlob* serializedRootSig = nullptr;
		ID3DBlob* errors = nullptr;
		D3D12SerializeRootSignature(
			&rootSig,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&serializedRootSig,
			&errors);

		// Check for errors during serialization
		if (errors != nullptr)
		{
			OutputDebugString((wchar_t*)errors->GetBufferPointer());
		}

		// Actually create the root sig
		device->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(rootSignatureLighting.GetAddressOf()));

		// Release resources if necessary
		if (serializedRootSig) serializedRootSig->Release();
		if (errors) errors->Release();
	}

	// --------------------------------------------
	// Root Signature for Point Light Pass
	// --------------------------------------------
	{
		// Describe the range of CBVs needed for the lighting shader
		// Assuming perFrame data is at b0 and perLight data is at b1
		D3D12_DESCRIPTOR_RANGE cbvRangesPS[2] = {};
		cbvRangesPS[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRangesPS[0].NumDescriptors = 1; // One for perFrame
		cbvRangesPS[0].BaseShaderRegister = 0; // b0
		cbvRangesPS[0].RegisterSpace = 0;
		cbvRangesPS[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		cbvRangesPS[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRangesPS[1].NumDescriptors = 1; // One for perLight
		cbvRangesPS[1].BaseShaderRegister = 1; // b1
		cbvRangesPS[1].RegisterSpace = 0;
		cbvRangesPS[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		// Describe the range of CBVs needed for the lighting shader
		// Assuming perFrame data is at b0 and perLight data is at b1
		D3D12_DESCRIPTOR_RANGE cbvRangesVS[2] = {};
		cbvRangesVS[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRangesVS[0].NumDescriptors = 1; // One for perFrame
		cbvRangesVS[0].BaseShaderRegister = 0; // b0
		cbvRangesVS[0].RegisterSpace = 0;
		cbvRangesVS[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		cbvRangesVS[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRangesVS[1].NumDescriptors = 1; // One for perLight
		cbvRangesVS[1].BaseShaderRegister = 1; // b1
		cbvRangesVS[1].RegisterSpace = 0;
		cbvRangesVS[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		// Describe the range of SRVs for the point light textures
		D3D12_DESCRIPTOR_RANGE srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = 4; // Assuming 4 GBuffer textures
		srvRange.BaseShaderRegister = 0; // t0, t1, t2, t3
		srvRange.RegisterSpace = 0;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		// Create the root parameters
		D3D12_ROOT_PARAMETER rootParams[5] = {};

		// CBV table param for perFrame data Vertex Shader
		rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[0].DescriptorTable.pDescriptorRanges = &cbvRangesVS[0];

		// CBV table param for perLight data Vertex Shader
		rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[1].DescriptorTable.pDescriptorRanges = &cbvRangesVS[1];

		// CBV table param for perFrame data Pixel Shader
		rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[2].DescriptorTable.pDescriptorRanges = &cbvRangesPS[0];

		// CBV table param for perLight data Pixel Shader
		rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[3].DescriptorTable.pDescriptorRanges = &cbvRangesPS[1];

		// SRV table param for GBuffer textures
		rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams[4].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[4].DescriptorTable.pDescriptorRanges = &srvRange;

		// Describe and serialize the root signature
		D3D12_ROOT_SIGNATURE_DESC rootSig = {};
		rootSig.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSig.NumParameters = ARRAYSIZE(rootParams);
		rootSig.pParameters = rootParams;
		rootSig.NumStaticSamplers = 0;
		rootSig.pStaticSamplers = nullptr;

		// Serialization and creation of the root signature
		ID3DBlob* serializedRootSig = nullptr;
		ID3DBlob* errors = nullptr;
		D3D12SerializeRootSignature(
			&rootSig,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&serializedRootSig,
			&errors);

		// Check for errors during serialization
		if (errors != nullptr)
		{
			OutputDebugString((wchar_t*)errors->GetBufferPointer());
		}

		// Actually create the root sig
		HRESULT hr = device->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(rootSignaturePointLight.GetAddressOf()));

		if (FAILED(hr))
		{
			printf("error");
			//device->CreateGraphicsPipelineState(&psoDescPointLight, IID_PPV_ARGS(pipelineStatePointLight.GetAddressOf()));
		}

		// Release resources if necessary
		if (serializedRootSig) serializedRootSig->Release();
		if (errors) errors->Release();
	}


	// --------------------------------------------
	// Pipeline state for GBuffer Pass
	// --------------------------------------------
	{
		// Describe the pipeline state
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

		// -- Input assembler related ---
		psoDesc.InputLayout.NumElements = inputElementCount;
		psoDesc.InputLayout.pInputElementDescs = inputElements;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		// Overall primitive topology type (triangle, line, etc.) is set here 
		// IASetPrimTop() is still used to set list/strip/adj options
		// See: https://docs.microsoft.com/en-us/windows/desktop/direct3d12/managing-graphics-pipeline-state-in-direct3d-12

		// Root sig
		psoDesc.pRootSignature = rootSignatureGBuffer.Get();

		// -- Shaders (VS/PS) --- 
		psoDesc.VS.pShaderBytecode = gBufferVertexShaderByteCode->GetBufferPointer();
		psoDesc.VS.BytecodeLength = gBufferVertexShaderByteCode->GetBufferSize();
		//psoDesc.PS.pShaderBytecode = pixelShaderByteCode->GetBufferPointer();
		//psoDesc.PS.BytecodeLength = pixelShaderByteCode->GetBufferSize();


		// -- Render targets ---
		psoDesc.NumRenderTargets = 4;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.RTVFormats[2] = DXGI_FORMAT_R32_FLOAT;
		psoDesc.RTVFormats[3] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		// Update the shaders for G-buffer rendering
		psoDesc.PS.pShaderBytecode = gBufferPixelShaderByteCode->GetBufferPointer();
		psoDesc.PS.BytecodeLength = gBufferPixelShaderByteCode->GetBufferSize();

		// -- States ---
		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		psoDesc.RasterizerState.DepthClipEnable = true;

		psoDesc.DepthStencilState.DepthEnable = true;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		// -- Misc ---
		psoDesc.SampleMask = 0xffffffff;

		// Create the pipe state object
		device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelineStateGBuffer.GetAddressOf()));
	}

	// --------------------------------------------
	// Pipeline state for Directional Light Pass
	// --------------------------------------------
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDescLighting = {};

		// No input layout required if vertices are generated in the shader
		psoDescLighting.InputLayout = { nullptr, 0 };
		psoDescLighting.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		// Root signature for the lighting pass
		psoDescLighting.pRootSignature = rootSignatureLighting.Get();

		// Shaders specific to the lighting pass
		psoDescLighting.VS.pShaderBytecode = lightingVertexShaderByteCode->GetBufferPointer();
		psoDescLighting.VS.BytecodeLength = lightingVertexShaderByteCode->GetBufferSize();

		psoDescLighting.PS.pShaderBytecode = lightingPixelShaderByteCode->GetBufferPointer();
		psoDescLighting.PS.BytecodeLength = lightingPixelShaderByteCode->GetBufferSize();

		// Set up render target formats for the lighting pass
		psoDescLighting.NumRenderTargets = 1; // Usually one output in lighting pass
		psoDescLighting.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // Adjust as necessary
		psoDescLighting.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		psoDescLighting.SampleDesc.Count = 1;   // No multisampling, just one sample per pixel
		psoDescLighting.SampleDesc.Quality = 0; // Default quality level

		psoDescLighting.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		psoDescLighting.RasterizerState.CullMode = D3D12_CULL_MODE_BACK; // or use D3D12_CULL_MODE_NONE if culling is not desired
		psoDescLighting.RasterizerState.DepthClipEnable = TRUE; // typically enabled

		// Depth stencil state (read-only depth)
		psoDescLighting.DepthStencilState.DepthEnable = false;
		psoDescLighting.DSVFormat = DXGI_FORMAT_UNKNOWN;

		// Additive blending for lighting calculations
		psoDescLighting.BlendState.RenderTarget[0].BlendEnable = TRUE;
		psoDescLighting.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		psoDescLighting.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		psoDescLighting.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDescLighting.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		psoDescLighting.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
		psoDescLighting.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		psoDescLighting.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		psoDescLighting.SampleMask = 0xffffffff;

		// Create the pipeline state object for the lighting pass
		device->CreateGraphicsPipelineState(&psoDescLighting, IID_PPV_ARGS(pipelineStateLighting.GetAddressOf()));
	}

	// --------------------------------------------
	// Pipeline state for Point Light Pass
	// --------------------------------------------
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDescPointLight = {};

		// No input layout required if vertices are generated in the shader
		psoDescPointLight.InputLayout.NumElements = inputElementCount;
		psoDescPointLight.InputLayout.pInputElementDescs = inputElements;
		psoDescPointLight.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		// Root signature for the lighting pass
		psoDescPointLight.pRootSignature = rootSignaturePointLight.Get();

		// Shaders specific to the lighting pass
		psoDescPointLight.VS.pShaderBytecode = pointLightingVertexShaderByteCode->GetBufferPointer();
		psoDescPointLight.VS.BytecodeLength = pointLightingVertexShaderByteCode->GetBufferSize();

		psoDescPointLight.PS.pShaderBytecode = pointLightingPixelShaderByteCode->GetBufferPointer();
		psoDescPointLight.PS.BytecodeLength = pointLightingPixelShaderByteCode->GetBufferSize();

		// Set up render target formats for the lighting pass
		psoDescPointLight.NumRenderTargets = 1; // Usually one output in lighting pass
		psoDescPointLight.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // Adjust as necessary
		psoDescPointLight.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		psoDescPointLight.SampleDesc.Count = 1;   // No multisampling, just one sample per pixel
		psoDescPointLight.SampleDesc.Quality = 0; // Default quality level

		psoDescPointLight.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		psoDescPointLight.RasterizerState.CullMode = D3D12_CULL_MODE_BACK; // or use D3D12_CULL_MODE_NONE if culling is not desired
		psoDescPointLight.RasterizerState.DepthClipEnable = TRUE; // typically enabled

		// Depth stencil state (read-only depth)
		psoDescPointLight.DepthStencilState.DepthEnable = false;
		//psoDescPointLight.DSVFormat = DXGI_FORMAT_UNKNOWN;

		// Additive blending for lighting calculations
		psoDescPointLight.BlendState.RenderTarget[0].BlendEnable = TRUE;
		psoDescPointLight.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		psoDescPointLight.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		psoDescPointLight.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDescPointLight.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		psoDescPointLight.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
		psoDescPointLight.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		psoDescPointLight.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		psoDescPointLight.SampleMask = 0xffffffff;

		// Create the pipeline state object for the lighting pass
		HRESULT hr = device->CreateGraphicsPipelineState(&psoDescPointLight, IID_PPV_ARGS(pipelineStatePointLight.GetAddressOf()));
		if (FAILED(hr))
		{
			printf("error");
			//device->CreateGraphicsPipelineState(&psoDescPointLight, IID_PPV_ARGS(pipelineStatePointLight.GetAddressOf()));
		}
	}
}


// --------------------------------------------------------
// Creates the geometry we're going to draw - a single triangle for now
// --------------------------------------------------------
void Game::CreateBasicGeometry()
{
	// Quick macro to simplify texture loading lines below
#define LoadTexture(x) DX12Helper::GetInstance().LoadTexture(FixPath(x).c_str())

	// Load textures
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneAlbedo = LoadTexture(L"../../Assets/Textures/cobblestone_albedo.png");
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneNormals = LoadTexture(L"../../Assets/Textures/cobblestone_normals.png");
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneRoughness = LoadTexture(L"../../Assets/Textures/cobblestone_roughness.png");
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneMetal = LoadTexture(L"../../Assets/Textures/cobblestone_metal.png");

	D3D12_CPU_DESCRIPTOR_HANDLE bronzeAlbedo = LoadTexture(L"../../Assets/Textures/bronze_albedo.png");
	D3D12_CPU_DESCRIPTOR_HANDLE bronzeNormals = LoadTexture(L"../../Assets/Textures/bronze_normals.png");
	D3D12_CPU_DESCRIPTOR_HANDLE bronzeRoughness = LoadTexture(L"../../Assets/Textures/bronze_roughness.png");
	D3D12_CPU_DESCRIPTOR_HANDLE bronzeMetal = LoadTexture(L"../../Assets/Textures/bronze_metal.png");

	D3D12_CPU_DESCRIPTOR_HANDLE scratchedAlbedo = LoadTexture(L"../../Assets/Textures/scratched_albedo.png");
	D3D12_CPU_DESCRIPTOR_HANDLE scratchedNormals = LoadTexture(L"../../Assets/Textures/scratched_normals.png");
	D3D12_CPU_DESCRIPTOR_HANDLE scratchedRoughness = LoadTexture(L"../../Assets/Textures/scratched_roughness.png");
	D3D12_CPU_DESCRIPTOR_HANDLE scratchedMetal = LoadTexture(L"../../Assets/Textures/scratched_metal.png");

	// During initialization
	gBufferRTVs[0] = CreateGBufferTexture(device.Get(), windowWidth, windowHeight, DXGI_FORMAT_R8G8B8A8_UNORM, 2);
	gBufferRTVs[1] = CreateGBufferTexture(device.Get(), windowWidth, windowHeight, DXGI_FORMAT_R8G8B8A8_UNORM, 3);
	gBufferRTVs[2] = CreateGBufferTexture(device.Get(), windowWidth, windowHeight, DXGI_FORMAT_R32_FLOAT, 4);
	gBufferRTVs[3] = CreateGBufferTexture(device.Get(), windowWidth, windowHeight, DXGI_FORMAT_R8G8B8A8_UNORM, 5);

	gBufferSRVs[0] = DX12Helper::GetInstance().CreateGBufferSRV(gBufferRTVs[0], DXGI_FORMAT_R8G8B8A8_UNORM);
	gBufferSRVs[1] = DX12Helper::GetInstance().CreateGBufferSRV(gBufferRTVs[1], DXGI_FORMAT_R8G8B8A8_UNORM); 
	gBufferSRVs[2] = DX12Helper::GetInstance().CreateGBufferSRV(gBufferRTVs[2], DXGI_FORMAT_R32_FLOAT);  
	gBufferSRVs[3] = DX12Helper::GetInstance().CreateGBufferSRV(gBufferRTVs[3], DXGI_FORMAT_R8G8B8A8_UNORM);  

	lightBufferRTV = CreateLightingTexture(device.Get(), windowWidth, windowHeight, DXGI_FORMAT_R8G8B8A8_UNORM, 6);

	// Create materials
	// Note: Samplers are handled by a single static sampler in the
	// root signature for this demo, rather than per-material
	std::shared_ptr<Material> cobbleMat = std::make_shared<Material>(pipelineStateGBuffer, XMFLOAT3(1, 1, 1));
	cobbleMat->AddTexture(cobblestoneAlbedo, 0);
	cobbleMat->AddTexture(cobblestoneNormals, 1);
	cobbleMat->AddTexture(cobblestoneRoughness, 2);
	cobbleMat->AddTexture(cobblestoneMetal, 3);
	cobbleMat->FinalizeTextures();

	std::shared_ptr<Material> bronzeMat = std::make_shared<Material>(pipelineStateGBuffer, XMFLOAT3(1, 1, 1));
	bronzeMat->AddTexture(bronzeAlbedo, 0);
	bronzeMat->AddTexture(bronzeNormals, 1);
	bronzeMat->AddTexture(bronzeRoughness, 2);
	bronzeMat->AddTexture(bronzeMetal, 3);
	bronzeMat->FinalizeTextures();

	std::shared_ptr<Material> scratchedMat = std::make_shared<Material>(pipelineStateGBuffer, XMFLOAT3(1, 1, 1));
	scratchedMat->AddTexture(scratchedAlbedo, 0);
	scratchedMat->AddTexture(scratchedNormals, 1);
	scratchedMat->AddTexture(scratchedRoughness, 2);
	scratchedMat->AddTexture(scratchedMetal, 3);
	scratchedMat->FinalizeTextures();

	// Load meshes
	std::shared_ptr<Mesh> floor		= std::make_shared<Mesh>(FixPath(L"../../Assets/Models/Sponza/Floor.obj").c_str());
	std::shared_ptr<Mesh> sphere	= std::make_shared<Mesh>(FixPath(L"../../Assets/Models/sphere.obj").c_str());
	std::shared_ptr<Mesh> sphere2 = std::make_shared<Mesh>(FixPath(L"../../Assets/Models/sphere.obj").c_str());
	std::shared_ptr<Mesh> helix		= std::make_shared<Mesh>(FixPath(L"../../Assets/Models/helix.obj").c_str());
	std::shared_ptr<Mesh> torus		= std::make_shared<Mesh>(FixPath(L"../../Assets/Models/torus.obj").c_str());
	std::shared_ptr<Mesh> cylinder	= std::make_shared<Mesh>(FixPath(L"../../Assets/Models/cylinder.obj").c_str());

	//// Create entities
	std::shared_ptr<GameEntity> entityPlane = std::make_shared<GameEntity>(floor, scratchedMat);
	entityPlane->GetTransform()->SetPosition(0.0f, 0.0f, -5.0f);

	//std::shared_ptr<GameEntity> entityHelix = std::make_shared<GameEntity>(helix, cobbleMat);
	//entityHelix->GetTransform()->SetPosition(0, 0, 0);

	std::shared_ptr<GameEntity> entitySphere = std::make_shared<GameEntity>(sphere, bronzeMat);
	entitySphere->GetTransform()->SetPosition(0.0f, 0.0f, 100.0f);

	std::shared_ptr<GameEntity> entitySphere2 = std::make_shared<GameEntity>(sphere, bronzeMat);
	entitySphere->GetTransform()->SetPosition(0.0f, 10.0f, 100.0f);

	sphere3 = std::make_shared<Mesh>(FixPath(L"../../Assets/Models/sphere.obj").c_str());
	
	//std::shared_ptr<GameEntity> entitySphere2 = std::make_shared<GameEntity>(sphere2, bronzeMat);
	//entitySphere2->GetTransform()->SetPosition(3, 0, 0);
	
	// Add to list
	entities.push_back(entityPlane);
	//entities.push_back(entityHelix);
	entities.push_back(entitySphere);

	entities.push_back(entitySphere2);
	//entities.push_back(entitySphere2);

	targets[0] = rtvHandles[2];
	targets[1] = rtvHandles[3];
	targets[2] = rtvHandles[4];
	targets[3] = rtvHandles[5];

	lightTarget = rtvHandles[6];
}


void Game::GenerateLights()
{
	// Reset
	lights.clear();

	// Setup directional lights
	Light dir1 = {};
	dir1.Type = LIGHT_TYPE_DIRECTIONAL;
	dir1.Direction = XMFLOAT3(1, -1, 1);
	dir1.Color = XMFLOAT3(0.8f, 0.8f, 0.8f);
	dir1.Intensity = 1.0f;

	Light dir2 = {};
	dir2.Type = LIGHT_TYPE_DIRECTIONAL;
	dir2.Direction = XMFLOAT3(-1, -0.25f, 0);
	dir2.Color = XMFLOAT3(0.2f, 0.2f, 0.2f);
	dir2.Intensity = 1.0f;

	Light dir3 = {};
	dir3.Type = LIGHT_TYPE_DIRECTIONAL;
	dir3.Direction = XMFLOAT3(0, -1, 1);
	dir3.Color = XMFLOAT3(0.2f, 0.2f, 0.2f);
	dir3.Intensity = 1.0f;

	// Add light to the list
	lights.push_back(dir1);
	lights.push_back(dir2);
	lights.push_back(dir3);

	// Create the rest of the lights
	while (lights.size() < MAX_LIGHTS)
	{
		Light point = {};
		point.Type = LIGHT_TYPE_POINT;
		point.Position = XMFLOAT3(RandomRange(-15.0f, -15.0f), RandomRange(-2.0f, 20.0f), RandomRange(0, 30));
		point.Color = XMFLOAT3(RandomRange(0, 1), RandomRange(0, 1), RandomRange(0, 1));
		point.Range = 80.0f;
		point.Intensity = 0.1f;// RandomRange(0.1f, 3.0f);

		// Add to the list
		lights.push_back(point);
	}
	
	// Make sure we're exactly MAX_LIGHTS big
	lights.resize(MAX_LIGHTS);
}



// --------------------------------------------------------
// Handle resizing DirectX "stuff" to match the new window size.
// For instance, updating our projection matrix's aspect ratio.
// --------------------------------------------------------
void Game::OnResize()
{
	// Handle base-level DX resize stuff
	DXCore::OnResize();

	// Update the camera's projection to match the new size
	if (camera)
	{
		camera->UpdateProjectionMatrix((float)windowWidth / windowHeight);
	}
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Example input checking: Quit if the escape key is pressed
	if (Input::GetInstance().KeyDown(VK_ESCAPE))
	{
		Quit();
	}

	if (GetAsyncKeyState('E') & 0x8000)
	{
		physics->AddImpulse();
	}


	physics->DoSimulation(deltaTime);
	


	// Rotate entities
	for (auto& e : entities)
	{   //deltaTIme * 0.5f;
		e->GetTransform()->Rotate(0, 0, 0);	
	}

	// Update other objects
	camera->Update(deltaTime);
}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Grab the helper
	DX12Helper& dx12Helper = DX12Helper::GetInstance();

	//currentSwapBuffer = swapChain->GetCurrentBackBufferIndex();

	// Clearing the render target
	{
		for (int i = 0; i < 4; i++)
		{
			D3D12_RESOURCE_BARRIER rb = {};
			rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			rb.Transition.pResource = GBuffers[i].Get();
			rb.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			rb.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			commandList->ResourceBarrier(1, &rb);
		}

		// Transition the back buffer from PRESENT to RENDER_TARGET
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = backBuffers[currentSwapBuffer].Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &barrier);

		// Background color for clearing
		float color[] = { 0, 0, 0, 1.0f };

		for (int i = 0; i < 4; i++)
		{
			// Clear the RTV
			commandList->ClearRenderTargetView(
				targets[i],
				color,
				0, 0); // No scissor rectangles
		}

		float color2[] = { 0, 0, 0, 1.0f };

		commandList->ClearRenderTargetView(
			rtvHandles[currentSwapBuffer],
			color2,
			0, 0); // No scissor rectangles

		// Clear the depth buffer, too
		commandList->ClearDepthStencilView(
			dsvHandle,
			D3D12_CLEAR_FLAG_DEPTH,
			1.0f,	// Max depth = 1.0f
			0,		// Not clearing stencil, but need a value
			0, 0);	// No scissor rects
	}

	// Rendering here!
	{
		// Root sig (must happen before root descriptor table)
		commandList->SetGraphicsRootSignature(rootSignatureGBuffer.Get());

		// Set constant buffer
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = dx12Helper.GetCBVSRVDescriptorHeap();
		commandList->SetDescriptorHeaps(1, descriptorHeap.GetAddressOf());		

		int numTargets = 4;
		// Set up other commands for rendering
		commandList->OMSetRenderTargets(numTargets, targets, true, &dsvHandle);

		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &scissorRect);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		physics->FetchSimulationResults();

		const int limit = 2;

		// + 1 is for spheres
		for (int index = 0; index < limit; index++)
		{
			physx::PxTransform dynamicColliderPos = physics->GetSphereColliderPosition(index);

			physx::PxTransform entityPos = physx::PxTransform(entities[index + 1]->GetTransform()->GetPosition().x,
														entities[index+1]->GetTransform()->GetPosition().y, 
														entities[index + 1]->GetTransform()->GetPosition().z);

			physx::PxVec3 relativeTransform = physx::PxVec3(dynamicColliderPos.p.x, dynamicColliderPos.p.y, dynamicColliderPos.p.z)
											- physx::PxVec3(entityPos.p.x, entityPos.p.y, entityPos.p.z);

			entities[index + 1]->GetTransform()->SetPosition(entities[index + 1]->GetTransform()->GetPosition().x + relativeTransform.x,
															entities[index + 1]->GetTransform()->GetPosition().y + relativeTransform.y,
															entities[index + 1]->GetTransform()->GetPosition().z + relativeTransform.z);

			DirectX::XMFLOAT3 conv2 = physics->GetSphereColliderRotation(index);

			entities[index + 1]->GetTransform()->SetRotation(conv2.x, conv2.z, conv2.y);
		}

		int count = 0;

		RenderGBuffer();
	
		RenderLighting();
	}

	// Present
	{
		for (int i = 0; i < 4; i++)
		{
			D3D12_RESOURCE_BARRIER rb = {};
			rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			rb.Transition.pResource = GBuffers[i].Get();
			rb.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			rb.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			commandList->ResourceBarrier(1, &rb);
		}

		// Transition lightTarget to readable state
		D3D12_RESOURCE_BARRIER lightTargetToReadBarrier = {};
		lightTargetToReadBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		lightTargetToReadBarrier.Transition.pResource = lightBuffer.Get(); // Replace with your light target resource
		lightTargetToReadBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		lightTargetToReadBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
		lightTargetToReadBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &lightTargetToReadBarrier);

		// Transition back buffer to writable state
		D3D12_RESOURCE_BARRIER backBufferToWriteBarrier = {};
		backBufferToWriteBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		backBufferToWriteBarrier.Transition.pResource = backBuffers[currentSwapBuffer].Get();
		backBufferToWriteBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		backBufferToWriteBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		backBufferToWriteBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &backBufferToWriteBarrier);

		// Copy lightTarget to the back buffer
		commandList->CopyResource(backBuffers[currentSwapBuffer].Get(), lightBuffer.Get());

		// Transition back buffer to present state
		D3D12_RESOURCE_BARRIER backBufferToPresentBarrier = backBufferToWriteBarrier;
		backBufferToPresentBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		backBufferToPresentBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		commandList->ResourceBarrier(1, &backBufferToPresentBarrier);

		// Must occur BEFORE present
		// Note: Resetting the allocator every frame requires us to sync the CPU & GPU,
		//       since we cannot reset the allocator if its command list is executing.
		//       This is a pretty big performance hit, as we can't proceed until the GPU
		//       is totally done.  A better solution would be to have several allocators
		//       and command lists, and rotate through them with each successive frame.
		dx12Helper.CloseExecuteAndResetCommandList();

		// Present the current back buffer
		bool vsyncNecessary = vsync || !deviceSupportsTearing || isFullscreen;
		swapChain->Present(
			vsyncNecessary ? 1 : 0,
			vsyncNecessary ? 0 : DXGI_PRESENT_ALLOW_TEARING);

		// Figure out which buffer is next
		currentSwapBuffer++;

		if (currentSwapBuffer >= 2)
			currentSwapBuffer = 0;

		currentGBufferCount++;
		if (currentGBufferCount >= 4)
			currentGBufferCount = 0;

	}
}

void Game::RenderGBuffer()
{
	// Loop through the meshes
	{
		for (auto& e : entities)
		{
			// Grab the material for this entity
			std::shared_ptr<Material> mat = e->GetMaterial();

			// Set the pipeline state for this material
			commandList->SetPipelineState(mat->GetPipelineState().Get());

			// Set up the vertex shader data we intend to use for drawing this entity
			{
				VertexShaderExternalData vsData = {};
				vsData.world = e->GetTransform()->GetWorldMatrix();
				vsData.worldInverseTranspose = e->GetTransform()->GetWorldInverseTransposeMatrix();
				vsData.view = camera->GetView();
				vsData.projection = camera->GetProjection();

				// Send this to a chunk of the constant buffer heap
				// and grab the GPU handle for it so we can set it for this draw
				D3D12_GPU_DESCRIPTOR_HANDLE cbHandleVS = DX12Helper::GetInstance().FillNextConstantBufferAndGetGPUDescriptorHandle(
					(void*)(&vsData), sizeof(VertexShaderExternalData));

				// Set this constant buffer handle
				// Note: This assumes that descriptor table 0 is the
				//       place to put this particular descriptor.  This
				//       is based on how we set up our root signature.
				commandList->SetGraphicsRootDescriptorTable(0, cbHandleVS);
			}

			GBufferPixelShaderExternalData psData = {};
			psData.color = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);

			D3D12_GPU_DESCRIPTOR_HANDLE cbHandlePS = DX12Helper::GetInstance().FillNextConstantBufferAndGetGPUDescriptorHandle(
				(void*)(&psData), sizeof(GBufferPixelShaderExternalData));

			//// Set this constant buffer handle
			//// Note: This assumes that descriptor table 1 is the
			////       place to put this particular descriptor.  This
			////       is based on how we set up our root signature.
			commandList->SetGraphicsRootDescriptorTable(1, cbHandlePS);

			// Set the G-buffer textures as shader resources
			// Set the SRV descriptor handle for this material's textures
			// Note: This assumes that descriptor table 2 is for textures (as per our root sig)
			commandList->SetGraphicsRootDescriptorTable(2, mat->GetFinalGPUHandleForTextures());

			// Grab the mesh and its buffer views
			std::shared_ptr<Mesh> mesh = e->GetMesh();
			D3D12_VERTEX_BUFFER_VIEW vbv = mesh->GetVB();
			D3D12_INDEX_BUFFER_VIEW  ibv = mesh->GetIB();

			// Set the geometry
			commandList->IASetVertexBuffers(0, 1, &vbv);
			commandList->IASetIndexBuffer(&ibv);

			// Draw
			commandList->DrawIndexedInstanced(mesh->GetIndexCount(), 1, 0, 0, 0);

		}
	}
}

void Game::RenderLighting()
{
	commandList->SetGraphicsRootSignature(rootSignatureLighting.Get());
	//commandList->SetGraphicsRootSignature(rootSignaturePointLight.Get());

	// Clear the render target
	const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	commandList->ClearRenderTargetView(lightTarget, clearColor, 0, 0);

	commandList->OMSetRenderTargets(1, &lightTarget, FALSE, nullptr);

	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (unsigned int i = 0; i < 3; i++) 
	{		
		commandList->SetPipelineState(pipelineStateLighting.Get());
		//	Pixel shader data and cbuffer setup
		{
			Light light = lights[i];

			PerFrameData psData = {};
			psData.CameraPosition = camera->GetTransform()->GetPosition();
			psData.InvViewProj = camera->GetInverseViewProjectionMatrix();

			D3D12_GPU_DESCRIPTOR_HANDLE cbHandlePS = DX12Helper::GetInstance().FillNextConstantBufferAndGetGPUDescriptorHandle(
				(void*)(&psData), sizeof(PerFrameData));
			commandList->SetGraphicsRootDescriptorTable(0, cbHandlePS);

			PerLightData perLightData = {};
			perLightData.ThisLight = light;

			D3D12_GPU_DESCRIPTOR_HANDLE cbHandlePerLight = DX12Helper::GetInstance().FillNextConstantBufferAndGetGPUDescriptorHandle(
				(void*)(&perLightData), sizeof(PerLightData));

			commandList->SetGraphicsRootDescriptorTable(1, cbHandlePerLight);

			commandList->SetGraphicsRootDescriptorTable(2, gBufferSRVs[0]);

			// Draw the light (e.g., fullscreen triangle)
			commandList->DrawInstanced(3, 1, 0, 0);		
		}
	}
	
	commandList->SetGraphicsRootSignature(rootSignaturePointLight.Get());

	for (unsigned int i = 3; i < 100; i++)
	{
		commandList->SetPipelineState(pipelineStatePointLight.Get());
		//	Pixel shader data and cbuffer setup
		{
			Light light = lights[i];

			VertexShaderPointLightData vsData = {};
			vsData.view = camera->GetView();
			vsData.projection = camera->GetProjection();
			vsData.cameraPosition = camera->GetTransform()->GetPosition();

			D3D12_GPU_DESCRIPTOR_HANDLE cbHandleVS_1 = DX12Helper::GetInstance().FillNextConstantBufferAndGetGPUDescriptorHandle(
				(void*)(&vsData), sizeof(VertexShaderPointLightData));
			commandList->SetGraphicsRootDescriptorTable(0, cbHandleVS_1);


			float rad = light.Range; // This sphere model has a radius of 0.5, so double the scale
			XMFLOAT4X4 world;
			XMMATRIX trans = XMMatrixTranslationFromVector(XMLoadFloat3(&light.Position));
			XMMATRIX sc = XMMatrixScaling(rad, rad, rad);
			XMStoreFloat4x4(&world, sc * trans);

			PerLight vsData_2 = {};
			vsData_2.world = world;

			D3D12_GPU_DESCRIPTOR_HANDLE cbHandleVS_2 = DX12Helper::GetInstance().FillNextConstantBufferAndGetGPUDescriptorHandle(
				(void*)(&vsData_2), sizeof(PerLight));
			commandList->SetGraphicsRootDescriptorTable(1, cbHandleVS_2);

			PerFramePointLight psData = {};
			psData.CameraPosition = camera->GetTransform()->GetPosition();
			psData.InvViewProj = camera->GetInverseViewProjectionMatrix();
			psData.WindowHeight = windowHeight;
			psData.WindowWidth = windowWidth;

			D3D12_GPU_DESCRIPTOR_HANDLE cbHandlePS = DX12Helper::GetInstance().FillNextConstantBufferAndGetGPUDescriptorHandle(
				(void*)(&psData), sizeof(PerFramePointLight));
			commandList->SetGraphicsRootDescriptorTable(2, cbHandlePS);

			PerLightData perLightData = {};
			perLightData.ThisLight = light;

			D3D12_GPU_DESCRIPTOR_HANDLE cbHandlePerLight = DX12Helper::GetInstance().FillNextConstantBufferAndGetGPUDescriptorHandle(
				(void*)(&perLightData), sizeof(PerLightData));

			commandList->SetGraphicsRootDescriptorTable(3, cbHandlePerLight);

			commandList->SetGraphicsRootDescriptorTable(4, gBufferSRVs[0]);

			//std::shared_ptr<Mesh> sphere = std::make_shared<Mesh>(FixPath(L"../../Assets/Models/sphere.obj").c_str());

			// Set buffers in the input assembler
			UINT stride = sizeof(Vertex);
			UINT offset = 0;
			D3D12_VERTEX_BUFFER_VIEW vbv = sphere3->GetVB();
			D3D12_INDEX_BUFFER_VIEW  ibv = sphere3->GetIB();

			commandList->IASetVertexBuffers(0, 1, &vbv);
			commandList->IASetIndexBuffer(&ibv);

			// Draw this mesh
			commandList->DrawIndexedInstanced(sphere3->GetIndexCount(), 1, 0, 0, 0);

			//// Draw the light (e.g., fullscreen triangle)
			//commandList->DrawInstanced(3, 1, 0, 0);
		}
	}
}





//	Pixel shader data and cbuffer setup
//{
	//PixelShaderExternalData psData = {};
	//psData.uvScale = mat->GetUVScale();
	//psData.uvOffset = mat->GetUVOffset();
	//psData.cameraPosition = camera->GetTransform()->GetPosition();
	//psData.lightCount = lightCount;
	//memcpy(psData.lights, &lights[0], sizeof(Light) * MAX_LIGHTS);

	//// Send this to a chunk of the constant buffer heap
	//// and grab the GPU handle for it so we can set it for this draw
	//D3D12_GPU_DESCRIPTOR_HANDLE cbHandlePS = dx12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle(
	//	(void*)(&psData), sizeof(PixelShaderExternalData));

	//// Set this constant buffer handle
	//// Note: This assumes that descriptor table 1 is the
	////       place to put this particular descriptor.  This
	////       is based on how we set up our root signature.
	//commandList->SetGraphicsRootDescriptorTable(1, cbHandlePS);
//}