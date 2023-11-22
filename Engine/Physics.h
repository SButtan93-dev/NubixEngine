#pragma once

#include "PxPhysicsAPI.h"
#include "pvd/PxPvdTransport.h"
#include "Transform.h";
#include <iostream>

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