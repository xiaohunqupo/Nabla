// Copyright (C) 2002-2012 Nikolaus Gebhardt / Thomas Alten
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __S_VIDEO_2_SOFTWARE_COMPILE_CONFIG_H_INCLUDED__
#define __S_VIDEO_2_SOFTWARE_COMPILE_CONFIG_H_INCLUDED__

#include "IrrCompileConfig.h"


// Generic Render Flags for burning's video rasterizer
// defined now in irrlicht compile config


#ifdef BURNINGVIDEO_RENDERER_BEAUTIFUL
	#define SOFTWARE_DRIVER_2_PERSPECTIVE_CORRECT
	#define SOFTWARE_DRIVER_2_SUBTEXEL
	#define SOFTWARE_DRIVER_2_BILINEAR
	#define SOFTWARE_DRIVER_2_LIGHTING
	#define SOFTWARE_DRIVER_2_USE_VERTEX_COLOR
	#define SOFTWARE_DRIVER_2_32BIT
	///#define SOFTWARE_DRIVER_2_MIPMAPPING //DevSH
	#define SOFTWARE_DRIVER_2_USE_WBUFFER
	#define SOFTWARE_DRIVER_2_TEXTURE_TRANSFORM
	#define SOFTWARE_DRIVER_2_TEXTURE_MAXSIZE		0
#endif


#ifdef BURNINGVIDEO_RENDERER_FAST
	#define SOFTWARE_DRIVER_2_PERSPECTIVE_CORRECT
	#define SOFTWARE_DRIVER_2_SUBTEXEL
	//#define SOFTWARE_DRIVER_2_BILINEAR
	//#define SOFTWARE_DRIVER_2_LIGHTING
	//#define SOFTWARE_DRIVER_2_USE_VERTEX_COLOR
	#define SOFTWARE_DRIVER_2_32BIT
	///#define SOFTWARE_DRIVER_2_MIPMAPPING //DevSH
	#define SOFTWARE_DRIVER_2_USE_WBUFFER
	#define SOFTWARE_DRIVER_2_TEXTURE_MAXSIZE		0
#endif

#ifdef BURNINGVIDEO_RENDERER_ULTRA_FAST
	#define BURNINGVIDEO_RENDERER_FAST

	//#define SOFTWARE_DRIVER_2_PERSPECTIVE_CORRECT
	#define SOFTWARE_DRIVER_2_SUBTEXEL
	//#define SOFTWARE_DRIVER_2_BILINEAR
	//#define SOFTWARE_DRIVER_2_LIGHTING
	//#define SOFTWARE_DRIVER_2_USE_VERTEX_COLOR
	//#define SOFTWARE_DRIVER_2_32BIT
	///#define SOFTWARE_DRIVER_2_MIPMAPPING //DevSH
	#define SOFTWARE_DRIVER_2_USE_WBUFFER
	#define SOFTWARE_DRIVER_2_TEXTURE_MAXSIZE		0
#endif

// Derivate flags

// texture format
#ifdef SOFTWARE_DRIVER_2_32BIT
	#define	BURNINGSHADER_COLOR_FORMAT	EF_B8G8R8A8_UNORM
#else
	#define	BURNINGSHADER_COLOR_FORMAT	EF_A1R5G5B5
#endif

// mip mapping
#if defined ( SOFTWARE_DRIVER_2_MIPMAPPING )
    #define SOFTWARE_DRIVER_2_MIPMAPPING_MAX		8
    #define SOFTWARE_DRIVER_2_MIPMAPPING_LOD_BIAS	0
#else
	#define SOFTWARE_DRIVER_2_MIPMAPPING_MAX		1
	#define SOFTWARE_DRIVER_2_MIPMAPPING_LOD_BIAS	0
#endif

#define SOFTWARE_DRIVER_2_MIPMAPPING_SCALE (8/SOFTWARE_DRIVER_2_MIPMAPPING_MAX)

#ifndef REALINLINE
	#ifdef _MSC_VER
		#define REALINLINE __forceinline
	#else
		#define REALINLINE inline
	#endif
#endif

#endif
