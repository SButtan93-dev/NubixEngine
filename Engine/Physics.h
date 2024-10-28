#pragma once

#include "PxPhysicsAPI.h"
#include "cooking/PxCooking.h"
#include "pvd/PxPvdTransport.h"
#include "NvBlastTk.h"
#include "lowlevel/NvBlast.h"
#include "NvBlastTkFramework.h"
#include "extensions/authoring/NvBlastExtAuthoringFractureTool.h"
#include "extensions/authoring/NvBlastExtAuthoring.h"
#include "extensions/authoring/NvBlastExtAuthoringMeshCleaner.h"
#include "extensions/authoringCommon/NvBlastExtAuthoringMesh.h"
#include "extensions/authoring/NvBlastExtAuthoringBondGenerator.h"
#include "extensions/authoringCommon/NvBlastExtAuthoringPatternGenerator.h"
#include "extensions/shaders/NvBlastExtDamageShaders.h"
#include "extensions/authoringCommon/NvBlastExtAuthoringConvexMeshBuilder.h"
#include "shared/NvFoundation/NvErrorCallback.h"
#include "globals/NvBlastDebugRender.h"
#include "globals/NvBlastGlobals.h"
#include "Transform.h"
#include <iostream>
#include "GameEntity.h"
#include "Helpers.h"
#include <set>
#include <deque>
#include <algorithm>
#include <time.h>
#include <unordered_set>

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


#define COLLISION_LAYER_SPHERE 1
#define COLLISION_LAYER_PLANE 2


class Physics;
// Forward declaration
class MyActorAndJointListener;

// Define a struct to hold mesh data for each chunk
struct ChunkMeshData {
    std::vector<NvcVec3> vertices;
    std::vector<UINT> indices;
    std::vector<NvcVec3> normals;    // Normals
    std::vector<NvcVec2> uvs;        // UVs
    std::vector<NvcVec3> tangents;   // Tangents
};

class CustomRandomGenerator : public Nv::Blast::RandomGeneratorBase {
public:
    void seed(int32_t seed) override {
        std::srand(seed);
    }

    float getRandomValue() override {
        return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
    }
};

class MySimulationEventCallback : public physx::PxSimulationEventCallback {
public:
    MySimulationEventCallback(Physics* physics);

    void onConstraintBreak(physx::PxConstraintInfo*, physx::PxU32) override;
    void onWake(physx::PxActor**, physx::PxU32) override;
    void onSleep(physx::PxActor**, physx::PxU32) override;
    void onTrigger(physx::PxTriggerPair*, physx::PxU32) override;
    void onAdvance(const physx::PxRigidBody* const*, const physx::PxTransform*, const physx::PxU32) override;
    void onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs) override;

private:
    Physics* mPhysics;
};


class Physics {

private:
    physx::PxFoundation* foundation = nullptr;

    physx::PxPvd* pvd = nullptr;
    physx::PxDefaultCpuDispatcher* dispatcher = nullptr;
    physx::PxDefaultAllocator* phyAllocator = nullptr;
    physx::PxDefaultErrorCallback phyErrorLog;
    physx::PxPvdTransport* transport = nullptr;

    physx::PxRigidStatic* planeActor = nullptr;
    physx::PxShape* planeShape = nullptr;
    physx::PxRigidStatic* planeActor2 = nullptr;
    //physx::PxRigidDynamic* aConvexActor = nullptr;

    std::vector<physx::PxRigidDynamic*> tracker;

    physx::PxShape* shape;
    physx::PxShape* planeShape2 = nullptr;

    Nv::Blast::Mesh* mesh;
    Nv::Blast::Mesh* cleanMesh;

    std::vector<float> supportChunkHealths;

    MySimulationEventCallback* simulationEventCallback;

    // Step 3: Manually define chunks
    std::vector<NvBlastChunkDesc> chunkDescs;

    std::vector<physx::PxActor*> actorsToRemove;

    Nv::Blast::TkFramework* framework = nullptr;
    Nv::Blast::TkGroup* group = nullptr;
    Nv::Blast::TkActor* actorTk1 = nullptr;

    bool firstActorHit = true;
    bool bGravity = false;

public:

    Physics();

    physx::PxMaterial* planeMaterial = nullptr;
    physx::PxMaterial* meshMaterial = nullptr;

    // Buffer to store new actors to be added after the simulation step
    std::vector<physx::PxRigidDynamic*> chunkActorsBuffer;

    physx::PxVec3 firstContactPosition;

    physx::PxTransform actorImpactPos;

    std::vector<std::shared_ptr<GameEntity>> renderEntities;

    std::vector<std::shared_ptr<GameEntity>> renderEntities2;

    void RemoveQueuedActors();

    ~Physics();

    void InitPhysics();

    physx::PxPhysics* physics = nullptr;
    physx::PxScene* phyScene = nullptr;
    Nv::Blast::FractureTool* fractureTool;
    Nv::Blast::TkAsset* blastTkAsset;

    // Create a map to store chunk index to mesh data mapping
    std::unordered_map<uint32_t, ChunkMeshData> chunkMeshDataMap;

