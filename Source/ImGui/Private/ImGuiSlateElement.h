#pragma once

#ifndef IMGUI_DISABLE

#if WITH_IMGUI_NATIVE_RENDERING

#include <Rendering/RenderingCommon.h>

#include "ImGuiDrawData.h"

class FTextureResource;

/// Renders ImGui draw data with native RHI draw calls, bypassing the Slate vertex batcher.
class FImGuiSlateElement : public ICustomSlateElement
{
public:
	virtual void Draw_RenderThread(FRDGBuilder& GraphBuilder, const FDrawPassInputs& Inputs) override;

	void SetDrawData_GameThread(ImDrawData* InDrawData);

	void SetGeometry_GameThread(const FGeometry& InGeometry);

private:
	FImGuiDrawData DrawData;

	FGeometry Geometry;

	TArray<const FTextureResource*> TextureResources;
};

#endif // #if WITH_IMGUI_NATIVE_RENDERING

#endif // #ifndef IMGUI_DISABLE
