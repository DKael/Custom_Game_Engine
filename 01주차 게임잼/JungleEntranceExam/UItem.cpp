#include "UItem.h"
#include "itemShapeData.h"
#include "UBar.h"

UItem::UItem(const FVector& _Location, const FVector& _Direction, const float _Speed, const float _Scale, const FItemDesc& _ItemDesc)
{
	Location = _Location;
	Direction = _Direction;
	Speed = _Speed;
	Scale = _Scale;
	ItemDesc = _ItemDesc;
}

UItem::~UItem()
{
}

// 물리/이동 업데이트
void UItem::Update(float deltaTime)
{
	Location.x += (Speed * deltaTime * Direction.x);
	Location.y += (Speed * deltaTime * Direction.y);

	// TODO: 화면 밖으로 나가면 제거 처리
	if (Location.y < -1.0)
	{
		Destroy();
	}
}

// 렌더링 (상수 버퍼 업데이트)
void UItem::Render(URenderer& renderer)
{
	/*renderer.UpdateConstant(Location, FVector(Scale, Scale, 0));

	UINT NumVerticesItem = sizeof(item_rect_vertices) / sizeof(FVertexSimple);
	ID3D11Buffer* vertexBufferItem = renderer.CreateVertexBuffer(item_rect_vertices, sizeof(item_rect_vertices));
	renderer.RenderPrimitive(vertexBufferItem, NumVerticesItem);*/

	renderer.UpdateConstant(Location, FVector(Scale, Scale, 0));

	FVertexSimple* vertices = nullptr;
	UINT vertexCount = 0;
	UINT vertexBufferSize = 0;

	switch (ItemDesc.Type)
	{
	case EItemType::AddLife:
		vertices = item_add_life_vertices;
		vertexCount = sizeof(item_add_life_vertices) / sizeof(FVertexSimple);
		vertexBufferSize = sizeof(item_add_life_vertices);
		break;

	case EItemType::MultiBall:
		vertices = item_multi_ball_vertices;
		vertexCount = sizeof(item_multi_ball_vertices) / sizeof(FVertexSimple);
		vertexBufferSize = sizeof(item_multi_ball_vertices);
		break;

	case EItemType::ScoreBonus:
		vertices = item_score_bonus_vertices;
		vertexCount = sizeof(item_score_bonus_vertices) / sizeof(FVertexSimple);
		vertexBufferSize = sizeof(item_score_bonus_vertices);
		break;

	case EItemType::PaddleExpand:
		vertices = item_paddle_expand_vertices;
		vertexCount = sizeof(item_paddle_expand_vertices) / sizeof(FVertexSimple);
		vertexBufferSize = sizeof(item_paddle_expand_vertices);
		break;

	case EItemType::PaddleShrink:
		vertices = item_paddle_shrink_vertices;
		vertexCount = sizeof(item_paddle_shrink_vertices) / sizeof(FVertexSimple);
		vertexBufferSize = sizeof(item_paddle_shrink_vertices);
		break;

	case EItemType::PaddleSpeedUp:
		vertices = item_paddle_speed_up_vertices;
		vertexCount = sizeof(item_paddle_speed_up_vertices) / sizeof(FVertexSimple);
		vertexBufferSize = sizeof(item_paddle_speed_up_vertices);
		break;

	case EItemType::PaddleSpeedDown:
		vertices = item_paddle_speed_down_vertices;
		vertexCount = sizeof(item_paddle_speed_down_vertices) / sizeof(FVertexSimple);
		vertexBufferSize = sizeof(item_paddle_speed_down_vertices);
		break;

	case EItemType::BallSpeedUp:
		vertices = item_ball_speed_up_vertices;
		vertexCount = sizeof(item_ball_speed_up_vertices) / sizeof(FVertexSimple);
		vertexBufferSize = sizeof(item_ball_speed_up_vertices);
		break;

	case EItemType::BallSpeedDown:
		vertices = item_ball_speed_down_vertices;
		vertexCount = sizeof(item_ball_speed_down_vertices) / sizeof(FVertexSimple);
		vertexBufferSize = sizeof(item_ball_speed_down_vertices);
		break;

	case EItemType::AddBullet:
		vertices = item_add_bullet_vertices;
		vertexCount = sizeof(item_add_bullet_vertices) / sizeof(FVertexSimple);
		vertexBufferSize = sizeof(item_add_bullet_vertices);
		break;

	default:
		//vertices = item_paddle_shrink_vertices;
		//vertexCount = sizeof(item_paddle_shrink_vertices) / sizeof(FVertexSimple);
		//vertexBufferSize = sizeof(item_paddle_shrink_vertices);
		break;
	}

	ID3D11Buffer* vertexBufferItem = renderer.CreateVertexBuffer(vertices, vertexBufferSize);
	renderer.RenderPrimitive(vertexBufferItem, vertexCount);

	if (vertexBufferItem != nullptr)
	{
		vertexBufferItem->Release();
		vertexBufferItem = nullptr;
	}
}

