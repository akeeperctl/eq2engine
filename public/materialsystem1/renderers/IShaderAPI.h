//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Equilibrium Middle-Level rendering API (ShaderAPI)
//				For high level look for the material system (Engine)
//				Contains FFP functions and Shader (FFP overriding programs)
//////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "imaging/textureformats.h"

#include "ShaderAPI_defs.h"

#include "IVertexBuffer.h"
#include "IVertexFormat.h"
#include "IIndexBuffer.h"
#include "ITexture.h"
#include "IOcclusionQuery.h"
#include "IRenderState.h"
#include "IShaderProgram.h"

#undef far
#undef near

struct KVSection;
class CImage;

enum ERHIWindowType : int
{
	RHI_WINDOW_HANDLE_UNKNOWN = -1,

	RHI_WINDOW_HANDLE_NATIVE_WINDOWS,
	RHI_WINDOW_HANDLE_NATIVE_X11,
	RHI_WINDOW_HANDLE_NATIVE_WAYLAND,
	RHI_WINDOW_HANDLE_NATIVE_COCOA,
	RHI_WINDOW_HANDLE_NATIVE_ANDROID,
};


// designed to be sent as windowHandle param
struct RenderWindowInfo
{
	enum Attribute
	{
		DISPLAY,
		WINDOW,
		SURFACE,
		TOPLEVEL
	};
	using GetterFunc = void*(*)(Attribute attrib);

	ERHIWindowType	windowType{ RHI_WINDOW_HANDLE_UNKNOWN };
	GetterFunc 		get{nullptr};
};

// shader api initializer
struct ShaderAPIParams
{
	RenderWindowInfo	windowInfo;
	ETextureFormat		screenFormat{ FORMAT_RGB8 };		// screen back buffer format

	int					screenRefreshRateHZ{ 60 };			// refresh rate in HZ
	int					multiSamplingMode{ 0 };				// multisampling
	int					depthBits{ 24 };					// bit depth for depth/stencil

	bool				verticalSyncEnabled{ false };		// vertical syncronization
};

//
// ShaderAPI interface
//
class IShaderAPI
{
public:
	virtual ~IShaderAPI() {}

	// initializes shader api.
	// Don't use this, this already called by materials->Init()
	virtual void				Init( const ShaderAPIParams &params ) = 0;

	// shutdowns shader api. Don't use this, this already called by materials->Shutdown()
	virtual void				Shutdown() = 0;

	// returns the parameters
	virtual const ShaderAPIParams&	GetParams() const = 0;

//-------------------------------------------------------------
// Renderer capabilities and information
//-------------------------------------------------------------

	virtual const ShaderAPICaps&	GetCaps() const = 0;

	virtual EShaderAPIType		GetShaderAPIClass() const = 0;
	virtual const char*			GetRendererName() const = 0;
	virtual void				PrintAPIInfo() const = 0;

//-------------------------------------------------------------
// Device statistics
//-------------------------------------------------------------

	virtual bool				IsDeviceActive() const = 0;

	virtual int					GetDrawCallsCount() const = 0;
	virtual int					GetDrawIndexedPrimitiveCallsCount() const = 0;
	virtual int					GetTrianglesCount() const = 0;

	virtual void				ResetCounters() = 0;

//-------------------------------------------------------------
// MT Synchronization
//-------------------------------------------------------------

	virtual void				Flush() = 0;
	virtual void				Finish() = 0;

//-------------------------------------------------------------
// Pipeline state layout
//-------------------------------------------------------------

	virtual IVertexFormat*		CreateVertexFormat( const char* name, ArrayCRef<VertexFormatDesc> formatDesc ) = 0;
	virtual IVertexFormat*		FindVertexFormat( const char* name ) const = 0;

//-------------------------------------------------------------
// Buffer objects
//-------------------------------------------------------------

	virtual IVertexBuffer*		CreateVertexBuffer(const BufferInfo& bufferInfo) = 0;
	virtual IIndexBuffer*		CreateIndexBuffer(const BufferInfo& bufferInfo) = 0;

	virtual void				DestroyVertexFormat(IVertexFormat* pFormat) = 0;
	virtual void				DestroyVertexBuffer(IVertexBuffer* pVertexBuffer) = 0;
	virtual void				DestroyIndexBuffer(IIndexBuffer* pIndexBuffer) = 0;

//-------------------------------------------------------------
// Shader resource management
//-------------------------------------------------------------

