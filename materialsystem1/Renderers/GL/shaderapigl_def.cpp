//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Equilibrium OpenGL ShaderAPI
//////////////////////////////////////////////////////////////////////////////////

#include "core/core_common.h"
#include "shaderapigl_def.h"

const GLuint g_gl_internalFormats[FORMAT_COUNT] = {
	0,
	// Unsigned formats
#ifdef USE_GLES2
	GL_R8, //GL_INTENSITY8,
	GL_RG8, //GL_LUMINANCE8_ALPHA8,
#else
	GL_R8,
	GL_RG8,
#endif // USE_GLES2
	GL_RGB8,
	GL_RGBA8,
#ifdef USE_GLES2
	0x822A, // GL_EXT_texture_norm16 //GL_INTENSITY16,
	0x822C, //GL_LUMINANCE16_ALPHA16,
	0x8054,
	0x805B,
#else
	GL_R16,
	GL_RG16,
	GL_RGB16,
	GL_RGBA16,
#endif // USE_GLES2

	// Signed formats
	0,
	0,
	0,
	0,

	0,
	0,
	0,
	0,
#ifdef USE_GLES2
	// Float formats
	GL_R16F, // GL_INTENSITY16F_ARB,
	GL_RG16F, //GL_LUMINANCE_ALPHA16F_ARB,
	GL_RGB16F,
	GL_RGBA16F,
	GL_R32F, //GL_INTENSITY32F_ARB,
	GL_RG32F, //GL_LUMINANCE_ALPHA32F_ARB,
	GL_RGB32F,
	GL_RGBA32F,
#else
	// Float formats
	GL_R16F,
	GL_RG16F,
	GL_RGB16F,
	GL_RGBA16F,
	GL_R32F,
	GL_RG32F,
	GL_RGB32F,
	GL_RGBA32F,
#endif // USE_GLES2

	// Signed integer formats
	0, // GL_INTENSITY16I_EXT,
	0, // GL_LUMINANCE_ALPHA16I_EXT,
	0, // GL_RGB16I_EXT,
	0, // GL_RGBA16I_EXT,

	0, // GL_INTENSITY32I_EXT,
	0, // GL_LUMINANCE_ALPHA32I_EXT,
	0, // GL_RGB32I_EXT,
	0, // GL_RGBA32I_EXT,

	// Unsigned integer formats
	0, // GL_INTENSITY16UI_EXT,
	0, // GL_LUMINANCE_ALPHA16UI_EXT,
	0, // GL_RGB16UI_EXT,
	0, // GL_RGBA16UI_EXT,

	0, // GL_INTENSITY32UI_EXT,
	0, // GL_LUMINANCE_ALPHA32UI_EXT,
	0, // GL_RGB32UI_EXT,
	0, // GL_RGBA32UI_EXT,

	// Packed formats
	0, // RGBE8 not directly supported
	0, // GL_RGB9_E5,
	0, // GL_R11F_G11F_B10F,
#ifdef USE_GLES2
	0, // GL_RGB5,
#else
	GL_RGB5,
#endif // USE_GLES2
	GL_RGBA4,
	GL_RGB10_A2,

	// Depth formats
	GL_DEPTH_COMPONENT16,
	GL_DEPTH_COMPONENT24,
	GL_DEPTH24_STENCIL8,
	0, // GL_DEPTH_COMPONENT32F,

	// Compressed formats
#ifdef USE_GLES2
	0x83F0, //GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
	0x83F2, //GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
	0x83F3, //GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
	0, // ATI1N not yet supported
	0x8226,// GL_COMPRESSED_LUMINANCE_ALPHA,

	0x8D64, // which is GL_COMPRESSED_RGB8_ETC1
	GL_COMPRESSED_RGB8_ETC2,
	GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,
	GL_COMPRESSED_RGBA8_ETC2_EAC,
	0, //GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG,
	0, //GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG,
	0, //GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG,
	0, //GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG,
#else
	GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
	GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
	GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
	0, // ATI1N not yet supported
	GL_COMPRESSED_RG,			//COMPRESSED_LUMINANCE_ALPHA_3DC_ATI,

	0, // GL_COMPRESSED_RGB8_ETC1
	0, // GL_COMPRESSED_RGB8_ETC2,
	0, // GL_COMPRESSED_RGBA8_ETC2_EAC,
	0, // GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG,
	0, // GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG,
	0, // GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG,
	0, // GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG,
#endif // USE_GLES2
};

const GLuint g_gl_chanCountTypes[] = { 
	0,
	GL_RED, 
	GL_RG, 
	GL_RGB, 
	GL_RGBA 
};

