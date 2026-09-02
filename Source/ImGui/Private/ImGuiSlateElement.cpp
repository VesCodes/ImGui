#include "ImGuiSlateElement.h"

#ifndef IMGUI_DISABLE

#if WITH_IMGUI_NATIVE_RENDERING

#include <GlobalRenderResources.h>
#include <PipelineStateCache.h>
#include <RenderGraphBuilder.h>
#include <RHIStaticStates.h>
#include <TextureResource.h>
#include <Engine/Texture.h>

#include "ImGuiShaders.h"

BEGIN_SHADER_PARAMETER_STRUCT(FImGuiPassParameters, )
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FImGuiVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		constexpr size_t Stride = sizeof(ImDrawVert);

		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(ImDrawVert, pos), VET_Float2, 0, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(ImDrawVert, uv), VET_Float2, 1, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(ImDrawVert, col), VET_Color, 2, Stride));

		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

static TGlobalResource<FImGuiVertexDeclaration, FRenderResource::EInitPhase::Pre> GImGuiVertexDeclaration;

void FImGuiSlateElement::Draw_RenderThread(FRDGBuilder& GraphBuilder, const FDrawPassInputs& Inputs)
{
	if (!DrawData.bValid || DrawData.TotalVtxCount <= 0 || DrawData.TotalIdxCount <= 0 || Inputs.OutputTexture == nullptr)
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "ImGui");

	FImGuiPassParameters* PassParameters = GraphBuilder.AllocParameters<FImGuiPassParameters>();
	PassParameters->RenderTargets[0] = FRenderTargetBinding(Inputs.OutputTexture, ERenderTargetLoadAction::ELoad);

	const FIntPoint ViewExtent = Inputs.OutputTexture->Desc.Extent;
	const FIntRect ViewRect(0, 0, ViewExtent.X, ViewExtent.Y);

	FIntRect ClippingRect = Inputs.SceneViewRect;
	if (ClippingRect.IsEmpty())
	{
		ClippingRect.Max = ViewExtent;
	}

	// ImGui positions are defined relative to the viewport origin, so we need to offset everything by the widget's position.

	const FVector2f DrawOffset(Geometry.GetAccumulatedRenderTransform().GetTranslation() - FVector2D(DrawData.DisplayPos));

	const FVector2f ScissorOffset = DrawOffset + Inputs.ElementsOffset;

	// Slate has already baked ElementsOffset into ElementsMatrix, so only the scissor offset carries it.
	const FMatrix44f ProjectionMatrix = FTranslationMatrix44f::Make(FVector3f(DrawOffset, 0.0f)) * Inputs.ElementsMatrix;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("ImGui"),
		PassParameters,
		ERDGPassFlags::Raster,
		[this, ViewRect, ClippingRect, ScissorOffset, ProjectionMatrix](FRHICommandList& RHICmdList)
		{
			const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FImGuiVertexShader> VertexShader(ShaderMap);
			TShaderMapRef<FImGuiPixelShader> PixelShader(ShaderMap);

			RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GImGuiVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			{
				FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();

				VertexShader->SetProjectionMatrix(BatchedParameters, ProjectionMatrix);
				RHICmdList.SetBatchedShaderParameters(VertexShader.GetVertexShader(), BatchedParameters);
			}

			const FRHIBufferCreateDesc VertexBufferDesc = FRHIBufferCreateDesc::CreateVertex<ImDrawVert>(
				                                        TEXT("ImGuiVertexBuffer"), static_cast<uint32>(DrawData.TotalVtxCount))
			                                              .AddUsage(EBufferUsageFlags::Volatile)
			                                              .SetInitialState(ERHIAccess::VertexOrIndexBuffer)
			                                              .SetInitActionInitializer();

			TRHIBufferInitializer<ImDrawVert> VertexBufferInitializer = RHICmdList.CreateBufferInitializer(VertexBufferDesc);

			const FRHIBufferCreateDesc IndexBufferDesc = FRHIBufferCreateDesc::CreateIndex<ImDrawIdx>(
				                                       TEXT("ImGuiIndexBuffer"), static_cast<uint32>(DrawData.TotalIdxCount))
			                                             .AddUsage(EBufferUsageFlags::Volatile)
			                                             .SetInitialState(ERHIAccess::VertexOrIndexBuffer)
			                                             .SetInitActionInitializer();

			TRHIBufferInitializer<ImDrawIdx> IndexBufferInitializer = RHICmdList.CreateBufferInitializer(IndexBufferDesc);

			uint32 VertexOffset = 0;
			uint32 IndexOffset = 0;

			for (const FImGuiDrawList& DrawList : DrawData.DrawLists)
			{
				if (DrawList.VtxBuffer.Size > 0 && DrawList.IdxBuffer.Size > 0)
				{
					VertexBufferInitializer.WriteArray(VertexOffset, MakeConstArrayView(DrawList.VtxBuffer.Data, DrawList.VtxBuffer.Size));
					IndexBufferInitializer.WriteArray(IndexOffset, MakeConstArrayView(DrawList.IdxBuffer.Data, DrawList.IdxBuffer.Size));
				}

				// The draw loop below must increment the offsets in exactly the same way.

				VertexOffset += DrawList.VtxBuffer.Size;
				IndexOffset += DrawList.IdxBuffer.Size;
			}

			const FBufferRHIRef IndexBuffer = IndexBufferInitializer.Finalize();
			RHICmdList.SetStreamSource(0, VertexBufferInitializer.Finalize(), 0);

			FRHISamplerState* DefaultSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

			FRHITexture* CurrentTexture = nullptr;
			FIntRect CurrentScissorRect;

			int32 TextureResourceIdx = -1;

			VertexOffset = 0;
			IndexOffset = 0;

			for (int32 ListIdx = 0; ListIdx < DrawData.DrawLists.Num(); ++ListIdx)
			{
				const FImGuiDrawList& DrawList = DrawData.DrawLists[ListIdx];

				for (int32 CmdIdx = 0; CmdIdx < DrawList.CmdBuffer.Size; ++CmdIdx)
				{
					TextureResourceIdx += 1;

					const ImDrawCmd& DrawCmd = DrawList.CmdBuffer[CmdIdx];

					if (DrawCmd.UserCallback != nullptr || DrawCmd.ElemCount == 0)
					{
						continue;
					}

					// Clamped the way Slate clamps its own clipping rects, see SetSlateClipping().

					const int32 ScissorMinX = FMath::Clamp(FMath::FloorToInt(DrawCmd.ClipRect.x + ScissorOffset.X), ClippingRect.Min.X, ClippingRect.Max.X);
					const int32 ScissorMinY = FMath::Clamp(FMath::FloorToInt(DrawCmd.ClipRect.y + ScissorOffset.Y), ClippingRect.Min.Y, ClippingRect.Max.Y);
					const int32 ScissorMaxX = FMath::Clamp(FMath::CeilToInt(DrawCmd.ClipRect.z + ScissorOffset.X), ScissorMinX, ClippingRect.Max.X);
					const int32 ScissorMaxY = FMath::Clamp(FMath::CeilToInt(DrawCmd.ClipRect.w + ScissorOffset.Y), ScissorMinY, ClippingRect.Max.Y);

					if (ScissorMaxX <= ScissorMinX || ScissorMaxY <= ScissorMinY)
					{
						continue;
					}

					FRHITexture* Texture = GWhiteTexture->TextureRHI;
					FRHISamplerState* Sampler = DefaultSampler;
					bool bSRGB = false;

					if (TextureResources.IsValidIndex(TextureResourceIdx))
					{
						const FTextureResource* TextureResource = TextureResources[TextureResourceIdx];
						if (TextureResource != nullptr && TextureResource->TextureRHI.IsValid())
						{
							Texture = TextureResource->TextureRHI.GetReference();
							bSRGB = TextureResource->bSRGB;

							if (TextureResource->SamplerStateRHI.IsValid())
							{
								Sampler = TextureResource->SamplerStateRHI.GetReference();
							}
						}
					}

					if (CurrentTexture != Texture)
					{
						CurrentTexture = Texture;

						FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();
						PixelShader->SetTexture(BatchedParameters, Texture, Sampler, bSRGB);
						RHICmdList.SetBatchedShaderParameters(PixelShader.GetPixelShader(), BatchedParameters);
					}

					const FIntRect ScissorRect(ScissorMinX, ScissorMinY, ScissorMaxX, ScissorMaxY);
					if (CurrentScissorRect != ScissorRect)
					{
						CurrentScissorRect = ScissorRect;

						RHICmdList.SetScissorRect(true, ScissorMinX, ScissorMinY, ScissorMaxX, ScissorMaxY);
					}

					RHICmdList.DrawIndexedPrimitive(
						IndexBuffer, VertexOffset + DrawCmd.VtxOffset, 0,
						static_cast<uint32>(DrawList.VtxBuffer.Size) - DrawCmd.VtxOffset,
						IndexOffset + DrawCmd.IdxOffset, DrawCmd.ElemCount / 3, 1);
				}

				VertexOffset += DrawList.VtxBuffer.Size;
				IndexOffset += DrawList.IdxBuffer.Size;
			}

			RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
		});
}

void FImGuiSlateElement::SetDrawData_GameThread(ImDrawData* InDrawData)
{
	DrawData.Update(InDrawData);

	TextureResources.Reset();

	if (!DrawData.bValid || GExitPurge)
	{
		return;
	}

	for (int32 ListIdx = 0; ListIdx < DrawData.DrawLists.Num(); ++ListIdx)
	{
		const FImGuiDrawList& DrawList = DrawData.DrawLists[ListIdx];

		for (const ImDrawCmd& DrawCmd : DrawList.CmdBuffer)
		{
			UTexture* Texture = DrawCmd.GetTexID();
			FTextureResource* TextureResource = IsValid(Texture) ? Texture->GetResource() : nullptr;

			if (TextureResource != nullptr)
			{
				// Keep texture streaming from dropping the higher detail mips.
				TextureResource->LastRenderTime = FApp::GetCurrentTime();
			}

			TextureResources.Emplace(TextureResource);
		}
	}
}

void FImGuiSlateElement::SetGeometry_GameThread(const FGeometry& InGeometry)
{
	Geometry = InGeometry;
}

#endif // #if WITH_IMGUI_NATIVE_RENDERING

#endif // #ifndef IMGUI_DISABLE