    bool isSplitApplied = false;

    bool renderToggle = false;

    void CreateScene();
    void CreateRigidBodyStatic();
    void CreateAttachPlaneRigidBodyStatic();
    void CreateRigidBodyStatic2();
    void CreateAttachPlaneRigidBodyStatic2();
    void AddFloorActors();

    void DoSimulation(float deltaTime);
    void FetchSimulationResults();
    void AddMeshToBlast(std::shared_ptr<GameEntity> e);

    void createConvexMeshAndShape(physx::PxRigidDynamic* actor, const std::vector<physx::PxVec3>& vertices, const std::vector<physx::PxU32>& indices, bool isInitialActor);
    void OnSphereHitGround(physx::PxActor* sphereActor, NvcVec3& impactPosition);

    void ToggleGravity(bool isGravity);

    void SetGravity(const physx::PxVec3& gravity);

   // void HandleSplitActor(Nv::Blast::TkActor* tkActor);

    physx::PxRigidDynamic* createPhysXActorFromBlastActor(Nv::Blast::TkActor* tkActor);

    std::vector<std::shared_ptr<GameEntity>> GetDynamicFracturedEntities();
};

class MyActorAndJointListener : public Nv::Blast::TkEventListener
{
public:
    MyActorAndJointListener(Physics* physics) : mPhysics(physics) {}