// 벽 충돌 적용
void UItem::ApplyWallCollision()
{
	//Location.x = std::clamp(Location.x, leftBorder + XLength, rightBorder - XLength);
	//// 벽과 충돌 여부를 체크하고 벽에 닿았으면 멈춤
	//if (Location.x < leftBorder + XLength)
	//{
	//	Location.x = leftBorder + XLength;
	//}
	//if (Location.x > rightBorder - XLength)
	//{
	//	Location.x = rightBorder - XLength;
	//}
}

// 충격량 적용
void UItem::ApplyGravity(float deltaTime, const FVector& gravity)
{

}

bool UItem::CheckCollision(const UDiagram* Other)
{
	const UBar* PlayerBar{ dynamic_cast<const UBar*>(Other) };

	if (PlayerBar == nullptr)
		return false;

	// X축 범위 먼저 검사
	bool bInXRange =
		(PlayerBar->Location.x - (PlayerBar->XLength + Scale) <= Location.x) &&
		(Location.x <= PlayerBar->Location.x + (PlayerBar->XLength + Scale));

	if (!bInXRange)
		return false;

	// Player 1
	if (PlayerBar->PlayerNo == 0)
	{
		// 아이템의 아래쪽이 바의 위쪽에 닿았는가
		return (Location.y - Scale <= PlayerBar->Location.y + PlayerBar->YLength);
	}
	// Player 2
	else if (PlayerBar->PlayerNo == 1)
	{
		// 아이템의 위쪽이 바의 아래쪽에 닿았는가
		return (Location.y + Scale >= PlayerBar->Location.y - PlayerBar->YLength);
	}

	return false;
}

void UItem::SetScale(const float _Scale)
{
	Scale = _Scale;
}

bool UItem::IsAlive() const
{
	return bAlive;
}

void UItem::Destroy()
{
	bAlive = false;
}

void UItem::ApplyTo(IItemEffectReceiver* Receiver)
{
	if (!bAlive || Receiver == nullptr)
		return;

	ApplyEffect(Receiver);
	bCollected = true;
	Destroy();
}

void UItem::ApplyEffect(IItemEffectReceiver* Receiver)
{
	switch (ItemDesc.Type)
	{
	case EItemType::AddLife:
		Receiver->AddLife();
		break;

	case EItemType::MultiBall:
		Receiver->SpawnExtraBalls(ItemDesc.IntValue);
		break;

	case EItemType::ScoreBonus:
		Receiver->AddScore(ItemDesc.IntValue);
		break;

	case EItemType::PaddleExpand:
		Receiver->ModifyPaddleSize(ItemDesc.FloatValue);
		break;

	case EItemType::PaddleShrink:
		Receiver->ModifyPaddleSize(ItemDesc.FloatValue);
		break;

	case EItemType::PaddleSpeedUp:
		Receiver->ModifyPaddleSpeed(ItemDesc.FloatValue);
		break;

	case EItemType::PaddleSpeedDown:
		Receiver->ModifyPaddleSpeed(ItemDesc.FloatValue);
		break;

	case EItemType::BallSpeedUp:
		Receiver->ModifyBallSpeed(ItemDesc.FloatValue);
		break;

	case EItemType::BallSpeedDown:
		Receiver->ModifyBallSpeed(ItemDesc.FloatValue);
		break;

	case EItemType::AddBullet:
		Receiver->AddBullet(ItemDesc.IntValue);
		break;

	default:
		break;
	}
}

//EItemType UItem::GetType() const
//{
//	return ItemDesc.Type;
//}
//
//const FItemDesc& UItem::GetDesc() const
//{
//	return ItemDesc;
//}