const GLuint g_gl_chanTypePerFormat[] = {
	0,

	// Unsigned formats
	GL_UNSIGNED_BYTE,
	GL_UNSIGNED_BYTE,
	GL_UNSIGNED_BYTE,
	GL_UNSIGNED_BYTE,

	GL_UNSIGNED_SHORT,
	GL_UNSIGNED_SHORT,
	GL_UNSIGNED_SHORT,
	GL_UNSIGNED_SHORT,

	// Signed formats
	0,
	0,
	0,
	0,

	0,
	0,
	0,
	0,

	// Float formats
	GL_HALF_FLOAT,
	GL_HALF_FLOAT,
	GL_HALF_FLOAT,
	GL_HALF_FLOAT,

	GL_FLOAT,
	GL_FLOAT,
	GL_FLOAT,
	GL_FLOAT,

	// Signed integer formats
	0,
	0,
	0,
	0,

	0,
	0,
	0,
	0,

	// Unsigned integer formats
	0,
	0,
	0,
	0,

	0,
	0,
	0,
	0,

	// Packed formats
	0, // RGBE8 not directly supported
	0, // RGBE9E5 not supported
	0, // RG11B10F not supported
	GL_UNSIGNED_SHORT_5_6_5,
#ifdef USE_GLES2
	0, // GL_UNSIGNED_SHORT_4_4_4_4 ???,
#else
	GL_UNSIGNED_SHORT_4_4_4_4_REV,
#endif // USE_GLES2
	GL_UNSIGNED_INT_2_10_10_10_REV,

	// Depth formats
	GL_UNSIGNED_SHORT,
	GL_UNSIGNED_INT,
	GL_UNSIGNED_INT_24_8,
	0, // D32F not supported

	// Compressed formats
#ifdef USE_GLES2
	0, //GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
	0, //GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
	0, //GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
#else
	GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
	GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
	GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
#endif // USE_GLES2

	0,//(D3DFORMAT) '1ITA', // 3Dc 1 channel
	0,//(D3DFORMAT) '2ITA', // 3Dc 2 channels

#ifdef USE_GLES2
	0x8D64, // which is GL_COMPRESSED_RGB8_ETC1
	GL_COMPRESSED_RGB8_ETC2,
	GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,
	GL_COMPRESSED_RGBA8_ETC2_EAC,
	0, //GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG,
	0, //GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG,
	0, //GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG,
	0, //GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG,
#else
	0, // GL_COMPRESSED_RGB8_ETC1
	0, // GL_COMPRESSED_RGB8_ETC2,
	0, // GL_COMPRESSED_RGBA8_ETC2_EAC,
	0, // GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG,
	0, // GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG,
	0, // GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG,
	0, // GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG,
#endif // #ifdef USE_GLES2

};

const GLenum g_gl_texAddrModes[] = {
	GL_REPEAT,
	GL_CLAMP_TO_EDGE,
	GL_MIRRORED_REPEAT
};

const GLenum g_gl_blendingConsts[] = {
	GL_ZERO,
	GL_ONE,
	GL_SRC_COLOR,
	GL_ONE_MINUS_SRC_COLOR,
	GL_DST_COLOR,
	GL_ONE_MINUS_DST_COLOR,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA,
	GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA,
	GL_SRC_ALPHA_SATURATE,
};

const GLenum g_gl_blendingModes[] = {
	GL_FUNC_ADD,
	GL_FUNC_SUBTRACT,
	GL_FUNC_REVERSE_SUBTRACT,
	GL_MIN,
	GL_MAX,
};

const GLenum g_gl_depthConst[] = {
	GL_NEVER,
	GL_LESS,
	GL_EQUAL,
	GL_LEQUAL,
	GL_GREATER,
	GL_NOTEQUAL,
	GL_GEQUAL,
	GL_ALWAYS,
};

const GLenum g_gl_stencilConst[] = {
	GL_KEEP,
	GL_ZERO,
	GL_REPLACE,
	GL_INVERT,
	GL_INCR_WRAP,
	GL_DECR_WRAP,
	GL_MAX,
	GL_INCR,
	GL_DECR,
	GL_ALWAYS,
};

const GLenum g_gl_cullConst[] = {
	GL_NONE,
	GL_BACK,
	GL_FRONT,
};

#ifndef USE_GLES2
const GLenum g_gl_fillConst[] = {
	GL_FILL,
	GL_LINE,
	GL_POINT,
};
#endif // USE_GLES2

const GLenum g_gl_texMinFilters[] = {
	GL_NEAREST,
	GL_LINEAR,
	GL_LINEAR_MIPMAP_NEAREST,
	GL_LINEAR_MIPMAP_LINEAR,
	GL_LINEAR_MIPMAP_NEAREST,
	GL_LINEAR_MIPMAP_LINEAR,
};

const GLenum g_gl_primitiveType[] = {
	GL_TRIANGLES,
	GL_TRIANGLE_FAN,
	GL_TRIANGLE_STRIP,
	0, // GL_QUADS,
	GL_LINES,
	GL_LINE_STRIP,
	GL_LINE_LOOP,
	GL_POINTS,
};

const GLenum g_gl_bufferUsages[] = {
	GL_STREAM_DRAW,
	GL_STATIC_DRAW,
	GL_DYNAMIC_DRAW,
};

// map to EImageType
const GLenum g_gl_texTargetType[] = {
	GL_TEXTURE_2D,
	GL_TEXTURE_2D,
	GL_TEXTURE_3D,
	GL_TEXTURE_CUBE_MAP,
};