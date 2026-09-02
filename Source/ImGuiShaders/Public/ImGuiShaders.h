#pragma once

#ifndef IMGUI_DISABLE

#include <GlobalShader.h>

class FImGuiShadersModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
};

class IMGUISHADERS_API FImGuiVertexShader : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FImGuiVertexShader);

public:
	FImGuiVertexShader() = default;

	explicit FImGuiVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	void SetProjectionMatrix(FRHIBatchedShaderParameters& BatchedParameters, const FMatrix44f& InProjectionMatrix);

private:
	LAYOUT_FIELD(FShaderParameter, ProjectionMatrix);
};

class IMGUISHADERS_API FImGuiPixelShader : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FImGuiPixelShader);

public:
	FImGuiPixelShader() = default;

	explicit FImGuiPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	void SetTexture(FRHIBatchedShaderParameters& BatchedParameters, FRHITexture* InTexture, FRHISamplerState* InSampler, bool bInSrgb);

private:
	LAYOUT_FIELD(FShaderResourceParameter, Texture);
	LAYOUT_FIELD(FShaderResourceParameter, TextureSampler);
	LAYOUT_FIELD(FShaderParameter, bSrgb);
};

#endif // #ifndef IMGUI_DISABLE
