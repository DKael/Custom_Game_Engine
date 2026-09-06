#include "UScene.h"
#include "UGameObject.h"
#include "URenderer.h"
#include "UDiagram.h"
UGameObject* UScene::SearchObject(unsigned int ID)
{
	for (UGameObject* Object : UGameObjectList) {
		if (Object->GetID() == ID) {
			return Object; // 조건에 맞으면 해당 객체 포인터를 즉시 반환
		}
	}
	//찾지못했다는 예외처리를 해야함
	return nullptr;
}

//씬에 있는 모든 오브젝트에 Update를 호출
void UScene::Update(float delta)
{
	for (UGameObject* Object : UGameObjectList) {
		Object->Update(delta);
	}
}

void UScene::RemoveObject(UGameObject* target)
{
	auto start = UGameObjectList.begin();

	for (int i = 0; i < UGameObjectList.size(); ++i) {
		if (UGameObjectList[i]->GetID() == target->GetID()) {
			UGameObjectList.erase(start + i);
		}
	}
	//찾지못했다는 예외처리를 해야함
}

