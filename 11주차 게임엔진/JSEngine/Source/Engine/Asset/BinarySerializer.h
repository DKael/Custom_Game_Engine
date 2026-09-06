#pragma once

#include "Core/CoreMinimal.h"

#include <fstream>

struct FStaticMesh;
struct FSkeletalMesh;
struct FPhysicsAssetData;
struct FSkeletonData;
struct FMatrix;
struct FTransform;
class UPhysicsAsset;
class USkeleton;
class UAnimSequence;

/*
 *	[주의사항]
 *	- Header나 Body 정보가 변경되면 반드시 Version을 바꿔야 합니다.
 */
struct FStaticMeshBinaryHeader
{
	uint32 MagicNumber = 0x4853454D;
	uint32 Version = 1;
	uint32 VertexCount = 0;
	uint32 IndexCount = 0;
	uint32 SectionCount = 0;
	uint32 SlotCount = 0;

	uint64 SourceFileWriteTime = 0;
};

struct FSkeletalMeshBinaryHeader
{
	uint32 MagicNumber = 0x534D4B53;	// 'SKMS' (Skeletal MeSh)
	uint32 Version = 4;					// v4: Skeleton/Animation/PhysicsAsset 분리
	uint32 VertexCount = 0;
	uint32 IndexCount = 0;
	uint32 SectionCount = 0;
	uint32 SlotCount = 0;
	uint32 BoneCount = 0;				// legacy v1~v3 embedded skeleton
	uint32 SocketCount = 0;				// mesh socket transitional data
	uint32 AnimationCount = 0;			// legacy v3 embedded animation

	uint64 SourceFileWriteTime = 0;
};

struct FSkeletonBinaryHeader
{
	uint32 MagicNumber = 0x4C454B53;		// 'SKEL'
	uint32 Version = 4;					// v4: compatible skeleton list added
	uint32 BoneCount = 0;
	uint32 SocketCount = 0;
	uint32 CurveMetaDataCount = 0;
	uint32 CompatibleSkeletonCount = 0;

	uint64 SourceFileWriteTime = 0;
};

struct FPhysicsAssetBinaryHeader
{
	uint32 MagicNumber = 0x53594850;		// 'PHYS'
	uint32 Version = 1;
	uint32 BodyCount = 0;
	uint32 ConstraintCount = 0;

	uint64 SourceFileWriteTime = 0;
};

class FBinarySerializer
{
public:
	bool SaveStaticMesh(const FString& BinaryPath, const FString& SourcePath, const FStaticMesh& Data);
	bool LoadStaticMesh(const FString& BinaryPath, FStaticMesh& OutData);

	bool SaveSkeleton(const FString& BinaryPath, const FString& SourcePath, const USkeleton& Data);
	bool LoadSkeleton(const FString& BinaryPath, USkeleton& OutData);

	bool SaveSkeletalMesh(const FString& BinaryPath, const FString& SourcePath, const FSkeletalMesh& Data);
	bool LoadSkeletalMesh(const FString& BinaryPath, FSkeletalMesh& OutData);
	bool LoadSkeletalMesh(
		const FString& BinaryPath,
		FSkeletalMesh& OutData,
		TArray<UAnimSequence*>* OutLegacyAnimationSequences);

	bool SavePhysicsAsset(const FString& BinaryPath, const FString& SourcePath, const UPhysicsAsset& Data);
	bool LoadPhysicsAsset(const FString& BinaryPath, UPhysicsAsset& OutData);

	//	Header Read + 검사 장치
	bool ReadStaticMeshHeader(const FString& BinaryPath, FStaticMeshBinaryHeader& OutHeader) const;
	bool ReadSkeletonHeader(const FString& BinaryPath, FSkeletonBinaryHeader& OutHeader) const;
	bool ReadSkeletalMeshHeader(const FString& BinaryPath, FSkeletalMeshBinaryHeader& OutHeader) const;
	bool ReadPhysicsAssetHeader(const FString& BinaryPath, FPhysicsAssetBinaryHeader& OutHeader) const;

private:
	
	void WriteInt32LE(std::ofstream& Out, int32 Value);
	void WriteUInt32LE(std::ofstream& Out, uint32 Value);
	void WriteUInt64LE(std::ofstream& Out, uint64 Value);
	void WriteFloatLE(std::ofstream& Out, float Value);
	