	virtual IShaderProgramPtr	FindShaderProgram(const char* pszName, const char* query = nullptr) = 0;

	virtual IShaderProgramPtr	CreateNewShaderProgram( const char* pszName, const char* query = nullptr) = 0;
	virtual void				FreeShaderProgram(IShaderProgram* pShaderProgram) = 0;

	virtual bool				LoadShadersFromFile(IShaderProgramPtr pShaderOutput, const char* pszFileNamePrefix, const char* extra = nullptr) = 0;
	virtual bool				CompileShadersFromStream(IShaderProgramPtr pShaderOutput, const ShaderProgCompileInfo& info, const char* extra = nullptr) = 0;

//-------------------------------------------------------------
// Occlusion query management
//-------------------------------------------------------------

	virtual IOcclusionQuery*	CreateOcclusionQuery() = 0;
	virtual void				DestroyOcclusionQuery(IOcclusionQuery* pQuery) = 0;

//-------------------------------------------------------------
// Texture resource managenent
//-------------------------------------------------------------

	// texture LOD uploading frequency
	virtual void				SetProgressiveTextureFrequency(int frames) = 0;
	virtual int					GetProgressiveTextureFrequency() const = 0;

	// returns default checkerboard texture
	virtual const ITexturePtr&	GetErrorTexture() const = 0;

	virtual ITexturePtr			FindOrCreateTexture(const char* pszName, bool& justCreated) = 0;
	virtual ITexturePtr			FindTexture(const char* pszName) = 0;

	virtual void				FreeTexture(ITexture* pTexture) = 0;

	// creates static texture from image (array for animated textures)
	virtual	ITexturePtr			CreateTexture(const ArrayCRef<CRefPtr<CImage>>& images, const SamplerStateParams& sampler, int flags = 0) = 0;

	// creates lockable texture
	virtual ITexturePtr			CreateProceduralTexture(const char* pszName,
														ETextureFormat nFormat,
														int width, int height,
														int depth = 1,
														int arraySize = 1,
														ETexFilterMode texFilter = TEXFILTER_NEAREST,
														ETexAddressMode textureAddress = TEXADDRESS_WRAP,
														int flags = 0,
														int dataSize = 0, const ubyte* data = nullptr
														) = 0;

	virtual ITexturePtr			CreateRenderTarget(const char* pszName,
													int width, int height,
													ETextureFormat nRTFormat,
													ETexFilterMode textureFilterType = TEXFILTER_LINEAR,
													ETexAddressMode textureAddress = TEXADDRESS_WRAP,
													ECompareFunc comparison = COMPFUNC_NEVER,
													int nFlags = 0
													) = 0;

//-------------------------------------------------------------
// Render states management
//-------------------------------------------------------------

	virtual IRenderState*		CreateBlendingState( const BlendStateParams &blendDesc ) = 0;
	virtual IRenderState*		CreateDepthStencilState( const DepthStencilStateParams &depthDesc ) = 0;
	virtual IRenderState*		CreateRasterizerState( const RasterizerStateParams &rasterDesc ) = 0;
	virtual void				DestroyRenderState( IRenderState* pShaderProgram, bool removeAllRefs = false) = 0;

//-------------------------------------------------------------
// Rasterizer and render state properties
//-------------------------------------------------------------

	virtual void				SetDepthRange( float near, float far ) = 0;
	virtual void				SetViewport( int x, int y, int w, int h ) = 0;
	virtual void				SetScissorRectangle(const IAARectangle& rect) = 0;

	virtual void				SetBlendingState( IRenderState* pBlending ) = 0;
	virtual void				SetDepthStencilState( IRenderState *pDepthStencilState ) = 0;
	virtual void				SetRasterizerState( IRenderState* pState ) = 0;

//-------------------------------------------------------------
// Vertex buffer object handling
//-------------------------------------------------------------

	virtual void				SetVertexFormat( IVertexFormat* pVertexFormat ) = 0;
	virtual void				SetVertexBuffer( IVertexBuffer* pVertexBuffer, int nStream, const intptr offset = 0 ) = 0;
	virtual void				SetIndexBuffer( IIndexBuffer *pIndexBuffer ) = 0;

