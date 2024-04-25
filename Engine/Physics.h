#pragma once

#include "PxPhysicsAPI.h"
#include "pvd/PxPvdTransport.h"
#include "NvBlastTk.h"
#include "lowlevel/NvBlast.h"
#include "NvBlastTkFramework.h"
#include "Transform.h";
#include <iostream>

#pragma comment(lib, "NvBlast.lib")
#pragma comment(lib, "NvBlastExtAssetUtils.lib")
#pragma comment(lib, "NvBlastExtAuthoring.lib")
#pragma comment(lib, "NvBlastExtSerialization.lib")
#pragma comment(lib, "NvBlastExtShaders.lib")
#pragma comment(lib, "NvBlastExtStress.lib")
#pragma comment(lib, "NvBlastExtTkSerialization.lib")
#pragma comment(lib, "NvBlastGlobals.lib")
#pragma comment(lib, "NvBlastTk.lib")
#pragma comment(lib, "UnitTests.lib")


class Physics
{
private:

	physx::PxPhysics* physics; // main adapter.
	physx::PxFoundation* foundation;

	physx::PxScene* phyScene; // Scene

	physx::PxDefaultAllocator* phyAllocator;

	// Dynamic actor #1
	std::vector<physx::PxRigidDynamic*> sphereActors;
	
	// Static plane
	physx::PxRigidStatic* planeActor;
	physx::PxRigidStatic* planeActor2;

	// Actors behaviour
	physx::PxMaterial* material;

	// Debug device
	physx::PxPvd* pvd;
	physx::PxPvdTransport* transport;
	physx::PxDefaultErrorCallback phyErrorLog;

	// Shapes
	physx::PxShape* sphereShape;
	physx::PxShape* planeShape;
	physx::PxShape* planeShape2;

	// Blast Toolkit members
	Nv::Blast::TkFramework* framework;
	Nv::Blast::TkAsset* blastAsset;
	std::vector<Nv::Blast::TkActor*> blastActors;

public:

	Physics();
	~Physics();

	void InitPhysics();
	void CreateScene();

	void CreateRigidBodyDynamic();
	void CreateSphereColliderAttachRigidBodyDynamic();

	void CreateRigidBodyStatic();
	void CreateAttachPlaneRigidBodyStatic();
	
	void CreateRigidBodyStatic2();
	void CreateAttachPlaneRigidBodyStatic2();

	void AddActors();

	// Return the transform from simulation back to the renderer mesh
	physx::PxTransform GetSphereColliderPosition(int index);
	DirectX::XMFLOAT3 GetSphereColliderRotation(int index);

	void AddImpulse();

	void DoSimulation(float deltaTime);
	void FetchSimulationResults();
};