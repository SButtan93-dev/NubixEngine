#include "Physics.h"

MySimulationEventCallback::MySimulationEventCallback(Physics* physics) : mPhysics(physics) {}

void MySimulationEventCallback::onConstraintBreak(physx::PxConstraintInfo*, physx::PxU32) {}
void MySimulationEventCallback::onWake(physx::PxActor**, physx::PxU32) {}
void MySimulationEventCallback::onSleep(physx::PxActor**, physx::PxU32) {}
void MySimulationEventCallback::onTrigger(physx::PxTriggerPair*, physx::PxU32) {}
void MySimulationEventCallback::onAdvance(const physx::PxRigidBody* const*, const physx::PxTransform*, const physx::PxU32) {}


void MySimulationEventCallback::onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs) {
	for (physx::PxU32 i = 0; i < nbPairs; ++i) {
		const physx::PxContactPair& cp = pairs[i];
		if (cp.events & physx::PxPairFlag::eNOTIFY_TOUCH_FOUND) {
			physx::PxActor* actor0 = pairHeader.actors[0];
			physx::PxActor* actor1 = pairHeader.actors[1];

			// Extract contact points
			physx::PxContactPairPoint contactPoints[1];  // Array to store contact points
			int numContactPoints = cp.extractContacts(contactPoints, 1);

			if (numContactPoints > 0) {
				// Assume the first contact point is sufficient for our purposes
				float mag = contactPoints[0].impulse.magnitude();

				mPhysics->firstContactPosition = contactPoints[0].position;

				NvcVec3 impactPosition = { mPhysics->firstContactPosition.x, mPhysics->firstContactPosition.y, mPhysics->firstContactPosition.z };

				mPhysics->OnSphereHitGround(actor0, impactPosition);

			}
		}
	}
}

physx::PxFilterFlags MyFilterShader(
	physx::PxFilterObjectAttributes attributes0, physx::PxFilterData filterData0,
	physx::PxFilterObjectAttributes attributes1, physx::PxFilterData filterData1,
	physx::PxPairFlags& pairFlags, const void* constantBlock, physx::PxU32 constantBlockSize) {
	pairFlags = physx::PxPairFlag::eCONTACT_DEFAULT;
	pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND;
	pairFlags |= physx::PxPairFlag::eSOLVE_CONTACT;
	pairFlags |= physx::PxPairFlag::eDETECT_DISCRETE_CONTACT;
	pairFlags |= physx::PxPairFlag::eNOTIFY_CONTACT_POINTS; // Ensure contact points are generated
	return physx::PxFilterFlag::eDEFAULT;
}