	virtual void				ChangeVertexFormat( IVertexFormat* pVertexFormat ) = 0;
	virtual void				ChangeVertexBuffer( IVertexBuffer* pVertexBuffer,int nStream, const intptr offset = 0 ) = 0;
	virtual void				ChangeIndexBuffer( IIndexBuffer *pIndexBuffer ) = 0;

//-------------------------------------------------------------
// Shaders state operations
//-------------------------------------------------------------

	virtual void				SetShader(IShaderProgramPtr pShader) = 0;

    virtual void				SetShaderConstantRaw(int nameHash, const void *data, int nSize) = 0;

	template<typename ARRAY_TYPE>
	void						SetShaderConstantArray(int nameHash, const ARRAY_TYPE& arr);
	template<typename T> void	SetShaderConstant(int nameHash, const T* constant, int count = 1);
	template<typename T> void	SetShaderConstant(int nameHash, const T& constant);

	virtual void				SetTexture(int nameHash, const ITexturePtr& pTexture) = 0;
	virtual const ITexturePtr&	GetTextureAt( int level ) const = 0;

//-------------------------------------------------------------
// Render target state operations
//-------------------------------------------------------------

	// clear current rendertarget
	virtual void				Clear(	bool color,
										bool depth = true,
										bool stencil = true,
										const MColor& fillColor = color_black,
										float depthValue = 1.0f,
										int stencilValue = 0
										) = 0;

	virtual void				CopyFramebufferToTexture(const ITexturePtr& renderTarget) = 0;
	virtual void				CopyRendertargetToTexture(const ITexturePtr& srcTarget, const ITexturePtr& destTex, IAARectangle* srcRect = nullptr, IAARectangle* destRect = nullptr) = 0;
	
	virtual void				ChangeRenderTargetToBackBuffer() = 0;
	virtual void				ChangeRenderTarget(const ITexturePtr& renderTarget, int rtSlice = 0, const ITexturePtr& depthTarget = nullptr, int depthSlice = 0) = 0;
	virtual void				ChangeRenderTargets(ArrayCRef<ITexturePtr> renderTargets,
													ArrayCRef<int> rtSlice = nullptr,
													const ITexturePtr& depthTarget = nullptr,
													int depthSlice = 0) = 0;

	virtual void				ResizeRenderTarget(const ITexturePtr& renderTarget, int newWide, int newTall ) = 0;

//-------------------------------------------------------------
// Sending states to API
//-------------------------------------------------------------

	// reset states. Use RESET_TYPE flags. By default all states reset
	virtual void				Reset(int resetFlags = STATE_RESET_ALL) = 0;

	// applies all states
	virtual void				Apply() = 0;

	virtual void				ApplyTextures() = 0;
	virtual void				ApplySamplerState() = 0;
	virtual void				ApplyBlendState() = 0;
	virtual void				ApplyDepthState() = 0;
	virtual void				ApplyRasterizerState() = 0;
	virtual void				ApplyBuffers() = 0;
	virtual void				ApplyShaderProgram() = 0;
	virtual void				ApplyConstants() = 0;

//-------------------------------------------------------------
// Primitive drawing
//-------------------------------------------------------------

	virtual void				DrawIndexedPrimitives(EPrimTopology nType, int firstIndex, int indices, int firstVertex, int vertices, int baseVertex = 0) = 0;
	virtual void				DrawNonIndexedPrimitives(EPrimTopology nType, int firstVertex, int vertices) = 0;
};

template<typename ARRAY_TYPE>
inline void IShaderAPI::SetShaderConstantArray(int nameHash, const ARRAY_TYPE& arr)
{
	SetShaderConstantRaw(nameHash, arr.ptr(), sizeof(typename ARRAY_TYPE::ITEM) * arr.numElem());
}

template<typename T>
inline void IShaderAPI::SetShaderConstant(int nameHash, const T* constant, int count)
{
	SetShaderConstantRaw(nameHash, constant, sizeof(T) * count);
}

template<typename T>
inline void IShaderAPI::SetShaderConstant(int nameHash, const T& constant)
{
	SetShaderConstantRaw(nameHash, &constant, sizeof(T));
}

// it always external, declare new one in your app...
extern IShaderAPI* g_renderAPI;
