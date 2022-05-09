//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Equilibrium OpenGL ShaderAPI
//////////////////////////////////////////////////////////////////////////////////

#ifndef CGLRENDERLIB_H
#define CGLRENDERLIB_H

#include "../Shared/IRenderLibrary.h"
#include "ShaderAPIGL.h"

class ShaderAPIGL;

#ifdef PLAT_LINUX
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <X11/extensions/xf86vmode.h>

typedef XID GLXContextID;
typedef XID GLXPixmap;
typedef XID GLXDrawable;
typedef XID GLXPbuffer;
typedef XID GLXWindow;
typedef XID GLXFBConfigID;
typedef struct __GLXcontextRec* GLXContext;
typedef struct __GLXFBConfigRec* GLXFBConfig;
#endif

#ifdef PLAT_WIN
#define GL_CONTEXT HGLRC
#elif defined(PLAT_LINUX)
#define GL_CONTEXT GLXContext
#elif defined(PLAT_OSX)
#define GL_CONTEXT GLXContext
#endif // _WIN32

#define MAX_SHARED_CONTEXTS 1 // thank you, OpenGL, REALLY FUCKED ME with having multiple context, works perfect btw it crashes

class CGLRenderLib : public IRenderLibrary
{
	friend class			ShaderAPIGL;

public:

							CGLRenderLib();
							~CGLRenderLib();

	bool					InitCaps();

	bool					InitAPI(const shaderAPIParams_t &params);
	void					ExitAPI();
	void					ReleaseSwapChains();

	// frame begin/end
	void					BeginFrame();
	void					EndFrame(IEqSwapChain* swapChain = NULL);

	// renderer interface
	IShaderAPI*				GetRenderer() const;

	// sets backbuffer size for default swap chain
	void					SetBackbufferSize(int w, int h);

	// reports focus state
	void					SetFocused(bool inFocus);

	// changes fullscreen mode
	bool					SetWindowed(bool enabled);

	// speaks for itself
	bool					IsWindowed() const;

	// captures screenshot, outputs image to 'img'
	bool					CaptureScreenshot(CImage &img);

	// creates swap chain
	IEqSwapChain*			CreateSwapChain(void* window, bool windowed = true);

	// destroys a swapchain
	void					DestroySwapChain(IEqSwapChain* swapChain);

	// returns default swap chain
	IEqSwapChain*			GetDefaultSwapchain();

	// start capturing GL commands from specific thread id
	void					BeginAsyncOperation(uintptr_t threadId);
	void					EndAsyncOperation();
	bool					IsMainThread(uintptr_t threadId) const;
protected:

	void					InitSharedContexts();
	void					DestroySharedContexts();

	Array<IEqSwapChain*>	m_swapChains;
	uintptr_t				m_mainThreadId;
	bool					m_asyncOperationActive;

	GL_CONTEXT				m_glContext;
	GL_CONTEXT				m_glSharedContext;

#ifdef PLAT_WIN
	DISPLAY_DEVICEA			m_dispDevice;
	DEVMODEA				m_devMode;

	HDC						m_hdc;
	
	HWND					m_hwnd;

#elif defined(PLAT_LINUX)
    XF86VidModeModeInfo**	m_dmodes;
    Display*				m_display;
    XVisualInfo*            m_xvi;
    int						m_screen;
#elif defined(PLAT_OSX)
	CFArrayRef				m_dmodes;
	CFDictionaryRef			m_initialMode;
#endif // _WIN32

	int						m_width;
	int						m_height;

	bool					m_bResized;
	bool					m_windowed;
};
#endif //CGLRENDERLIB_H