void Physics::OnSphereHitGround(physx::PxActor* sphereActor, NvcVec3& impactPosition) {
	auto it = std::find(tracker.begin(), tracker.end(), sphereActor);


	if (it != tracker.end() && firstActorHit == true) {
		firstActorHit = false;

		// Get the transform of the actor (from world space to local space)
		physx::PxRigidDynamic* dynamicActor = static_cast<physx::PxRigidDynamic*>(sphereActor);
		physx::PxTransform actorTransform = dynamicActor->getGlobalPose();

		// Convert the impact position from world space to local space
		physx::PxVec3 worldImpactPosition(dynamicActor->getGlobalPose().p.x, dynamicActor->getGlobalPose().p.y - 0.5f, dynamicActor->getGlobalPose().p.z);
		physx::PxVec3 localImpactPosition = actorTransform.transformInv(worldImpactPosition);

		actorImpactPos = actorTransform;

		NvBlastDamageProgram damageProgram = { NvBlastExtFalloffGraphShader, NvBlastExtFalloffSubgraphShader };

		// Define the material for the actor
		NvBlastExtMaterial blastMaterial;
		blastMaterial.health = 100.0f;            // Total health of the actor
		blastMaterial.minDamageThreshold = 0.01f;  // Minimum threshold for damage to be applied
		blastMaterial.maxDamageThreshold = 0.5f;  // Maximum threshold before the actor is completely fractured

		// Define the radial damage description (e.g., explosion damage)
		NvBlastExtRadialDamageDesc damageDesc;
		damageDesc.position[0] = localImpactPosition.x;  // Explosion position X
		damageDesc.position[1] = localImpactPosition.y;  // Explosion position Y
		damageDesc.position[2] = localImpactPosition.z;  // Explosion position Z
		damageDesc.minRadius = 0.0f;    // Minimum radius of the damage area
		damageDesc.maxRadius = 3.0f;    // Maximum radius of the damage area
		damageDesc.damage = 1.0f;      // Intensity of the damage


		NvBlastExtDamageAccelerator* accel = NvBlastExtDamageAcceleratorCreate(blastTkAsset->getAssetLL(), 0);

		NvBlastExtProgramParams programParams = { &damageDesc, &blastMaterial, accel };

		// Define the radial damage description (e.g., explosion damage)
		NvBlastExtRadialDamageDesc damageDesc2;
		damageDesc2.position[0] = localImpactPosition.x;  // Explosion position X
		damageDesc2.position[1] = localImpactPosition.y;  // Explosion position Y
		damageDesc2.position[2] = localImpactPosition.z;  // Explosion position Z
		damageDesc2.minRadius = 0.0f;    // Minimum radius of the damage area
		damageDesc2.maxRadius = 3.0f;    // Maximum radius of the damage area
		damageDesc2.damage = 0.4f;      // Intensity of the damage


		NvBlastExtDamageAccelerator* accel2 = NvBlastExtDamageAcceleratorCreate(blastTkAsset->getAssetLL(), 0);

		NvBlastExtProgramParams programParams2 = { &damageDesc2, &blastMaterial, accel2 };

		// Apply damage to the first actor (twice)
		actorTk1->damage(damageProgram, &programParams);

		actorTk1->damage(damageProgram, &programParams2);

		// Process the group to apply the damage
		group->process();

		actorsToRemove.push_back(tracker[0]);

		// Remove the actor from the tracker vector
		auto it = std::find(tracker.begin(), tracker.end(), tracker[0]);
		if (it != tracker.end()) {
			tracker.erase(it);  // Remove the actor from the tracker
		}

		isSplitApplied = true;
	}
}

void Physics::RemoveQueuedActors() {
	for (physx::PxActor* actor : actorsToRemove) {
		phyScene->removeActor(*actor);
		actor->release();
		//tracker.erase()
	}
	actorsToRemove.clear();
}

Physics::~Physics()
{
	planeActor->release();
	planeShape->release();

	for (const auto& e : tracker)
	{
		if (e->isReleasable())
		{
			e->release();
		}

	}

	planeMaterial->release();
	meshMaterial->release();

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

	simulationEventCallback = new MySimulationEventCallback(this);


	CreateScene();


	// Set gravity here directly after creating the scene
	SetGravity(physx::PxVec3(0.0f, -0.0f, 0.0f));

	phyScene->setSimulationEventCallback(simulationEventCallback); // Ensure this is set

	planeMaterial = physics->createMaterial(0.0f, 0.0f, 1.0f); // static friction, dynamic friction, restitution

	meshMaterial = physics->createMaterial(0.7f, 0.6f, 0.1f); // static friction, dynamic friction, restitution

	CreateRigidBodyStatic();
	CreateAttachPlaneRigidBodyStatic();

	AddFloorActors();
}

void Physics::SetGravity(const physx::PxVec3& gravity)
{
	if (phyScene)
	{
		phyScene->setGravity(gravity);
	}
}