	bool ReadInt32LE(std::ifstream& In, int32& OutValue) const;
	bool ReadUInt32LE(std::ifstream& In, uint32& OutValue) const;
	bool ReadUInt64LE(std::ifstream& In, uint64& OutValue) const;
	bool ReadFloatLE(std::ifstream& In, float& OutValue) const;
	
	void WriteHeader(std::ofstream& Out, const FStaticMeshBinaryHeader& Header);
	bool ReadHeader(std::ifstream& In, FStaticMeshBinaryHeader& OutHeader) const;

	void WriteString(std::ofstream& Out, const FString& String);
	bool ReadString(std::ifstream& In, FString& OutString) const;

	void WriteIndexArray(std::ofstream& Out, const TArray<uint32>& Array);
	bool ReadIndexArray(std::ifstream& In, TArray<uint32>& OutArray) const;
	
	void WriteVertices(std::ofstream& Out, const FStaticMesh& Data);
	bool ReadVertices(std::ifstream& In, FStaticMesh& OutData, uint32 VertexCount) const;

	void WriteSections(std::ofstream& Out, const FStaticMesh& Data);
	bool ReadSections(std::ifstream& In, FStaticMesh& OutData, uint32 SectionCount) const;

	void WriteBounds(std::ofstream& Out, const FStaticMesh& Data);
	bool ReadBounds(std::ifstream& In, FStaticMesh& OutData) const;

	/* Skeletal Mesh */
	void WriteSkeletalHeader(std::ofstream& Out, const FSkeletalMeshBinaryHeader& Header);
	bool ReadSkeletalHeader(std::ifstream& In, FSkeletalMeshBinaryHeader& OutHeader) const;

	void WriteSkeletonHeader(std::ofstream& Out, const FSkeletonBinaryHeader& Header);
	bool ReadSkeletonHeader(std::ifstream& In, FSkeletonBinaryHeader& OutHeader) const;

	void WritePhysicsAssetHeader(std::ofstream& Out, const FPhysicsAssetBinaryHeader& Header);
	bool ReadPhysicsAssetHeader(std::ifstream& In, FPhysicsAssetBinaryHeader& OutHeader) const;

	void WriteMatrix4x4(std::ofstream& Out, const FMatrix& M);
	bool ReadMatrix4x4(std::ifstream& In, FMatrix& OutM) const;

	void WriteTransform(std::ofstream& Out, const FTransform& Transform);
	bool ReadTransform(std::ifstream& In, FTransform& OutTransform) const;

	void WriteSkeletonData(std::ofstream& Out, const FSkeletonData& Data);
	bool ReadSkeletonData(std::ifstream& In, FSkeletonData& OutData, const FSkeletonBinaryHeader& Header) const;

	void WriteSkeletalVertices(std::ofstream& Out, const FSkeletalMesh& Data);
	bool ReadSkeletalVertices(std::ifstream& In, FSkeletalMesh& OutData, uint32 VertexCount) const;

	void WriteSkeletalSections(std::ofstream& Out, const FSkeletalMesh& Data);
	bool ReadSkeletalSections(std::ifstream& In, FSkeletalMesh& OutData, uint32 SectionCount) const;

	void WriteBones(std::ofstream& Out, const FSkeletalMesh& Data);
	bool ReadBones(std::ifstream& In, FSkeletalMesh& OutData, uint32 BoneCount) const;

	void WriteSockets(std::ofstream& Out, const FSkeletalMesh& Data);
	bool ReadSockets(std::ifstream& In, FSkeletalMesh& OutData, uint32 SocketCount) const;

	void WriteAnimationSequences(std::ofstream& Out, const FSkeletalMesh& Data);
	bool ReadAnimationSequences(
		std::ifstream& In,
		FSkeletalMesh& OutData,
		uint32 AnimationCount,
		TArray<UAnimSequence*>* OutAnimationSequences = nullptr) const;

	void WriteSkeletalBounds(std::ofstream& Out, const FSkeletalMesh& Data);
	bool ReadSkeletalBounds(std::ifstream& In, FSkeletalMesh& OutData) const;

	void WritePhysicsAssetData(std::ofstream& Out, const FPhysicsAssetData& Data);
	bool ReadPhysicsAssetData(std::ifstream& In, FPhysicsAssetData& OutData, const FPhysicsAssetBinaryHeader& Header) const;
};
