#include "ImGuiDrawData.h"

#ifndef IMGUI_DISABLE

void FImGuiDrawList::Update(ImDrawList* Source)
{
	VtxBuffer.swap(Source->VtxBuffer);
	IdxBuffer.swap(Source->IdxBuffer);
	CmdBuffer.swap(Source->CmdBuffer);
	Flags = Source->Flags;
}

void FImGuiDrawData::Update(ImDrawData* Source)
{
	bValid = Source->Valid;

	TotalIdxCount = Source->TotalIdxCount;
	TotalVtxCount = Source->TotalVtxCount;

	DrawLists.SetNum(Source->CmdLists.Size);

	for (int32 ListIdx = 0; ListIdx < DrawLists.Num(); ++ListIdx)
	{
		DrawLists[ListIdx].Update(Source->CmdLists[ListIdx]);
	}

	DisplayPos = Source->DisplayPos;
	DisplaySize = Source->DisplaySize;
	FrameBufferScale = Source->FramebufferScale;
}

#endif // #ifndef IMGUI_DISABLE