    // TkEventListener interface
    void receive(const Nv::Blast::TkEvent* events, uint32_t eventCount) override
    {
        // Events are batched into an event buffer.  Loop over all events:
        for (uint32_t i = 0; i < eventCount; ++i)
        {
            const Nv::Blast::TkEvent& event = events[i];

            // See TkEvent documentation for event types
            switch (event.type)
            {
            case Nv::Blast::TkSplitEvent::EVENT_TYPE:  // A TkActor has split into smaller actors
            {
                const Nv::Blast::TkSplitEvent* splitEvent = event.getPayload<Nv::Blast::TkSplitEvent>();  // Split event payload

                // The parent actor may no longer be valid.  Instead, we receive the information it held
                // which we need to update our app's representation (e.g. removal of the corresponding physics actor)
               // myRemoveActorFunction(splitEvent->parentData.family, splitEvent->parentData.index, splitEvent->parentData.userData);


                // The split event contains an array of "child" actors that came from the parent.  These are valid
                // TkActor pointers and may be used to create physics and graphics representations in our application
                for (uint32_t j = 0; j < splitEvent->numChildren; ++j){
                    Nv::Blast::TkActor* childTkActor = splitEvent->children[j];

                    // Get the number of visible chunks for this specific child actor
                    uint32_t chunkCount = childTkActor->getVisibleChunkCount();

                    // Retrieve the indices of the visible chunks
                    std::vector<uint32_t> chunkIndices(chunkCount);
                    childTkActor->getVisibleChunkIndices(chunkIndices.data(), chunkCount);

                    // Prepare vectors to store the combined vertices and indices for the merged chunks
                    std::vector<physx::PxVec3> combinedVertices;

                    std::vector<UINT> combinedIndices;

                    int vertexOffset = 0;

                    // Loop through each chunk to collect its vertices and indices
                    for (uint32_t chunkIndex : chunkIndices){
                        ChunkMeshData& meshData = mPhysics->chunkMeshDataMap[chunkIndex];

                        for (size_t i = 0; i < meshData.vertices.size(); ++i){
                            combinedVertices.push_back(physx::PxVec3(
                                meshData.vertices[i].x,
                                meshData.vertices[i].y,
                                meshData.vertices[i].z
                            ));
                        }
                        
                        for (size_t i = 0; i < meshData.indices.size(); ++i){

                            combinedIndices.push_back(meshData.indices[i] + vertexOffset);
                        }

                        // Update vertex offset for the next chunk
                        vertexOffset += static_cast<int>(meshData.vertices.size());         
                    }

                    // Now create a single PhysX actor using the combined vertices and indices
                    physx::PxRigidDynamic* combinedActor = mPhysics->physics->createRigidDynamic(
                       physx::PxTransform(mPhysics->actorImpactPos.p.x, mPhysics->actorImpactPos.p.y, mPhysics->actorImpactPos.p.z));  // Adjust transform as needed

                    // Use your existing function to create a convex mesh for the combined chunks
                    mPhysics->createConvexMeshAndShape(combinedActor, combinedVertices, combinedIndices, false);
                    mPhysics->chunkActorsBuffer.push_back(combinedActor);

                    // Extract PhysX convex mesh data (PhysX part)
                    physx::PxU32 shapeCount = combinedActor->getNbShapes();
                    physx::PxShape** shapes = new physx::PxShape * [shapeCount];
                    combinedActor->getShapes(shapes, shapeCount);

                    std::vector<Vertex> physxRenderVertices;  // Vertex structure for rendering PhysX convex mesh
                    std::vector<UINT> physxIndices;           // Indices for rendering PhysX convex mesh
                    physx::PxU32 physxVertexOffset = 0;       // Offset for PhysX vertices

                    for (physx::PxU32 i = 0; i < shapeCount; i++){
                        physx::PxShape* shape = shapes[i];
                        const physx::PxConvexMeshGeometry& convexMeshGeometry = static_cast<const physx::PxConvexMeshGeometry&>(shape->getGeometry());
                        physx::PxConvexMesh* convexMesh = convexMeshGeometry.convexMesh;

                        // Get vertices and indices from the PhysX convex mesh
                        const physx::PxVec3* convexVertices = convexMesh->getVertices();
                        const physx::PxU8* indexBuffer = convexMesh->getIndexBuffer();
                        physx::PxU32 numVerts = convexMesh->getNbVertices();
                        physx::PxU32 numPolygons = convexMesh->getNbPolygons();

                        for (physx::PxU32 p = 0; p < numPolygons; p++){
                            physx::PxHullPolygon polygonData;
                            convexMesh->getPolygonData(p, polygonData);
                            const physx::PxU8* faceIndices = indexBuffer + polygonData.mIndexBase;

                            for (physx::PxU32 v = 0; v < polygonData.mNbVerts; ++v){
                                Vertex vert;
                                physx::PxVec3 currentVertex = convexVertices[faceIndices[v]];

                                ChunkMeshData& meshData = mPhysics->chunkMeshDataMap[0]; // Access original chunk data

                                // Compare currentVertex with vertices in original chunk and find the closest one
                                float minDistance = FLT_MAX;
                                size_t closestVertexIndex = 0;

                                for (size_t origVIndex = 0; origVIndex < meshData.vertices.size(); ++origVIndex){
                                    const physx::PxVec3& originalVertex = physx::PxVec3(meshData.vertices[origVIndex].x, meshData.vertices[origVIndex].y, meshData.vertices[origVIndex].z);
                                    float distance = (originalVertex - currentVertex).magnitude();
                                    if (distance < minDistance)
                                    {
                                        minDistance = distance;
                                        closestVertexIndex = origVIndex;
                                    }
                                }

                                vert.Position = DirectX::XMFLOAT3(convexVertices[faceIndices[v]].x, convexVertices[faceIndices[v]].y, convexVertices[faceIndices[v]].z);
                                vert.Normal = DirectX::XMFLOAT3(polygonData.mPlane[0], polygonData.mPlane[1], polygonData.mPlane[2]);
                                vert.UV = DirectX::XMFLOAT2(meshData.uvs[closestVertexIndex].y, meshData.uvs[closestVertexIndex].x);
                                vert.Tangent = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);  // Tangent will be calculated

                                physxRenderVertices.push_back(vert);
                            }

                            // Triangulate the polygon
                            for (physx::PxU32 v = 2; v < polygonData.mNbVerts; ++v){
                                physxIndices.push_back(physxVertexOffset);
                                physxIndices.push_back(physxVertexOffset + v -1); // w/o DX12 widing order change in renderer
                                physxIndices.push_back(physxVertexOffset + v);
                            }

                            physxVertexOffset += polygonData.mNbVerts;
                        }
                    }

                    // Create the mesh for rendering the PhysX convex mesh
                    int physxVertexCount = static_cast<int>(physxRenderVertices.size());
                    int physxIndexCount = static_cast<int>(physxIndices.size());

                    std::shared_ptr<Mesh> physxMesh = std::make_shared<Mesh>(
                        &physxRenderVertices[0],  // Pointer to PhysX vertex data
                        physxVertexCount,         // Number of PhysX vertices
                        &physxIndices[0],         // Pointer to PhysX index data
                        physxIndexCount           // Number of PhysX indices
                    );
                    std::shared_ptr<GameEntity> entity = std::make_shared<GameEntity>(physxMesh, nullptr);

                    mPhysics->renderEntities2.push_back(entity);

                    mPhysics->renderToggle = true;
                }

                break;
            }
            case Nv::Blast::TkJointUpdateEvent::EVENT_TYPE:
            {
                const Nv::Blast::TkJointUpdateEvent* jointEvent = event.getPayload<Nv::Blast::TkJointUpdateEvent>();  // Joint update event payload

                // Joint events have three subtypes, see which one we have
                switch (jointEvent->subtype)
                {
                case Nv::Blast::TkJointUpdateEvent::External:
                    // myCreateJointFunction(jointEvent->joint);   // An internal joint has been "exposed" (now joins two different actors).  Create a physics joint.
                    break;
                case Nv::Blast::TkJointUpdateEvent::Changed:
                    // myUpdatejointFunction(jointEvent->joint);   // A joint's actors have changed, so we need to update its corresponding physics joint.
                    break;
                case Nv::Blast::TkJointUpdateEvent::Unreferenced:
                    //myDestroyJointFunction(jointEvent->joint);  // This joint is no longer referenced, so we may delete the corresponding physics joint.
                    break;
                }
            }

         // Unhandled:
            case Nv::Blast::TkFractureCommands::EVENT_TYPE:
            case Nv::Blast::TkFractureEvents::EVENT_TYPE:
            default:
                break;
            }
        }
    }

private:
    Physics* mPhysics;
};