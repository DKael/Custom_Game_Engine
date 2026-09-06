#pragma once

#include "Misc/ObjViewer/UI/ObjViewerWidget.h"
#include "Core/Logging/Stats.h"

/*
	렌더 큐 정렬 상태 디버그 위젯
	STATS 빌드에서만 표시되며, 패스별 커맨드 수 / 상태 변경 횟수 / GPU 시간을 보여준다.
*/
class FObjViewerRenderQueueWidget : public FObjViewerWidget
{
public:
	void Render(float DeltaTime) override;

private:
#if STATS
	bool bPaused = false;
	bool bShowOnlyActive = true; // 커맨드가 있는 패스만 표시
#endif
};
