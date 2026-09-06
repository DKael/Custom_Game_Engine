#include "Editor/UI/EditorMainPanel.h"

#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Editor/Animation/AnimationSequenceEditorDocument.h"
#include "Editor/Document/EditorDocument.h"
#include "Editor/EditorEngine.h"

void FEditorMainPanel::OpenAnimationSequenceAsset(const FString& SequencePath)
{
	const FString NormalizedPath = FPaths::Normalize(SequencePath);
	const FEditorTabId TabId = MakeAnimationSequenceTabId(NormalizedPath);

	if (FEditorDocument* ExistingDocument = FindDocumentByTabId(TabId))
	{
		EditorTabs.OpenOrFocusTab(TabId, ExistingDocument->GetTabLabel());
		EditorTabs.SetTabLabel(TabId, ExistingDocument->GetTabLabel());
		ActivateEditorTab(TabId);
		return;
	}

	UAnimSequence* Sequence = FResourceManager::Get().LoadAnimSequence(NormalizedPath);
	if (!Sequence)
	{
		if (EditorEngine)
		{
			EditorEngine->GetNotificationService().Error("Failed to open Animation Sequence: " + NormalizedPath);
		}
		return;
	}

	auto Document = std::make_unique<FAnimationSequenceEditorDocument>();
	if (!Document->Initialize(EditorEngine, NormalizedPath, Sequence))
	{
		if (EditorEngine)
		{
			EditorEngine->GetNotificationService().Error("Failed to initialize Animation Sequence editor: " + NormalizedPath);
		}
		return;
	}

	const FString TabLabel = Document->GetTabLabel();
	OpenDocument(std::move(Document));
	EditorTabs.OpenOrFocusTab(TabId, TabLabel);
	EditorTabs.SetTabLabel(TabId, TabLabel);
	ActivateEditorTab(TabId);
}
