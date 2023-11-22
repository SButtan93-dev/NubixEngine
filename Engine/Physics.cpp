#include "Physics.h"


Physics::~Physics()
{
	for (const auto& sphereActor : sphereActors)
		sphereActor->release();

	sphereActors.clear();
	sphereShape->release();

	planeActor->release();
	planeShape->release();
	planeShape2->release();

	material->release();

	phyScene->release();
	physics->release();
	pvd->release();
	transport->release();
	phyAllocator = nullptr;
	foundation->release();
}


Physics::Physics()
{
	phyAllocator = new physx::PxDefaultAllocator;
}


// -----------------------------------------------------------------------------------------------
// --------------------------------- INITIALIZATION ----------------------------------------------
// -----------------------------------------------------------------------------------------------
void Physics::InitPhysics()
{	
	foundation = PxCreateFoundation(PX_PHYSICS_VERSION, *phyAllocator, phyErrorLog);
	if (!foundation) {
		ferror;
		return;
	}

	transport = physx::PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
	pvd = physx::PxCreatePvd(*foundation);
	pvd->connect(*transport, physx::PxPvdInstrumentationFlag::eALL);

	physics = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, physx::PxTolerancesScale(), true, pvd);

	if (!physics) {
		std::cerr << "Error creating PhysX SDK." << std::endl;
		return;
	}

	CreateScene();

	CreateRigidBodyDynamic();
	CreateSphereColliderAttachRigidBodyDynamic();

	CreateRigidBodyStatic();
	CreateAttachPlaneRigidBodyStatic();

	CreateRigidBodyStatic2();
	CreateAttachPlaneRigidBodyStatic2();

	AddActors();

}

void Physics::CreateScene()
{
	physx::PxSceneDesc sceneDesc(physics->getTolerancesScale());
	sceneDesc.gravity = physx::PxVec3(0.0f, -9.5f, 0.0f);
	sceneDesc.cpuDispatcher = physx::PxDefaultCpuDispatcherCreate(2); // Set a CPU dispatcher
	sceneDesc.filterShader = physx::PxDefaultSimulationFilterShader; // Set the collision filter shader

	if (!sceneDesc.isValid()) {
		std::cerr << "Error creating PhysX SDK." << std::endl;
	}

	phyScene = physics->createScene(sceneDesc);

	physx::PxPvdSceneClient* pvdClient = phyScene->getScenePvdClient();
	if (pvdClient)
	{
		pvdClient->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
		pvdClient->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
		pvdClient->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
	}
}

void Physics::AddActors()
{
	for(const auto& sphereActor : sphereActors)
		phyScene->addActor(*sphereActor);

	phyScene->addActor(*planeActor);
	phyScene->addActor(*planeActor2);
}


//-------------------------------------------------------------------------------------------------------------
//-----------------------------------   DYNAMIC COLLIDERS -----------------------------------------------------
//-------------------------------------------------------------------------------------------------------------

void Physics::CreateRigidBodyDynamic()
{
	sphereActors.push_back(physics->createRigidDynamic(physx::PxTransform(physx::PxVec3(0.0f, 100.0f, 0.0f))));
	sphereActors.push_back(physics->createRigidDynamic(physx::PxTransform(physx::PxVec3(10.0f, 100.0f, 0.0f))));
}

void Physics::CreateSphereColliderAttachRigidBodyDynamic()
{
	physx::PxSphereGeometry sphereGeometry;
	sphereGeometry.radius = 1.0f;
	material = physics->createMaterial(0.5f, 0.5f, 0.4f);
	sphereShape = physics->createShape(sphereGeometry, *material);

	for(const auto& sphereActor : sphereActors)
		sphereActor->attachShape(*sphereShape);
}

//-------------------------------------------------------------------------------------------------------------
//-----------------------------------   STATIC COLLIDERS ------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------

void Physics::CreateRigidBodyStatic()
{
	// Create a plane shape
	planeActor = physics->createRigidStatic(physx::PxTransform(physx::PxVec3(0.0f, -3.0f, -5.0f), 
															   physx::PxQuat(physx::PxHalfPi, 
															   physx::PxVec3(0.0f, 0.0f, 1.0f))));
}

void Physics::CreateAttachPlaneRigidBodyStatic()
{
	planeShape = physics->createShape(physx::PxPlaneGeometry(), *material);
	planeActor->attachShape(*planeShape);
}

void Physics::CreateRigidBodyStatic2()
{
	planeActor2 = physics->createRigidStatic(physx::PxTransform(physx::PxVec3(5.0f, -3.0f, -20.0f), 
																physx::PxQuat(physx::PxHalfPi, 
																physx::PxVec3(0.0f, -1.0f, 0.0f))));
}

void Physics::CreateAttachPlaneRigidBodyStatic2()
{
	planeShape2 = physics->createShape(physx::PxPlaneGeometry(), *material);
	planeActor2->attachShape(*planeShape2);
}


// -----------------------------------------------------------------------------------------------
// --------------------------------- PHYSX GAME LOOP ---------------------------------------------
// -----------------------------------------------------------------------------------------------

void Physics::DoSimulation(float deltaTime)
{
	if(phyScene)
		phyScene->simulate(deltaTime*2);
}

void Physics::FetchSimulationResults()
{
	if (phyScene)
		phyScene->fetchResults(true);
}


physx::PxTransform Physics::GetSphereColliderPosition(int index)
{
	if (phyScene)
	{
		printf("%f %f %f.\n", sphereActors[index]->getGlobalPose().p.x, sphereActors[index]->getGlobalPose().p.y, sphereActors[index]->getGlobalPose().p.z);
		return sphereActors[index]->getGlobalPose();
	}
}

DirectX::XMFLOAT3 Physics::GetSphereColliderRotation(int index)
{
	if (phyScene)
	{
		Transform abc;
		physx::PxReal roll, pitch, yaw;
		physx::PxQuat rotation = sphereActors[index]->getGlobalPose().q;

		DirectX::XMFLOAT4 conv = DirectX::XMFLOAT4(rotation.x, rotation.z, rotation.y, rotation.w);
		DirectX::XMFLOAT3 conv2 = abc.QuaternionToEuler(conv);

		return conv2;
	}
}

void Physics::AddImpulse()
{
	for(const auto& sphereActor : sphereActors)
		sphereActor->addTorque(physx::PxVec3(-50.0f, 0.0f, 0.0f));
}