void Physics::CreateScene()
{
	physx::PxSceneDesc sceneDesc(physics->getTolerancesScale());
	sceneDesc.gravity = physx::PxVec3(0.0f, -0.0f, 0.0f);
	sceneDesc.filterShader = MyFilterShader;
	sceneDesc.simulationEventCallback = simulationEventCallback; // Ensure this is set
	sceneDesc.cpuDispatcher = physx::PxDefaultCpuDispatcherCreate(2); // CPU dispatcher

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

void Physics::AddFloorActors()
{
	phyScene->addActor(*planeActor);
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
	planeShape = physics->createShape(physx::PxPlaneGeometry(), *planeMaterial);
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

}


// -----------------------------------------------------------------------------------------------
// --------------------------------- PHYSX GAME LOOP ---------------------------------------------
// -----------------------------------------------------------------------------------------------

void Physics::DoSimulation(float deltaTime)
{
	const float fixedDeltaTime = 1.0f / 60.0f; // 60 FPS
	if (phyScene) {
		phyScene->simulate(fixedDeltaTime);
	}
}


void Physics::FetchSimulationResults()
{
	if (phyScene){

		bool test = phyScene->fetchResults(true);

		// Only set gravity after the results have been fetched and simulation has completed
		if (bGravity)
		{

			physx::PxVec3 newGravity(0.0f, -5.0f, 0.0f);
			SetGravity(newGravity);
			tracker[0]->addTorque(physx::PxVec3(0.0f, -5.0f, 0.0f), physx::PxForceMode::eFORCE, false);


			bGravity = false;
		}

		if (isSplitApplied){
				RemoveQueuedActors();
				isSplitApplied = false;

				// Add new actors from the buffer to the scene
				for (const auto& newActor : chunkActorsBuffer) {
					phyScene->addActor(*newActor);
				}
			}

		// Get the current gravity vector
		physx::PxVec3 currentGravity = phyScene->getGravity();
		std::cout << "Current Gravity: (" << currentGravity.x << ", " << currentGravity.y << ", " << currentGravity.z << ")\n";
	}
}


// -----------------------------------------------------------------------------------------------
// --------------------------------------- BLAST --------------------------------------------------
// -----------------------------------------------------------------------------------------------

// Used for initialization of original physx and blast mesh actor.
// Steps:
// 1. Pass initial mesh 
// 2. Clean (open edges?) and add to fracture tool
// 3. Use cleaned mesh to generate pre-fracture(Voronoi) using support graph structure (built-in)
// 4. Calculate all the chunks and their bonds. Root chunk (index 0) remains 'Unsupported', all others
// have their bonds set as 'supported'. This step is critical for a smooth blast destruction.
// 5. Create Blast actor
// 6. Create Physx actor for the root chunk i.e. the original object

void Physics::AddMeshToBlast(std::shared_ptr<GameEntity> e)
{
	renderEntities.push_back(e);
	// Step 1: Mesh creation and cleaning
	std::vector<uint32_t> blastIndices;
	std::vector<NvcVec3> blastVerts;
	std::vector<NvcVec3> blastNormals;
	std::vector<NvcVec2> blastUVs;

	blastVerts.reserve(e.get()->GetMesh().get()->verts.size());
	blastNormals.reserve(e.get()->GetMesh().get()->normals.size());
	blastUVs.reserve(e.get()->GetMesh().get()->uvs.size());
	blastIndices.reserve(e.get()->GetMesh().get()->indices.size());

	for (const auto& pos : e.get()->GetMesh().get()->verts){
		NvcVec3 blastVertex = { pos.Position.x, pos.Position.y, pos.Position.z };
		NvcVec3 blastNormal = { pos.Normal.x, pos.Normal.y, pos.Normal.z };
		NvcVec2 blastUV = { pos.UV.x, pos.UV.y };

		blastVerts.push_back(blastVertex);
		blastNormals.push_back(blastNormal);
		blastUVs.push_back(blastUV);
	}

	for (const auto& indices : e.get()->GetMesh().get()->indices){
		blastIndices.push_back(indices);
	}

	mesh = NvBlastExtAuthoringCreateMesh(blastVerts.data(), blastNormals.data(), blastUVs.data(),
		e.get()->GetMesh().get()->vertCounter, blastIndices.data(), e.get()->GetMesh().get()->GetIndexCount());

	Nv::Blast::MeshCleaner* cleaner = NvBlastExtAuthoringCreateMeshCleaner();
	cleanMesh = cleaner->cleanMesh(mesh);

	fractureTool = NvBlastExtAuthoringCreateFractureTool();

	// Set the mesh to the fracture tool
	Nv::Blast::Mesh const* const meshes[] = { cleanMesh };
	fractureTool->setSourceMeshes(meshes, 1);

	uint32_t cellCount = 10;
	CustomRandomGenerator randomGenerator;
	randomGenerator.seed(12345);  // Random

	Nv::Blast::VoronoiSitesGenerator* sitesGenerator = NvBlastExtAuthoringCreateVoronoiSitesGenerator(cleanMesh, &randomGenerator);
	if (!sitesGenerator) {
		std::cerr << "Failed to create Voronoi sites generator." << std::endl;
		return;
	}
	sitesGenerator->uniformlyGenerateSitesInMesh(cellCount);

	const NvcVec3* voronoiPoints;
	uint32_t generatedSites = sitesGenerator->getVoronoiSites(voronoiPoints);
	int32_t result = fractureTool->voronoiFracturing(0, generatedSites, voronoiPoints, false);
	fractureTool->finalizeFracturing();

	// Main Blast logic begins from here
	framework = NvBlastTkFrameworkCreate();

	// Get the number of chunks
	uint32_t chunkCount = fractureTool->getChunkCount();
	for (uint32_t i = 0; i < chunkCount; ++i) {
		// Get the polygonal mesh representation of the chunk
		Nv::Blast::Mesh* chunkMesh = fractureTool->createChunkMesh(i);

		// Extract vertices from chunkMesh
		uint32_t vertexCount = chunkMesh->getVerticesCount();
		const Nv::Blast::Vertex* vertices = chunkMesh->getVertices();

		// Store the vertices in a vector
		std::vector<NvcVec3> meshVertices;
		std::vector<NvcVec3> meshNormals;
		std::vector<NvcVec2> meshUVs;
		std::vector<NvcVec3> meshTangents; // If tangents are available

		meshVertices.reserve(vertexCount);
		for (uint32_t v = 0; v < vertexCount; ++v) {
			meshVertices.push_back(vertices[v].p);  // Assuming vertices have a 'p' position member
			meshNormals.push_back(vertices[v].n);   // Store normals
			meshUVs.push_back(vertices[v].uv[0]);   // Store UVs
		}

		// Prepare a vector to store indices
		std::vector<uint32_t> meshIndices;

		// Iterate through each facet of the chunk
		uint32_t facetCount = chunkMesh->getFacetCount();
		for (uint32_t f = 0; f < facetCount; ++f) {
			const Nv::Blast::Facet* facet = chunkMesh->getFacet(f);

			// Iterate through each edge of the facet
			for (uint32_t j = 0; j < facet->edgesCount; ++j) {
				const Nv::Blast::Edge* edge = chunkMesh->getEdges() + facet->firstEdgeNumber + j;

				// Add the start and end vertex indices of the edge to the indices array
				meshIndices.push_back(edge->s);  // Start vertex index of the edge
				meshIndices.push_back(edge->e);  // End vertex index of the edge
			}
		}


		// Store the mesh data (vertices and indices) in the map
		ChunkMeshData meshData;
		meshData.vertices.assign(meshVertices.begin(), meshVertices.end());
		meshData.indices.assign(meshIndices.begin(), meshIndices.end());
		meshData.normals = meshNormals;
		meshData.uvs = meshUVs;
		meshData.tangents = { {0.0f}, {0.0f}, {0.0f} };
		chunkMeshDataMap[i] = meshData;

		delete chunkMesh;
	}

	chunkDescs.resize(chunkCount);

	for (uint32_t i = 0; i < chunkCount; ++i) {
		// Get the polygonal mesh representation of the chunk
		Nv::Blast::Mesh* chunkMesh = fractureTool->createChunkMesh(i);

		// Compute volume and centroid
		NvcVec3 centroid;
		float volume = chunkMesh->getMeshVolumeAndCentroid(centroid);

		// Update chunkDesc with computed values
		chunkDescs[i].centroid[0] = centroid.x;
		chunkDescs[i].centroid[1] = centroid.y;
		chunkDescs[i].centroid[2] = centroid.z;
		chunkDescs[i].volume = volume;

		// Set parent index and flags
		if (i == 0) {
			// Root chunk (typically non-support)
			chunkDescs[i].parentChunkDescIndex = UINT32_MAX;
			chunkDescs[i].flags = NvBlastChunkDesc::NoFlags; // Root chunk is non-support
		}
		else {
			// Support chunks (children of root)
			chunkDescs[i].parentChunkDescIndex = 0; // Parent is the root chunk (index 0)
			chunkDescs[i].flags = NvBlastChunkDesc::SupportFlag; // Mark as support chunk
		}

		// Set user data (optional)
		chunkDescs[i].userData = i;

		printf("Chunk %d: flags = %d\n", i, chunkDescs[i].flags);

		delete chunkMesh;
	}

	// Simple distance threshold logic
	float adjacencyThreshold = 2.0f;

	std::vector<NvBlastBondDesc> bondDescs;

	for (uint32_t i = 1; i < chunkCount; ++i) {
		if (chunkDescs[i].flags & NvBlastChunkDesc::SupportFlag) {
			for (uint32_t j = i + 1; j < chunkCount; ++j) {
				if (chunkDescs[j].flags & NvBlastChunkDesc::SupportFlag) {

					// Calculate the distance between chunk i and chunk j
					float dx = chunkDescs[j].centroid[0] - chunkDescs[i].centroid[0];
					float dy = chunkDescs[j].centroid[1] - chunkDescs[i].centroid[1];
					float dz = chunkDescs[j].centroid[2] - chunkDescs[i].centroid[2];
					float distanceSquared = dx * dx + dy * dy + dz * dz;

					// Only create a bond if the chunks are within the adjacency threshold
					if (distanceSquared <= adjacencyThreshold * adjacencyThreshold) {
						NvBlastBondDesc bondDesc = {};

						bondDesc.chunkIndices[0] = i;
						bondDesc.chunkIndices[1] = j;

						bondDesc.bond.area = 1.0f;  // Placeholder value for bond area

						// Calculate bond centroid
						bondDesc.bond.centroid[0] = (chunkDescs[i].centroid[0] + chunkDescs[j].centroid[0]) / 2.0f;
						bondDesc.bond.centroid[1] = (chunkDescs[i].centroid[1] + chunkDescs[j].centroid[1]) / 2.0f;
						bondDesc.bond.centroid[2] = (chunkDescs[i].centroid[2] + chunkDescs[j].centroid[2]) / 2.0f;

						// Calculate bond normal
						float length = std::sqrt(distanceSquared);
						bondDesc.bond.normal[0] = dx / length;
						bondDesc.bond.normal[1] = dy / length;
						bondDesc.bond.normal[2] = dz / length;

						bondDesc.bond.userData = 0;

						bondDescs.push_back(bondDesc);

						printf("Bond created between support chunk %d and support chunk %d (distance: %.2f)\n", i, j, length);
					}
				}
			}
		}
	}

	// Fill in the TkAssetDesc structure
	Nv::Blast::TkAssetDesc tkAssetDesc = {};
	tkAssetDesc.chunkCount = chunkCount;
	tkAssetDesc.chunkDescs = chunkDescs.data();
	tkAssetDesc.bondCount = static_cast<uint32_t>(bondDescs.size());
	tkAssetDesc.bondDescs = bondDescs.data();

	// Set BondJointed flags corresponding to joints selected by the user (if applicable)
	std::vector<uint8_t> bondFlags(tkAssetDesc.bondCount, 0); // Initialize bond flags with no flags

	for (uint32_t i = 0; i < tkAssetDesc.bondCount; ++i)
	{
		bondFlags[i] |= Nv::Blast::TkAssetDesc::NoFlags;
	}

	tkAssetDesc.bondFlags = bondFlags.data();

	int test = framework->ensureAssetExactSupportCoverage(chunkDescs.data(), chunkCount);

	blastTkAsset = framework->createAsset(tkAssetDesc);  // Create a new TkAsset

	actorTk1 = framework->createActor(blastTkAsset);

	// Access the family associated with the TkActor
	Nv::Blast::TkFamily& family = actorTk1->getFamily();

	MyActorAndJointListener* myListener = new MyActorAndJointListener(this);

	family.addListener(*myListener);

	Nv::Blast::TkGroupDesc groupDesc;
	groupDesc.workerCount = 6;  // This can be set based on how many threads you want to use

	// Create the group using the framework
	group = framework->createGroup(groupDesc);

	group->addActor(*actorTk1);

	// Now use rootChunkMesh instead of cleanMesh
	std::vector<physx::PxVec3> rootChunkVertices(cleanMesh->getVerticesCount());
	for (uint32_t i = 0; i < cleanMesh->getVerticesCount(); ++i) {
		rootChunkVertices[i] = physx::PxVec3(cleanMesh->getVertices()[i].p.x, cleanMesh->getVertices()[i].p.y, cleanMesh->getVertices()[i].p.z);
		
	}

	std::vector<uint32_t> rootChunkIndices;
	// Process each facet for root chunk
	for (uint32_t i = 0; i < cleanMesh->getFacetCount(); ++i) {
		Nv::Blast::Facet* facet = const_cast<Nv::Blast::Facet*>(cleanMesh->getFacet(i));
		facet->userData = i;

		for (uint32_t j = 0; j < facet->edgesCount; ++j) {
			const Nv::Blast::Edge* edge = cleanMesh->getEdges() + facet->firstEdgeNumber + j;
			rootChunkIndices.push_back(edge->s);
		}
	}

	physx::PxTransform initialPose(physx::PxVec3(-100.0f, 10.0f, 10.0f)); 
	tracker.push_back(physics->createRigidDynamic(initialPose));
	tracker[0]->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, false);
	tracker[0]->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, false);

	if (!tracker[0]) {
		std::cerr << "Failed to create rigid dynamic actor." << std::endl;
		return;
	}

	//Create a convex mesh and shape for the actor
	createConvexMeshAndShape(tracker[0], rootChunkVertices, rootChunkIndices, true);

	// Ensure the actor has at least one shape
	if (tracker[0]->getNbShapes() == 0) {
		std::cerr << "Actor has no shapes attached." << std::endl;
		//aConvexActor->release();
		return;
	}

	// Set mass and inertia properties
	if (!physx::PxRigidBodyExt::updateMassAndInertia(*tracker[0], 0.1f)) {
		std::cerr << "Failed to update mass and inertia for actor." << std::endl;
		tracker[0]->release();
		return;
	}

	// FILTERING - COULD BE USED FOR INTERACTION WITH SPECIFIC KINDS OF MESHES
	//physx::PxFilterData filterData;
	//filterData.word0 = COLLISION_LAYER_SPHERE;
	//filterData.word1 = COLLISION_LAYER_PLANE;
	//shape->setSimulationFilterData(filterData);

	phyScene->addActor(*tracker[0]);

}


