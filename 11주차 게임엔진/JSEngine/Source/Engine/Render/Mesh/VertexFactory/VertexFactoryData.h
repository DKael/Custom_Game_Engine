#pragma once
/*
	VertexFactory 에서 사용할 데이터
	FRenderCommand 에 담아보내고, 구분해야할 데이터가 있는 경우 상속받아 새로 만듬 
	(e.g., FSkeletalVertexFactoryData, FStaticVertexFactoryData)
*/
class FVertexFactoryData
{
public:
    virtual ~FVertexFactoryData() = default;
};