// Helper function to create a convex mesh and add it to a PhysX actor
void Physics::createConvexMeshAndShape(physx::PxRigidDynamic* actor, const std::vector<physx::PxVec3>& vertices, const std::vector<physx::PxU32>& indices, bool isInitialActor)
{
	physx::PxConvexMeshDesc convexDesc;
	convexDesc.points.count = static_cast<physx::PxU32>(vertices.size());
	convexDesc.points.stride = sizeof(physx::PxVec3);
	convexDesc.points.data = vertices.data();
	convexDesc.indices.count = static_cast<physx::PxU32>(indices.size());
	convexDesc.indices.stride = sizeof(physx::PxU32);
	convexDesc.indices.data = indices.data();
	convexDesc.flags = physx::PxConvexFlag::eCOMPUTE_CONVEX;

	physx::PxTolerancesScale scale;
	physx::PxCookingParams params(scale);
	physx::PxDefaultMemoryOutputStream buf;
	physx::PxConvexMeshCookingResult::Enum result;
	if (!PxCookConvexMesh(params, convexDesc, buf, &result)) {
		std::cerr << "Failed to cook convex mesh." << std::endl;
		return;
	}
	physx::PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
	physx::PxConvexMesh* convexMesh = physics->createConvexMesh(input);

	// Create a shape for each chunk
	if (convexMesh) {
		// Get mass properties of the convex mesh
		physx::PxReal mass;
		physx::PxMat33 localInertia;
		physx::PxVec3 localCenterOfMass;
		convexMesh->getMassInformation(mass, localInertia, localCenterOfMass);

		// Use mass information to decide material properties
		physx::PxReal staticFriction = (mass > 0.1f) ? 0.1f : 0.1f;
		physx::PxReal dynamicFriction = (mass > 0.1f) ? 1.1f : 0.1f;
		physx::PxReal restitution = (mass > 0.1f) ? 0.0f : 0.0f;

		// Create a material for this chunk based on its mass
		physx::PxMaterial* chunkMaterial = physics->createMaterial(staticFriction, dynamicFriction, restitution);

		if (!isInitialActor) {
			// Create the shape for the convex mesh using the material
			physx::PxShape* shape2 = physx::PxRigidActorExt::createExclusiveShape(
				*actor,
				physx::PxConvexMeshGeometry(convexMesh),
				*chunkMaterial
			);
		}
		else
		{
			// Create the shape for the convex mesh using the material
			shape = physx::PxRigidActorExt::createExclusiveShape(
				*actor,
				physx::PxConvexMeshGeometry(convexMesh),
				*chunkMaterial
			);
		}

		// More weight to make it more realistic, objects dont fly away!
		physx::PxRigidBodyExt::setMassAndUpdateInertia(*actor, mass);

		// Correct the center of mass for the new actor
		actor->setCMassLocalPose(physx::PxTransform(localCenterOfMass));

	}
}

// Render original actor or split actors
std::vector<std::shared_ptr<GameEntity>> Physics::GetDynamicFracturedEntities()
{
	if (renderToggle)
	{
		for (int i = 0; i < chunkActorsBuffer.size(); i++){
			renderEntities2[i].get()->GetTransform()->SetPosition(chunkActorsBuffer[i]->getGlobalPose().p.x + 90.0f, chunkActorsBuffer[i]->getGlobalPose().p.y + 1.5f, chunkActorsBuffer[i]->getGlobalPose().p.z - 15.0f);
			
			// pitch, yaw, roll
			DirectX::XMFLOAT3 pyr;
			// Extract the quaternion from PhysX
			physx::PxQuat quat = chunkActorsBuffer[i]->getGlobalPose().q;

			// Convert the quaternion to Euler angles (pitch, yaw, roll)
			DirectX::XMVECTOR quaternion = DirectX::XMVectorSet(quat.x, quat.y, quat.z, quat.w);
			DirectX::XMFLOAT4X4 rotationMatrix;
			DirectX::XMStoreFloat4x4(&rotationMatrix, DirectX::XMMatrixRotationQuaternion(quaternion));

			// Extract pitch, yaw, roll from the rotation matrix
			pyr.x = asinf(-rotationMatrix._32);  // Pitch
			pyr.y = atan2f(rotationMatrix._31, rotationMatrix._33);  // Yaw
			pyr.z = atan2f(rotationMatrix._12, rotationMatrix._22);  // Roll

			renderEntities2[i]->GetTransform()->SetRotation(pyr.x, pyr.y, pyr.z);

		}
		return renderEntities2;
	}
	else
	{
		for (int i = 0; i < renderEntities.size(); i++){
			renderEntities[i].get()->GetTransform()->SetPosition(tracker[i]->getGlobalPose().p.x + 90.0f, tracker[i]->getGlobalPose().p.y + 1.5f, tracker[i]->getGlobalPose().p.z - 15.0f);
		}
		return renderEntities;
	}
}

// Based on player's 'E' event
void Physics::ToggleGravity(bool isGravity)
{
	this->bGravity = isGravity;
}