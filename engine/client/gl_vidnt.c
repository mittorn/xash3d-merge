/*
gl_vidnt.c - NT GL vid component
Copyright (C) 2010 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "common.h"
#include "client.h"
#include "gl_local.h"
#include "mod_local.h"
#include "input.h"

#define VID_AUTOMODE		"-1"
#define VID_DEFAULTMODE		2.0f
#define DISP_CHANGE_BADDUALVIEW	-6 // MSVC 6.0 doesn't
#define num_vidmodes		ARRAYSIZE( vidmode )
#define WINDOW_STYLE		(WS_OVERLAPPED|WS_BORDER|WS_SYSMENU|WS_CAPTION|WS_VISIBLE)
#define WINDOW_EX_STYLE		(0)
#define WINDOW_NAME			"Xash3D Window" // Half-Life

convar_t	*renderinfo;
convar_t	*gl_extensions;
convar_t	*gl_alphabits;
convar_t	*gl_stencilbits;
convar_t	*gl_ignorehwgamma;
convar_t	*gl_texture_anisotropy;
convar_t	*gl_texture_lodbias;
convar_t	*gl_texture_nearest;
convar_t	*gl_compress_textures;
convar_t	*gl_compensate_gamma_screenshots;
convar_t	*gl_keeptjunctions;
convar_t	*gl_showtextures;
convar_t	*gl_detailscale;
convar_t	*gl_swapInterval;
convar_t	*gl_check_errors;
convar_t	*gl_allow_static;
convar_t	*gl_allow_mirrors;
convar_t	*gl_wireframe;
convar_t	*gl_round_down;
convar_t	*gl_overview;
convar_t	*gl_max_size;
convar_t	*gl_picmip;
convar_t	*gl_skymip;
convar_t	*gl_finish;
convar_t	*gl_nosort;
convar_t	*gl_clear;
convar_t	*gl_test;

convar_t	*r_xpos;
convar_t	*r_ypos;
convar_t	*r_width;
convar_t	*r_height;
convar_t	*r_speeds;
convar_t	*r_fullbright;
convar_t	*r_norefresh;
convar_t	*r_lighting_extended;
convar_t	*r_lighting_modulate;
convar_t	*r_lighting_ambient;
convar_t	*r_detailtextures;
convar_t	*r_faceplanecull;
convar_t	*r_drawentities;
convar_t	*r_adjust_fov;
convar_t	*r_flaresize;
convar_t	*r_lefthand;
convar_t	*r_decals;
convar_t	*r_novis;
convar_t	*r_nocull;
convar_t	*r_lockpvs;
convar_t	*r_lockcull;
convar_t	*r_dynamic;
convar_t	*r_lightmap;
convar_t	*r_fastsky;

convar_t	*vid_displayfrequency;
convar_t	*vid_fullscreen;
convar_t	*vid_gamma;
convar_t	*vid_mode;

byte		*r_temppool;

ref_globals_t	tr;
glconfig_t	glConfig;
glstate_t		glState;
glwstate_t	glw_state;
uint		num_instances;

typedef enum
{
	rserr_ok,
	rserr_invalid_fullscreen,
	rserr_invalid_mode,
	rserr_unknown
} rserr_t;

typedef struct vidmode_s
{
	const char	*desc;
	int		width; 
	int		height;
	qboolean		wideScreen;
} vidmode_t;

vidmode_t vidmode[] =
{
{ "Mode  0: 4x3",	640,	480,	false	},
{ "Mode  1: 4x3",	800,	600,	false	},
{ "Mode  2: 4x3",	960,	720,	false	},
{ "Mode  3: 4x3",	1024,	768,	false	},
{ "Mode  4: 4x3",	1152,	864,	false	},
{ "Mode  5: 4x3",	1280,	800,	false	},
{ "Mode  6: 4x3",	1280,	960,	false	},
{ "Mode  7: 4x3",	1280,	1024,	false	},
{ "Mode  8: 4x3",	1600,	1200,	false	},
{ "Mode  9: 4x3",	2048,	1536,	false	},
{ "Mode 10: 16x9",	800,	480,	true	},
{ "Mode 11: 16x9",	856,	480,	true	},
{ "Mode 12: 16x9",	960,	540,	true	},
{ "Mode 13: 16x9",	1024,	576,	true	},
{ "Mode 14: 16x9",	1024,	600,	true	},
{ "Mode 15: 16x9",	1280,	720,	true	},
{ "Mode 16: 16x9",	1360,	768,	true	},
{ "Mode 17: 16x9",	1366,	768,	true	},
{ "Mode 18: 16x9",	1440,	900,	true	},
{ "Mode 19: 16x9",	1680,	1050,	true	},
{ "Mode 20: 16x9",	1920,	1080,	true	},
{ "Mode 21: 16x9",	1920,	1200,	true	},
{ "Mode 22: 16x9",	2560,	1600,	true	},
{ "Mode 23: 16x9",	1600,	 900,	true	},
};

static dllfunc_t opengl_110funcs[] =
{
{ "glClearColor"				, (void **)&pglClearColor },
{ "glClear"				, (void **)&pglClear },
{ "glAlphaFunc"				, (void **)&pglAlphaFunc },
{ "glBlendFunc"				, (void **)&pglBlendFunc },
{ "glCullFace"				, (void **)&pglCullFace },
{ "glDrawBuffer"				, (void **)&pglDrawBuffer },
{ "glReadBuffer"				, (void **)&pglReadBuffer },
{ "glAccum"				, (void **)&pglAccum },
{ "glEnable"				, (void **)&pglEnable },
{ "glDisable"				, (void **)&pglDisable },
{ "glEnableClientState"			, (void **)&pglEnableClientState },
{ "glDisableClientState"			, (void **)&pglDisableClientState },
{ "glGetBooleanv"				, (void **)&pglGetBooleanv },
{ "glGetDoublev"				, (void **)&pglGetDoublev },
{ "glGetFloatv"				, (void **)&pglGetFloatv },
{ "glGetIntegerv"				, (void **)&pglGetIntegerv },
{ "glGetError"				, (void **)&pglGetError },
{ "glGetString"				, (void **)&pglGetString },
{ "glFinish"				, (void **)&pglFinish },
{ "glFlush"				, (void **)&pglFlush },
{ "glClearDepth"				, (void **)&pglClearDepth },
{ "glDepthFunc"				, (void **)&pglDepthFunc },
{ "glDepthMask"				, (void **)&pglDepthMask },
{ "glDepthRange"				, (void **)&pglDepthRange },
{ "glFrontFace"				, (void **)&pglFrontFace },
{ "glDrawElements"				, (void **)&pglDrawElements },
{ "glDrawArrays"				, (void **)&pglDrawArrays },
{ "glColorMask"				, (void **)&pglColorMask },
{ "glIndexPointer"				, (void **)&pglIndexPointer },
{ "glVertexPointer"				, (void **)&pglVertexPointer },
{ "glNormalPointer"				, (void **)&pglNormalPointer },
{ "glColorPointer"				, (void **)&pglColorPointer },
{ "glTexCoordPointer"			, (void **)&pglTexCoordPointer },
{ "glArrayElement"				, (void **)&pglArrayElement },
{ "glColor3f"				, (void **)&pglColor3f },
{ "glColor3fv"				, (void **)&pglColor3fv },
{ "glColor4f"				, (void **)&pglColor4f },
{ "glColor4fv"				, (void **)&pglColor4fv },
{ "glColor3ub"				, (void **)&pglColor3ub },
{ "glColor4ub"				, (void **)&pglColor4ub },
{ "glColor4ubv"				, (void **)&pglColor4ubv },
{ "glTexCoord1f"				, (void **)&pglTexCoord1f },
{ "glTexCoord2f"				, (void **)&pglTexCoord2f },
{ "glTexCoord3f"				, (void **)&pglTexCoord3f },
{ "glTexCoord4f"				, (void **)&pglTexCoord4f },
{ "glTexCoord1fv"				, (void **)&pglTexCoord1fv },
{ "glTexCoord2fv"				, (void **)&pglTexCoord2fv },
{ "glTexCoord3fv"				, (void **)&pglTexCoord3fv },
{ "glTexCoord4fv"				, (void **)&pglTexCoord4fv },
{ "glTexGenf"				, (void **)&pglTexGenf },
{ "glTexGenfv"				, (void **)&pglTexGenfv },
{ "glTexGeni"				, (void **)&pglTexGeni },
{ "glVertex2f"				, (void **)&pglVertex2f },
{ "glVertex3f"				, (void **)&pglVertex3f },
{ "glVertex3fv"				, (void **)&pglVertex3fv },
{ "glNormal3f"				, (void **)&pglNormal3f },
{ "glNormal3fv"				, (void **)&pglNormal3fv },
{ "glBegin"				, (void **)&pglBegin },
{ "glEnd"					, (void **)&pglEnd },
{ "glLineWidth"				, (void**)&pglLineWidth },
{ "glPointSize"				, (void**)&pglPointSize },
{ "glMatrixMode"				, (void **)&pglMatrixMode },
{ "glOrtho"				, (void **)&pglOrtho },
{ "glRasterPos2f"				, (void **) &pglRasterPos2f },
{ "glFrustum"				, (void **)&pglFrustum },
{ "glViewport"				, (void **)&pglViewport },
{ "glPushMatrix"				, (void **)&pglPushMatrix },
{ "glPopMatrix"				, (void **)&pglPopMatrix },
{ "glPushAttrib"				, (void **)&pglPushAttrib },
{ "glPopAttrib"				, (void **)&pglPopAttrib },
{ "glLoadIdentity"				, (void **)&pglLoadIdentity },
{ "glLoadMatrixd"				, (void **)&pglLoadMatrixd },
{ "glLoadMatrixf"				, (void **)&pglLoadMatrixf },
{ "glMultMatrixd"				, (void **)&pglMultMatrixd },
{ "glMultMatrixf"				, (void **)&pglMultMatrixf },
{ "glRotated"				, (void **)&pglRotated },
{ "glRotatef"				, (void **)&pglRotatef },
{ "glScaled"				, (void **)&pglScaled },
{ "glScalef"				, (void **)&pglScalef },
{ "glTranslated"				, (void **)&pglTranslated },
{ "glTranslatef"				, (void **)&pglTranslatef },
{ "glReadPixels"				, (void **)&pglReadPixels },
{ "glDrawPixels"				, (void **)&pglDrawPixels },
{ "glStencilFunc"				, (void **)&pglStencilFunc },
{ "glStencilMask"				, (void **)&pglStencilMask },
{ "glStencilOp"				, (void **)&pglStencilOp },
{ "glClearStencil"				, (void **)&pglClearStencil },
{ "glIsEnabled"				, (void **)&pglIsEnabled },
{ "glIsList"				, (void **)&pglIsList },
{ "glIsTexture"				, (void **)&pglIsTexture },
{ "glTexEnvf"				, (void **)&pglTexEnvf },
{ "glTexEnvfv"				, (void **)&pglTexEnvfv },
{ "glTexEnvi"				, (void **)&pglTexEnvi },
{ "glTexParameterf"				, (void **)&pglTexParameterf },
{ "glTexParameterfv"			, (void **)&pglTexParameterfv },
{ "glTexParameteri"				, (void **)&pglTexParameteri },
{ "glHint"				, (void **)&pglHint },
{ "glPixelStoref"				, (void **)&pglPixelStoref },
{ "glPixelStorei"				, (void **)&pglPixelStorei },
{ "glGenTextures"				, (void **)&pglGenTextures },
{ "glDeleteTextures"			, (void **)&pglDeleteTextures },
{ "glBindTexture"				, (void **)&pglBindTexture },
{ "glTexImage1D"				, (void **)&pglTexImage1D },
{ "glTexImage2D"				, (void **)&pglTexImage2D },
{ "glTexSubImage1D"				, (void **)&pglTexSubImage1D },
{ "glTexSubImage2D"				, (void **)&pglTexSubImage2D },
{ "glCopyTexImage1D"			, (void **)&pglCopyTexImage1D },
{ "glCopyTexImage2D"			, (void **)&pglCopyTexImage2D },
{ "glCopyTexSubImage1D"			, (void **)&pglCopyTexSubImage1D },
{ "glCopyTexSubImage2D"			, (void **)&pglCopyTexSubImage2D },
{ "glScissor"				, (void **)&pglScissor },
{ "glGetTexImage"				, (void **)&pglGetTexImage },
{ "glGetTexEnviv"				, (void **)&pglGetTexEnviv },
{ "glPolygonOffset"				, (void **)&pglPolygonOffset },
{ "glPolygonMode"				, (void **)&pglPolygonMode },
{ "glPolygonStipple"			, (void **)&pglPolygonStipple },
{ "glClipPlane"				, (void **)&pglClipPlane },
{ "glGetClipPlane"				, (void **)&pglGetClipPlane },
{ "glShadeModel"				, (void **)&pglShadeModel },
{ "glGetTexLevelParameteriv"			, (void **)&pglGetTexLevelParameteriv },
{ "glGetTexLevelParameterfv"			, (void **)&pglGetTexLevelParameterfv },
{ "glFogfv"				, (void **)&pglFogfv },
{ "glFogf"				, (void **)&pglFogf },
{ "glFogi"				, (void **)&pglFogi },
{ NULL					, NULL }
};

static dllfunc_t drawrangeelementsfuncs[] =
{
{ "glDrawRangeElements"			, (void **)&pglDrawRangeElements },
{ NULL					, NULL }
};

static dllfunc_t drawrangeelementsextfuncs[] =
{
{ "glDrawRangeElementsEXT"			, (void **)&pglDrawRangeElementsEXT },
{ NULL					, NULL }
};

static dllfunc_t debugoutputfuncs[] =
{
{ "glDebugMessageControlARB"			, (void **)&pglDebugMessageControlARB },
{ "glDebugMessageInsertARB"			, (void **)&pglDebugMessageInsertARB },
{ "glDebugMessageCallbackARB"			, (void **)&pglDebugMessageCallbackARB },
{ "glGetDebugMessageLogARB"			, (void **)&pglGetDebugMessageLogARB },
{ NULL					, NULL }
};

static dllfunc_t multitexturefuncs[] =
{
{ "glMultiTexCoord1fARB"			, (void **)&pglMultiTexCoord1f },
{ "glMultiTexCoord2fARB"			, (void **)&pglMultiTexCoord2f },
{ "glMultiTexCoord3fARB"			, (void **)&pglMultiTexCoord3f },
{ "glMultiTexCoord4fARB"			, (void **)&pglMultiTexCoord4f },
{ "glActiveTextureARB"			, (void **)&pglActiveTexture },
{ "glActiveTextureARB"			, (void **)&pglActiveTextureARB },
{ "glClientActiveTextureARB"			, (void **)&pglClientActiveTexture },
{ "glClientActiveTextureARB"			, (void **)&pglClientActiveTextureARB },
{ NULL					, NULL }
};

static dllfunc_t texture3dextfuncs[] =
{
{ "glTexImage3DEXT"	  			, (void **)&pglTexImage3D },
{ "glTexSubImage3DEXT"			, (void **)&pglTexSubImage3D },
{ "glCopyTexSubImage3DEXT"			, (void **)&pglCopyTexSubImage3D },
{ NULL					, NULL }
};

static dllfunc_t shaderobjectsfuncs[] =
{
{ "glDeleteObjectARB"			, (void **)&pglDeleteObjectARB },
{ "glGetHandleARB"				, (void **)&pglGetHandleARB },
{ "glDetachObjectARB"			, (void **)&pglDetachObjectARB },
{ "glCreateShaderObjectARB"			, (void **)&pglCreateShaderObjectARB },
{ "glShaderSourceARB"			, (void **)&pglShaderSourceARB },
{ "glCompileShaderARB"			, (void **)&pglCompileShaderARB },
{ "glCreateProgramObjectARB"			, (void **)&pglCreateProgramObjectARB },
{ "glAttachObjectARB"			, (void **)&pglAttachObjectARB },
{ "glLinkProgramARB"			, (void **)&pglLinkProgramARB },
{ "glUseProgramObjectARB"			, (void **)&pglUseProgramObjectARB },
{ "glValidateProgramARB"			, (void **)&pglValidateProgramARB },
{ "glUniform1fARB"				, (void **)&pglUniform1fARB },
{ "glUniform2fARB"				, (void **)&pglUniform2fARB },
{ "glUniform3fARB"				, (void **)&pglUniform3fARB },
{ "glUniform4fARB"				, (void **)&pglUniform4fARB },
{ "glUniform1iARB"				, (void **)&pglUniform1iARB },
{ "glUniform2iARB"				, (void **)&pglUniform2iARB },
{ "glUniform3iARB"				, (void **)&pglUniform3iARB },
{ "glUniform4iARB"				, (void **)&pglUniform4iARB },
{ "glUniform1fvARB"				, (void **)&pglUniform1fvARB },
{ "glUniform2fvARB"				, (void **)&pglUniform2fvARB },
{ "glUniform3fvARB"				, (void **)&pglUniform3fvARB },
{ "glUniform4fvARB"				, (void **)&pglUniform4fvARB },
{ "glUniform1ivARB"				, (void **)&pglUniform1ivARB },
{ "glUniform2ivARB"				, (void **)&pglUniform2ivARB },
{ "glUniform3ivARB"				, (void **)&pglUniform3ivARB },
{ "glUniform4ivARB"				, (void **)&pglUniform4ivARB },
{ "glUniformMatrix2fvARB"			, (void **)&pglUniformMatrix2fvARB },
{ "glUniformMatrix3fvARB"			, (void **)&pglUniformMatrix3fvARB },
{ "glUniformMatrix4fvARB"			, (void **)&pglUniformMatrix4fvARB },
{ "glGetObjectParameterfvARB"			, (void **)&pglGetObjectParameterfvARB },
{ "glGetObjectParameterivARB"			, (void **)&pglGetObjectParameterivARB },
{ "glGetInfoLogARB"				, (void **)&pglGetInfoLogARB },
{ "glGetAttachedObjectsARB"			, (void **)&pglGetAttachedObjectsARB },
{ "glGetUniformLocationARB"			, (void **)&pglGetUniformLocationARB },
{ "glGetActiveUniformARB"			, (void **)&pglGetActiveUniformARB },
{ "glGetUniformfvARB"			, (void **)&pglGetUniformfvARB },
{ "glGetUniformivARB"			, (void **)&pglGetUniformivARB },
{ "glGetShaderSourceARB"			, (void **)&pglGetShaderSourceARB },
{ "glVertexAttribPointerARB"			, (void **)&pglVertexAttribPointerARB },
{ "glEnableVertexAttribArrayARB"		, (void **)&pglEnableVertexAttribArrayARB },
{ "glDisableVertexAttribArrayARB"		, (void **)&pglDisableVertexAttribArrayARB },
{ "glBindAttribLocationARB"			, (void **)&pglBindAttribLocationARB },
{ "glGetActiveAttribARB"			, (void **)&pglGetActiveAttribARB },
{ "glGetAttribLocationARB"			, (void **)&pglGetAttribLocationARB },
{ "glVertexAttrib2f"			, (void **)&pglVertexAttrib2fARB },
{ "glVertexAttrib2fv"			, (void **)&pglVertexAttrib2fvARB },
{ "glVertexAttrib3fv"			, (void **)&pglVertexAttrib3fvARB },
{ NULL					, NULL }
};

static dllfunc_t vbofuncs[] =
{
{ "glBindBufferARB"				, (void **)&pglBindBufferARB },
{ "glDeleteBuffersARB"			, (void **)&pglDeleteBuffersARB },
{ "glGenBuffersARB"				, (void **)&pglGenBuffersARB },
{ "glIsBufferARB"				, (void **)&pglIsBufferARB },
{ "glMapBufferARB"				, (void **)&pglMapBufferARB },
{ "glUnmapBufferARB"			, (void **)&pglUnmapBufferARB },
{ "glBufferDataARB"				, (void **)&pglBufferDataARB },
{ "glBufferSubDataARB"			, (void **)&pglBufferSubDataARB },
{ NULL					, NULL}
};

static dllfunc_t vaofuncs[] =
{
{ "glBindVertexArray"			, (void **)&pglBindVertexArray },
{ "glDeleteVertexArrays"			, (void **)&pglDeleteVertexArrays },
{ "glGenVertexArrays"			, (void **)&pglGenVertexArrays },
{ "glIsVertexArray"				, (void **)&pglIsVertexArray },
{ NULL					, NULL }
};

static dllfunc_t arbfbofuncs[] =
{
{ "glIsRenderbuffer"			, (void **)&pglIsRenderbuffer },
{ "glBindRenderbuffer"			, (void **)&pglBindRenderbuffer },
{ "glDeleteRenderbuffers"			, (void **)&pglDeleteRenderbuffers },
{ "glGenRenderbuffers"			, (void **)&pglGenRenderbuffers },
{ "glRenderbufferStorage"			, (void **)&pglRenderbufferStorage },
{ "glRenderbufferStorageMultisample"		, (void **)&pglRenderbufferStorageMultisample }, // not in GL_EXT_framebuffer_object
{ "glGetRenderbufferParameteriv"		, (void **)&pglGetRenderbufferParameteriv },
{ "glIsFramebuffer"				, (void **)&pglIsFramebuffer },
{ "glBindFramebuffer"			, (void **)&pglBindFramebuffer },
{ "glDeleteFramebuffers"			, (void **)&pglDeleteFramebuffers },
{ "glGenFramebuffers"			, (void **)&pglGenFramebuffers },
{ "glCheckFramebufferStatus"			, (void **)&pglCheckFramebufferStatus },
{ "glFramebufferTexture1D"			, (void **)&pglFramebufferTexture1D },
{ "glFramebufferTexture2D"			, (void **)&pglFramebufferTexture2D },
{ "glFramebufferTexture3D"			, (void **)&pglFramebufferTexture3D },
{ "glFramebufferTextureLayer"			, (void **)&pglFramebufferTextureLayer }, // not in GL_EXT_framebuffer_object
{ "glFramebufferRenderbuffer"			, (void **)&pglFramebufferRenderbuffer },
{ "glGetFramebufferAttachmentParameteriv"	, (void **)&pglGetFramebufferAttachmentParameteriv },
{ "glBlitFramebuffer"			, (void **)&pglBlitFramebuffer }, // not in GL_EXT_framebuffer_object
{ "glGenerateMipmap"			, (void **)&pglGenerateMipmap },
{ NULL					, NULL}
};

static dllfunc_t extfbofuncs[] =
{
{ "glIsRenderbufferEXT"			, (void **)&pglIsRenderbuffer },
{ "glBindRenderbufferEXT"			, (void **)&pglBindRenderbuffer },
{ "glDeleteRenderbuffersEXT"			, (void **)&pglDeleteRenderbuffers },
{ "glGenRenderbuffersEXT"			, (void **)&pglGenRenderbuffers },
{ "glRenderbufferStorageEXT"			, (void **)&pglRenderbufferStorage },
{ "glGetRenderbufferParameterivEXT"		, (void **)&pglGetRenderbufferParameteriv },
{ "glIsFramebufferEXT"			, (void **)&pglIsFramebuffer },
{ "glBindFramebufferEXT"			, (void **)&pglBindFramebuffer },
{ "glDeleteFramebuffersEXT"			, (void **)&pglDeleteFramebuffers },
{ "glGenFramebuffersEXT"			, (void **)&pglGenFramebuffers },
{ "glCheckFramebufferStatusEXT"		, (void **)&pglCheckFramebufferStatus },
{ "glFramebufferTexture1DEXT"			, (void **)&pglFramebufferTexture1D },
{ "glFramebufferTexture2DEXT"			, (void **)&pglFramebufferTexture2D },
{ "glFramebufferTexture3DEXT"			, (void **)&pglFramebufferTexture3D },
{ "glFramebufferRenderbufferEXT"		, (void **)&pglFramebufferRenderbuffer },
{ "glGetFramebufferAttachmentParameterivEXT"	, (void **)&pglGetFramebufferAttachmentParameteriv },
{ "glGenerateMipmapEXT"			, (void **)&pglGenerateMipmap },
{ NULL, NULL}
};

static dllfunc_t occlusionfunc[] =
{
{ "glGenQueriesARB"				, (void **)&pglGenQueriesARB },
{ "glDeleteQueriesARB"			, (void **)&pglDeleteQueriesARB },
{ "glIsQueryARB"				, (void **)&pglIsQueryARB },
{ "glBeginQueryARB"				, (void **)&pglBeginQueryARB },
{ "glEndQueryARB"				, (void **)&pglEndQueryARB },
{ "glGetQueryivARB"				, (void **)&pglGetQueryivARB },
{ "glGetQueryObjectivARB"			, (void **)&pglGetQueryObjectivARB },
{ "glGetQueryObjectuivARB"			, (void **)&pglGetQueryObjectuivARB },
{ NULL					, NULL }
};

static dllfunc_t texturecompressionfuncs[] =
{
{ "glCompressedTexImage3DARB"			, (void **)&pglCompressedTexImage3DARB },
{ "glCompressedTexImage2DARB"			, (void **)&pglCompressedTexImage2DARB },
{ "glCompressedTexImage1DARB"			, (void **)&pglCompressedTexImage1DARB },
{ "glCompressedTexSubImage3DARB"		, (void **)&pglCompressedTexSubImage3DARB },
{ "glCompressedTexSubImage2DARB"		, (void **)&pglCompressedTexSubImage2DARB },
{ "glCompressedTexSubImage1DARB"		, (void **)&pglCompressedTexSubImage1DARB },
{ "glGetCompressedTexImageARB"		, (void **)&pglGetCompressedTexImage },
{ NULL					, NULL }
};

static dllfunc_t drawbuffersfuncs[] =
{
{ "glDrawBuffersARB"			, (void **)&pglDrawBuffersARB },
{ NULL					, NULL }
};

static dllfunc_t wgl_funcs[] =
{
{ "wglSwapBuffers"				, (void **)&pwglSwapBuffers },
{ "wglCreateContext"			, (void **)&pwglCreateContext },
{ "wglDeleteContext"			, (void **)&pwglDeleteContext },
{ "wglMakeCurrent"				, (void **)&pwglMakeCurrent },
{ "wglGetCurrentContext"			, (void **)&pwglGetCurrentContext },
{ NULL					, NULL }
};

static dllfunc_t wglproc_funcs[] =
{
{ "wglGetProcAddress"			, (void **)&pwglGetProcAddress },
{ NULL, NULL }
};

static dllfunc_t wglswapintervalfuncs[] =
{
{ "wglSwapIntervalEXT"			, (void **)&pwglSwapIntervalEXT },
{ NULL, NULL }
};

static dllfunc_t wglgetextensionsstring[] =
{
{ "wglGetExtensionsStringEXT"			, (void **)&pwglGetExtensionsStringEXT },
{ NULL, NULL }
};

dll_info_t opengl_dll = { "opengl32.dll", wgl_funcs, true };

/*
========================
DebugCallback

For ARB_debug_output
========================
*/
static void CALLBACK GL_DebugOutput( GLuint source, GLuint type, GLuint id, GLuint severity, GLint length, const GLcharARB *message, GLvoid *userParam )
{
	switch( type )
	{
	case GL_DEBUG_TYPE_ERROR_ARB:
		if( host.developer < D_ERROR )	// "-dev 2"
			return;
		Con_Printf( "^1OpenGL Error:^7 %s\n", message );
		break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:
		if( host.developer < D_WARN )		// "-dev 3"
			return;
		Con_Printf( "^3OpenGL Warning:^7 %s\n", message );
		break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:
		if( host.developer < D_WARN )		// "-dev 3"
			return;
		Con_Printf( "^3OpenGL Warning:^7 %s\n", message );
		break;
	case GL_DEBUG_TYPE_PORTABILITY_ARB:
		if( host.developer < D_REPORT )	// "-dev 4"
			return;
		Con_Printf( "^3OpenGL Warning:^7 %s\n", message );
		break;
	case GL_DEBUG_TYPE_PERFORMANCE_ARB:
		if( host.developer < D_REPORT )	// "-dev 4"
			return;
		Con_Printf( "OpenGL Notify: %s\n", message );
		break;
	case GL_DEBUG_TYPE_OTHER_ARB:
	default:	if( host.developer < D_NOTE )		// "-dev 5"
			return;
		Con_Printf( "OpenGL: %s\n", message );
		break;
	}
}

/*
=================
GL_SetExtension
=================
*/
void GL_SetExtension( int r_ext, int enable )
{
	if( r_ext >= 0 && r_ext < GL_EXTCOUNT )
		glConfig.extension[r_ext] = enable ? GL_TRUE : GL_FALSE;
	else MsgDev( D_ERROR, "GL_SetExtension: invalid extension %d\n", r_ext );
}

/*
=================
GL_Support
=================
*/
qboolean GL_Support( int r_ext )
{
	if( r_ext >= 0 && r_ext < GL_EXTCOUNT )
		return glConfig.extension[r_ext] ? true : false;
	MsgDev( D_ERROR, "GL_Support: invalid extension %d\n", r_ext );

	return false;		
}

/*
=================
GL_MaxTextureUnits
=================
*/
int GL_MaxTextureUnits( void )
{
	if( GL_Support( GL_SHADER_GLSL100_EXT ))
		return Q_min( Q_max( glConfig.max_texture_coords, glConfig.max_teximage_units ), MAX_TEXTURE_UNITS );
	return glConfig.max_texture_units;
}

/*
=================
GL_GetProcAddress
=================
*/
void *GL_GetProcAddress( const char *name )
{
	void	*p = NULL;

	if( pwglGetProcAddress != NULL )
		p = (void *)pwglGetProcAddress( name );
	if( !p ) p = (void *)Sys_GetProcAddress( &opengl_dll, name );

	return p;
}

/*
=================
GL_CheckExtension
=================
*/
void GL_CheckExtension( const char *name, const dllfunc_t *funcs, const char *cvarname, int r_ext )
{
	const dllfunc_t	*func;
	convar_t		*parm;
	const char	*extensions_string;

	MsgDev( D_NOTE, "GL_CheckExtension: %s ", name );

	if( cvarname )
	{
		// system config disable extensions
		parm = Cvar_Get( cvarname, "1", CVAR_GLCONFIG, va( "enable or disable %s", name ));

		if( parm->integer == 0 || ( gl_extensions->integer == 0 && r_ext != GL_OPENGL_110 ))
		{
			MsgDev( D_NOTE, "- disabled\n" );
			GL_SetExtension( r_ext, 0 );
			return; // nothing to process at
		}
		GL_SetExtension( r_ext, 1 );
	}

	extensions_string = glConfig.extensions_string; 

	if( name[0] == 'W' && name[1] == 'G' && name[2] == 'L' && glConfig.wgl_extensions_string != NULL )
		extensions_string = glConfig.wgl_extensions_string;

	if(( name[2] == '_' || name[3] == '_' ) && !Q_strstr( extensions_string, name ))
	{
		GL_SetExtension( r_ext, false );	// update render info
		MsgDev( D_NOTE, "- ^1failed\n" );
		return;
	}

	// clear exports
	for( func = funcs; func && func->name; func++ )
		*func->func = NULL;

	GL_SetExtension( r_ext, true ); // predict extension state
	for( func = funcs; func && func->name != NULL; func++ )
	{
		// functions are cleared before all the extensions are evaluated
		if(!(*func->func = (void *)GL_GetProcAddress( func->name )))
			GL_SetExtension( r_ext, false ); // one or more functions are invalid, extension will be disabled
	}

	if( GL_Support( r_ext ))
		MsgDev( D_NOTE, "- ^2enabled\n" );
	else MsgDev( D_NOTE, "- ^1failed\n" );
}

/*
===============
GL_BuildGammaTable
===============
*/
void GL_BuildGammaTable( void )
{
	double	invGamma;
	double	div;
	int	i, v;

	invGamma = 1.0 / bound( 0.5, vid_gamma->value, 2.3 );
	div = (double) 1.0 / 255.5;

	memcpy( glState.gammaRamp, glState.stateRamp, sizeof( glState.gammaRamp ));
	
	for( i = 0; i < 256; i++ )
	{
		v = (int)(65535.0 * pow(((double)i + 0.5 ) * div, invGamma ) + 0.5 );
		glState.gammaRamp[i+0]   = ((word)bound( 0, v, 65535 ));
		glState.gammaRamp[i+256] = ((word)bound( 0, v, 65535 ));
		glState.gammaRamp[i+512] = ((word)bound( 0, v, 65535 ));
	}
}

/*
===============
GL_UpdateGammaRamp
===============
*/
void GL_UpdateGammaRamp( void )
{
	if( !glConfig.deviceSupportsGamma ) return;

	GL_BuildGammaTable();

	SetDeviceGammaRamp( glw_state.hDC, glState.gammaRamp );
}

/*
===============
GL_UpdateSwapInterval
===============
*/
void GL_UpdateSwapInterval( void )
{
	// disable VSync while level is loading
	if( cls.state < ca_active )
	{
		if( pwglSwapIntervalEXT )
			pwglSwapIntervalEXT( 0 );
		gl_swapInterval->modified = true;
	}
	else if( gl_swapInterval->modified )
	{
		gl_swapInterval->modified = false;

		if( pwglSwapIntervalEXT )
			pwglSwapIntervalEXT( gl_swapInterval->integer );
	}
}

/*
===============
GL_SetDefaultTexState
===============
*/
static void GL_SetDefaultTexState( void )
{
	int	i;

	memset( glState.currentTextures, -1, MAX_TEXTURE_UNITS * sizeof( *glState.currentTextures ));
	memset( glState.texCoordArrayMode, 0, MAX_TEXTURE_UNITS * sizeof( *glState.texCoordArrayMode ));
	memset( glState.genSTEnabled, 0, MAX_TEXTURE_UNITS * sizeof( *glState.genSTEnabled ));

	for( i = 0; i < MAX_TEXTURE_UNITS; i++ )
	{
		glState.currentTextureTargets[i] = GL_NONE;
		glState.texIdentityMatrix[i] = true;
	}
}

/*
===============
GL_SetDefaultState
===============
*/
static void GL_SetDefaultState( void )
{
	memset( &glState, 0, sizeof( glState ));
	GL_SetDefaultTexState ();
}

/*
===============
GL_ContextError
===============
*/
static void GL_ContextError( void )
{
	DWORD error = GetLastError();

	if( error == ( 0xc0070000|ERROR_INVALID_VERSION_ARB ))
		MsgDev( D_ERROR, "Unsupported OpenGL context version (%s).\n", "2.0" );
	else if( error == ( 0xc0070000|ERROR_INVALID_PROFILE_ARB ))
		MsgDev( D_ERROR, "Unsupported OpenGL profile (%s).\n", "compat" );
	else if( error == ( 0xc0070000|ERROR_INVALID_OPERATION ))
		MsgDev( D_ERROR, "wglCreateContextAttribsARB returned invalid operation.\n" );
	else if( error == ( 0xc0070000|ERROR_DC_NOT_FOUND ))
		MsgDev( D_ERROR, "wglCreateContextAttribsARB returned dc not found.\n" );
	else if( error == ( 0xc0070000|ERROR_INVALID_PIXEL_FORMAT ))
		MsgDev( D_ERROR, "wglCreateContextAttribsARB returned dc not found.\n" );
	else if( error == ( 0xc0070000|ERROR_NO_SYSTEM_RESOURCES ))
		MsgDev( D_ERROR, "wglCreateContextAttribsARB ran out of system resources.\n" );
	else if( error == ( 0xc0070000|ERROR_INVALID_PARAMETER ))
		MsgDev( D_ERROR, "wglCreateContextAttribsARB reported invalid parameter.\n" );
	else MsgDev( D_ERROR, "Unknown error creating an OpenGL (%s) Context.\n", "2.0" );
}

/*
=================
GL_CreateContext
=================
*/
qboolean GL_CreateContext( void )
{
	HGLRC hBaseRC;

	glw_state.extended = false;

	if(!( glw_state.hGLRC = pwglCreateContext( glw_state.hDC )))
		return GL_DeleteContext();

	if(!( pwglMakeCurrent( glw_state.hDC, glw_state.hGLRC )))
		return GL_DeleteContext();

	if( !Sys_CheckParm( "-gldebug" ) || host.developer < D_INFO ) // debug bit the kills perfomance
		return true;

	pwglCreateContextAttribsARB = GL_GetProcAddress( "wglCreateContextAttribsARB" );

	if( pwglCreateContextAttribsARB != NULL )
	{
		int attribs[] =
		{
		WGL_CONTEXT_MAJOR_VERSION_ARB, 2,
		WGL_CONTEXT_MINOR_VERSION_ARB, 0,
		WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,         
//		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
		0
		};

		hBaseRC = glw_state.hGLRC; // backup
		glw_state.hGLRC = NULL;

		if( !( glw_state.hGLRC = pwglCreateContextAttribsARB( glw_state.hDC, NULL, attribs )))
		{
			glw_state.hGLRC = hBaseRC;
			GL_ContextError();
			return true; // just use old context
		}

		if(!( pwglMakeCurrent( glw_state.hDC, glw_state.hGLRC )))
		{
			pwglDeleteContext( glw_state.hGLRC );
			glw_state.hGLRC = hBaseRC;
			GL_ContextError();
			return true;
		}

		MsgDev( D_NOTE, "GL_CreateContext: using extended context\n" );
		pwglDeleteContext( hBaseRC );	// release first context
		glw_state.extended = true;
	}

	return true;
}

/*
=================
GL_UpdateContext
=================
*/
qboolean GL_UpdateContext( void )
{
	if(!( pwglMakeCurrent( glw_state.hDC, glw_state.hGLRC )))
		return GL_DeleteContext();

	return true;
}

/*
=================
GL_DeleteContext

always return false
=================
*/
qboolean GL_DeleteContext( void )
{
	if( pwglMakeCurrent )
		pwglMakeCurrent( NULL, NULL );

	if( glw_state.hGLRC )
	{
		if( pwglDeleteContext )
			pwglDeleteContext( glw_state.hGLRC );
		glw_state.hGLRC = NULL;
	}

	if( glw_state.hDC )
	{
		ReleaseDC( host.hWnd, glw_state.hDC );
		glw_state.hDC = NULL;
	}

	return false;
}

/*
=================
VID_ChoosePFD
=================
*/
static int VID_ChoosePFD( PIXELFORMATDESCRIPTOR *pfd, int colorBits, int alphaBits, int depthBits, int stencilBits )
{
	int	pixelFormat = 0;

	MsgDev( D_NOTE, "VID_ChoosePFD( color %i, alpha %i, depth %i, stencil %i )\n", colorBits, alphaBits, depthBits, stencilBits );

	// Fill out the PFD
	pfd->nSize = sizeof (PIXELFORMATDESCRIPTOR);
	pfd->nVersion = 1;
	pfd->dwFlags = PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
	pfd->iPixelType = PFD_TYPE_RGBA;

	pfd->cColorBits = colorBits;
	pfd->cRedBits = 0;
	pfd->cRedShift = 0;
	pfd->cGreenBits = 0;
	pfd->cGreenShift = 0;
	pfd->cBlueBits = 0;
	pfd->cBlueShift = 0;	// wow! Blue Shift %)

	pfd->cAlphaBits = alphaBits;
	pfd->cAlphaShift = 0;

	pfd->cAccumBits = 0;
	pfd->cAccumRedBits = 0;
	pfd->cAccumGreenBits = 0;
	pfd->cAccumBlueBits = 0;
	pfd->cAccumAlphaBits= 0;

	pfd->cDepthBits = depthBits;
	pfd->cStencilBits = stencilBits;

	pfd->cAuxBuffers = 0;
	pfd->iLayerType = PFD_MAIN_PLANE;
	pfd->bReserved = 0;

	pfd->dwLayerMask = 0;
	pfd->dwVisibleMask = 0;
	pfd->dwDamageMask = 0;

	// count PFDs
	pixelFormat = ChoosePixelFormat( glw_state.hDC, pfd );

	if( !pixelFormat )
	{
		MsgDev( D_ERROR, "VID_ChoosePFD failed\n" );
		return 0;
	}

	return pixelFormat;
}

/*
=================
pfnEnumWnd

callback to enumerate active windows
=================
*/
BOOL CALLBACK pfnEnumWnd( HWND hwnd, LPARAM lParam )
{
	string	wndname;

	if( GetClassName( hwnd, wndname, sizeof( wndname ) - 1 ))
	{
		if( !Q_strcmp( wndname, WINDOW_NAME ))
			num_instances++;
	}
	return true;
}

/*
=================
VID_EnumerateInstances
=================
*/
uint VID_EnumerateInstances( void )
{
	num_instances = 0;

	if( EnumWindows( &pfnEnumWnd, 0 ))
		return num_instances;
	return 1;
}

/*
=================
VID_StartupGamma
=================
*/
void VID_StartupGamma( void )
{
	size_t	gamma_size;
	byte	*savedGamma;

	// init gamma ramp
	memset( glState.stateRamp, 0, sizeof( glState.stateRamp ));

	glConfig.deviceSupportsGamma = GetDeviceGammaRamp( glw_state.hDC, glState.stateRamp );

	if( !glConfig.deviceSupportsGamma )
	{
		// force to set cvar
		Cvar_FullSet( "gl_ignorehwgamma", "1", CVAR_GLCONFIG );
	}

	if( gl_ignorehwgamma->integer )
	{
		glConfig.deviceSupportsGamma = false;	// even if supported!
		BuildGammaTable( vid_gamma->value, GAMMA );
		MsgDev( D_NOTE, "VID_StartupGamma: software gamma initialized\n" );
		return;
	}

	savedGamma = FS_LoadFile( "gamma.dat", &gamma_size, false );

	if( !savedGamma || gamma_size != sizeof( glState.stateRamp ))
	{
		// saved gamma not found or corrupted file
		FS_WriteFile( "gamma.dat", glState.stateRamp, sizeof( glState.stateRamp ));
		MsgDev( D_NOTE, "VID_StartupGamma: gamma.dat initialized\n" );
		if( savedGamma ) Mem_Free( savedGamma );
	}
	else
	{
		GL_BuildGammaTable();

		// validate base gamma
		if( !memcmp( savedGamma, glState.stateRamp, sizeof( glState.stateRamp )))
		{
			// all ok, previous gamma is valid
			MsgDev( D_NOTE, "VID_StartupGamma: validate screen gamma - ok\n" );
		}
		else if( !memcmp( glState.gammaRamp, glState.stateRamp, sizeof( glState.stateRamp )))
		{
			// screen gamma is equal to render gamma (probably previous instance crashed)
			// run additional check to make sure for it
			if( memcmp( savedGamma, glState.stateRamp, sizeof( glState.stateRamp )))
			{
				// yes, current gamma it's totally wrong, restore it from gamma.dat
				MsgDev( D_NOTE, "VID_StartupGamma: restore original gamma after crash\n" );
				memcpy( glState.stateRamp, savedGamma, sizeof( glState.gammaRamp ));
			}
			else
			{
				// oops, savedGamma == glState.stateRamp == glState.gammaRamp
				// probably r_gamma set as default
				MsgDev( D_NOTE, "VID_StartupGamma: validate screen gamma - disabled\n" ); 
			}
		}
		else if( !memcmp( glState.gammaRamp, savedGamma, sizeof( glState.stateRamp )))
		{
			// saved gamma is equal render gamma, probably gamma.dat wroted after crash
			// run additional check to make sure it
			if( memcmp( savedGamma, glState.stateRamp, sizeof( glState.stateRamp )))
			{
				// yes, saved gamma it's totally wrong, get origianl gamma from screen
				MsgDev( D_NOTE, "VID_StartupGamma: merge gamma.dat after crash\n" );
				FS_WriteFile( "gamma.dat", glState.stateRamp, sizeof( glState.stateRamp ));
			}
			else
			{
				// oops, savedGamma == glState.stateRamp == glState.gammaRamp
				// probably r_gamma set as default
				MsgDev( D_NOTE, "VID_StartupGamma: validate screen gamma - disabled\n" ); 
			}
		}
		else
		{
			// current gamma unset by other application, so we can restore it here
			MsgDev( D_NOTE, "VID_StartupGamma: restore original gamma after crash\n" );
			memcpy( glState.stateRamp, savedGamma, sizeof( glState.gammaRamp ));			
		}

		Mem_Free( savedGamma );
	}

	vid_gamma->modified = true;
}

/*
=================
VID_RestoreGamma
=================
*/
void VID_RestoreGamma( void )
{
	if( !glw_state.hDC || !glConfig.deviceSupportsGamma )
		return;

	// don't touch gamma if multiple instances was running
	if( VID_EnumerateInstances( ) > 1 ) return;

	SetDeviceGammaRamp( glw_state.hDC, glState.stateRamp );
}

/*
=================
GL_SetPixelformat
=================
*/
qboolean GL_SetPixelformat( void )
{
	PIXELFORMATDESCRIPTOR	PFD;
	int			alphaBits;
	int			stencilBits;
	int			pixelFormat;

	if(( glw_state.hDC = GetDC( host.hWnd )) == NULL )
		return false;

	// set alpha/stencil
	alphaBits = bound( 0, gl_alphabits->integer, 8 );
	stencilBits = bound( 0, gl_stencilbits->integer, 8 );

	if( glw_state.desktopBitsPixel < 32 )
	{
		// clear alphabits in case we in 16-bit mode
		alphaBits = 0;
	}

	// choose a pixel format
	pixelFormat = VID_ChoosePFD( &PFD, 24, alphaBits, 32, stencilBits );

	if( !pixelFormat )
	{
		// try again with default color/depth/stencil
		pixelFormat = VID_ChoosePFD( &PFD, 24, 0, 32, 0 );

		if( !pixelFormat )
		{
			MsgDev( D_ERROR, "GL_SetPixelformat: failed to find an appropriate PIXELFORMAT\n" );
			return false;
		}
	}

	// set the pixel format
	if( !SetPixelFormat( glw_state.hDC, pixelFormat, &PFD ))
	{
		MsgDev( D_ERROR, "GL_SetPixelformat: failed\n" );
		return false;
	}

	DescribePixelFormat( glw_state.hDC, pixelFormat, sizeof( PIXELFORMATDESCRIPTOR ), &PFD );

	if( PFD.dwFlags & PFD_GENERIC_FORMAT )
	{
		if( PFD.dwFlags & PFD_GENERIC_ACCELERATED )
		{
			MsgDev( D_NOTE, "VID_ChoosePFD: using Generic MCD acceleration\n" );
		}
		else
		{
			MsgDev( D_ERROR, "GL_SetPixelformat: no hardware acceleration found\n" );
			return false;
		}
	}
	else
	{
		MsgDev( D_NOTE, "VID_ChoosePFD: using hardware acceleration\n");
	}

	glConfig.color_bits = PFD.cColorBits;
	glConfig.alpha_bits = PFD.cAlphaBits;
	glConfig.depth_bits = PFD.cDepthBits;
	glConfig.stencil_bits = PFD.cStencilBits;

	if( PFD.cStencilBits != 0 )
		glState.stencilEnabled = true;
	else glState.stencilEnabled = false;

	// print out PFD specifics 
	MsgDev( D_NOTE, "GL PFD: color( %d-bits ) alpha( %d-bits ) Z( %d-bit )\n", PFD.cColorBits, PFD.cAlphaBits, PFD.cDepthBits );

	return true;
}

/*
=================
R_SaveVideoMode
=================
*/
void R_SaveVideoMode( int vid_mode )
{
	int	mode = bound( 0, vid_mode, num_vidmodes ); // check range

	glState.width = vidmode[mode].width;
	glState.height = vidmode[mode].height;
	glState.wideScreen = vidmode[mode].wideScreen;

	Cvar_FullSet( "width", va( "%i", vidmode[mode].width ), CVAR_READ_ONLY );
	Cvar_FullSet( "height", va( "%i", vidmode[mode].height ), CVAR_READ_ONLY );
	Cvar_SetFloat( "vid_mode", mode ); // merge if it out of bounds

	MsgDev( D_NOTE, "Set: %s [%dx%d]\n", vidmode[mode].desc, vidmode[mode].width, vidmode[mode].height );
}

/*
=================
R_DescribeVIDMode
=================
*/
qboolean R_DescribeVIDMode( int width, int height )
{
	int	i;

	for( i = 0; i < sizeof( vidmode ) / sizeof( vidmode[0] ); i++ )
	{
		if( vidmode[i].width == width && vidmode[i].height == height )
		{
			// found specified mode
			Cvar_SetFloat( "vid_mode", i );
			return true;
		}
	}

	return false;
}

/*
=================
VID_CreateWindow
=================
*/
qboolean VID_CreateWindow( int width, int height, qboolean fullscreen )
{
	int		x = 0, y = 0, w, h;
	int		stylebits = WINDOW_STYLE;
	int		exstyle = WINDOW_EX_STYLE;
	static string	wndname;
	HWND		window;
	RECT		rect;	
	WNDCLASS		wc;

	Q_strncpy( wndname, GI->title, sizeof( wndname ));

	// register the frame class
	wc.style         = CS_OWNDC|CS_NOCLOSE;
	wc.lpfnWndProc   = (WNDPROC)IN_WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = host.hInst;
	wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (void *)COLOR_3DSHADOW;
	wc.lpszClassName = WINDOW_NAME;
	wc.lpszMenuName  = 0;
	wc.hIcon         = 0;

	// find the icon file in the filesystem
	if( FS_FileExists( GI->iconpath, true ))
	{
		if( FS_GetDiskPath( GI->iconpath, true ))
		{
			string	localPath;
			Q_snprintf( localPath, sizeof( localPath ), "%s/%s", GI->gamedir, GI->iconpath );
			wc.hIcon = LoadImage( NULL, localPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE|LR_DEFAULTSIZE );
		}
		else MsgDev( D_INFO, "Extract %s from pak if you want to see it.\n", GI->iconpath );
	}

	// couldn't loaded for some reasons? use default
	if( !wc.hIcon ) wc.hIcon = LoadIcon( host.hInst, MAKEINTRESOURCE( 101 ));

	if( !RegisterClass( &wc ))
	{ 
		MsgDev( D_ERROR, "VID_CreateWindow: couldn't register window class %s\n" WINDOW_NAME );
		return false;
	}

	if( fullscreen )
	{
		stylebits = WS_POPUP|WS_VISIBLE;
		exstyle = WS_EX_TOPMOST;
	}

	rect.left = 0;
	rect.top = 0;
	rect.right  = width;
	rect.bottom = height;

	AdjustWindowRect( &rect, stylebits, FALSE );
	w = rect.right - rect.left;
	h = rect.bottom - rect.top;

	if( !fullscreen )
	{
		x = r_xpos->integer;
		y = r_ypos->integer;

		// adjust window coordinates if necessary 
		// so that the window is completely on screen
		if( x < 0 ) x = 0;
		if( y < 0 ) y = 0;

		if( Cvar_VariableInteger( "vid_mode" ) != glConfig.prev_mode )
		{
			// adjust window in the screen size
			if(( x + w > glw_state.desktopWidth ) || ( y + h > glw_state.desktopHeight ))
			{
				x = ( glw_state.desktopWidth - w ) / 2;
				y = ( glw_state.desktopHeight - h ) / 2;
			}
		}
	}

	window = CreateWindowEx( exstyle, WINDOW_NAME, wndname, stylebits, x, y, w, h, NULL, NULL, host.hInst, NULL );

	if( host.hWnd != window )
	{
		// make sure what CreateWindowEx call the IN_WndProc
		MsgDev( D_WARN, "VID_CreateWindow: bad hWnd for '%s'\n", wndname );
	}

	if( !host.hWnd ) 
	{
		// host.hWnd must be filled in IN_WndProc
		MsgDev( D_ERROR, "VID_CreateWindow: couldn't create '%s'\n", wndname );
		return false;
	}

	ShowWindow( host.hWnd, SW_SHOW );
	UpdateWindow( host.hWnd );

	// init all the gl stuff for the window
	if( !GL_SetPixelformat( ))
	{
		ShowWindow( host.hWnd, SW_HIDE );
		DestroyWindow( host.hWnd );
		host.hWnd = NULL;

		UnregisterClass( WINDOW_NAME, host.hInst );
		MsgDev( D_ERROR, "OpenGL driver not installed\n" );

		return false;
	}

	if( !glw_state.initialized )
	{
		if( !GL_CreateContext( ))
			return false;

		VID_StartupGamma();
	}
	else
	{
		if( !GL_UpdateContext( ))
			return false;		
	}

	SetForegroundWindow( host.hWnd );
	SetFocus( host.hWnd );

	return true;
}

/*
=================
VID_DestroyWindow
=================
*/
void VID_DestroyWindow( void )
{
	if( pwglMakeCurrent )
		pwglMakeCurrent( NULL, NULL );

	if( glw_state.hDC )
	{
		ReleaseDC( host.hWnd, glw_state.hDC );
		glw_state.hDC = NULL;
	}

	if( host.hWnd )
	{
		DestroyWindow ( host.hWnd );
		host.hWnd = NULL;
	}

	UnregisterClass( WINDOW_NAME, host.hInst );

	if( glState.fullScreen )
	{
		ChangeDisplaySettings( 0, 0 );
		glState.fullScreen = false;
	}
}

/*
=================
R_ChangeDisplaySettings
=================
*/
rserr_t R_ChangeDisplaySettings( int vid_mode, qboolean fullscreen )
{
	int	width, height;
	int	cds_result;
	HDC	hDC;
	
	R_SaveVideoMode( vid_mode );

	width = r_width->integer;
	height = r_height->integer;

	// check our desktop attributes
	hDC = GetDC( GetDesktopWindow( ));
	glw_state.desktopBitsPixel = GetDeviceCaps( hDC, BITSPIXEL );
	glw_state.desktopWidth = GetDeviceCaps( hDC, HORZRES );
	glw_state.desktopHeight = GetDeviceCaps( hDC, VERTRES );
	ReleaseDC( GetDesktopWindow(), hDC );

	// destroy the existing window
	if( host.hWnd ) VID_DestroyWindow();

	// do a CDS if needed
	if( fullscreen )
	{
		DEVMODE	dm;

		memset( &dm, 0, sizeof( dm ));
		dm.dmSize = sizeof( dm );
		dm.dmPelsWidth = width;
		dm.dmPelsHeight = height;
		dm.dmFields = DM_PELSWIDTH|DM_PELSHEIGHT;

		if( vid_displayfrequency->integer > 0 )
		{
			if( vid_displayfrequency->integer < 60 ) Cvar_SetFloat( "vid_displayfrequency", 60 );
			if( vid_displayfrequency->integer > 100 ) Cvar_SetFloat( "vid_displayfrequency", 100 );

			dm.dmFields |= DM_DISPLAYFREQUENCY;
			dm.dmDisplayFrequency = vid_displayfrequency->integer;
		}

		cds_result = ChangeDisplaySettings( &dm, CDS_FULLSCREEN );

		if( cds_result == DISP_CHANGE_SUCCESSFUL )
		{
			glState.fullScreen = true;

			if( !VID_CreateWindow( width, height, true ))
				return rserr_invalid_mode;
			return rserr_ok;
		}
		else if( cds_result == DISP_CHANGE_BADDUALVIEW )
		{
			dm.dmPelsWidth = width * 2;
			dm.dmPelsHeight = height;
			dm.dmFields = DM_PELSWIDTH|DM_PELSHEIGHT;

			// our first CDS failed, so maybe we're running on some weird dual monitor system 
			if( ChangeDisplaySettings( &dm, CDS_FULLSCREEN ) != DISP_CHANGE_SUCCESSFUL )
			{
				ChangeDisplaySettings( 0, 0 );
				glState.fullScreen = false;
				if( !VID_CreateWindow( width, height, false ))
					return rserr_invalid_mode;
				return rserr_invalid_fullscreen;
			}
			else
			{
				if( !VID_CreateWindow( width, height, true ))
					return rserr_invalid_mode;
				glState.fullScreen = true;
				return rserr_ok;
			}
		}
		else
		{
			int	freq_specified = 0;

			if( vid_displayfrequency->integer > 0 )
			{
				// clear out custom frequency
				freq_specified = vid_displayfrequency->integer;
				Cvar_SetFloat( "vid_displayfrequency", 0.0f );
				dm.dmFields &= ~DM_DISPLAYFREQUENCY;
				dm.dmDisplayFrequency = 0;
			}

			// our first CDS failed, so maybe we're running with too high displayfrequency
			if( ChangeDisplaySettings( &dm, CDS_FULLSCREEN ) != DISP_CHANGE_SUCCESSFUL )
			{
				ChangeDisplaySettings( 0, 0 );
				glState.fullScreen = false;
				if( !VID_CreateWindow( width, height, false ))
					return rserr_invalid_mode;
				return rserr_invalid_fullscreen;
			}
			else
			{
				if( !VID_CreateWindow( width, height, true ))
					return rserr_invalid_mode;

				if( freq_specified )
					MsgDev( D_ERROR, "VID_SetMode: display frequency %i Hz is not supported\n", freq_specified );
				glState.fullScreen = true;
				return rserr_ok;
			}
		}
	}
	else
	{
		ChangeDisplaySettings( 0, 0 );
		glState.fullScreen = false;
		if( !VID_CreateWindow( width, height, false ))
			return rserr_invalid_mode;
	}

	return rserr_ok;
}

/*
==================
VID_SetMode

Set the described video mode
==================
*/
qboolean VID_SetMode( void )
{
	qboolean	fullscreen;
	rserr_t	err;

	if( vid_mode->integer == -1 )	// trying to get resolution automatically by default
	{
		HDC	hDCScreen = GetDC( NULL );
		int	iScreenWidth = GetDeviceCaps( hDCScreen, HORZRES );
		int	iScreenHeight = GetDeviceCaps( hDCScreen, VERTRES );

		ReleaseDC( NULL, hDCScreen );

		if( R_DescribeVIDMode( iScreenWidth, iScreenHeight ))
		{
			MsgDev( D_NOTE, "found specified vid mode %i [%ix%i]\n", vid_mode->integer, iScreenWidth, iScreenHeight );
			Cvar_SetFloat( "fullscreen", 1 );
		}
		else
		{
			MsgDev( D_NOTE, "failed to set specified vid mode [%ix%i]\n", iScreenWidth, iScreenHeight );
			Cvar_SetFloat( "vid_mode", VID_DEFAULTMODE );
		}
	}

	fullscreen = vid_fullscreen->integer;
	gl_swapInterval->modified = true;

	if(( err = R_ChangeDisplaySettings( vid_mode->integer, fullscreen )) == rserr_ok )
	{
		glConfig.prev_mode = vid_mode->integer;
	}
	else
	{
		if( err == rserr_invalid_fullscreen )
		{
			Cvar_SetFloat( "fullscreen", 0 );
			MsgDev( D_ERROR, "VID_SetMode: fullscreen unavailable in this mode\n" );
			if(( err = R_ChangeDisplaySettings( vid_mode->integer, false )) == rserr_ok )
				return true;
		}
		else if( err == rserr_invalid_mode )
		{
			Cvar_SetFloat( "vid_mode", glConfig.prev_mode );
			MsgDev( D_ERROR, "VID_SetMode: invalid mode\n" );
		}

		// try setting it back to something safe
		if(( err = R_ChangeDisplaySettings( glConfig.prev_mode, false )) != rserr_ok )
		{
			MsgDev( D_ERROR, "VID_SetMode: could not revert to safe mode\n" );
			return false;
		}
	}

	return true;
}

/*
==================
VID_CheckChanges

check vid modes and fullscreen
==================
*/
void VID_CheckChanges( void )
{
	if( cl_allow_levelshots->modified )
          {
		GL_FreeTexture( cls.loadingBar );
		SCR_RegisterTextures(); // reload 'lambda' image
		cl_allow_levelshots->modified = false;
          }
 
	if( renderinfo->modified )
	{
		if( !VID_SetMode( ))
		{
			Msg( "Error: can't initialize video subsystem\n" );
			Host_NewInstance( va("#%s", GI->gamefolder ), "stopped" );
		}
		else
		{
			renderinfo->modified = false;
			SCR_VidInit(); // tell the client.dll what vid_mode has changed
		}
	}
}

/*
==================
R_Init_OpenGL
==================
*/
qboolean R_Init_OpenGL( void )
{
	Sys_LoadLibrary( &opengl_dll );	// load opengl32.dll

	if( !opengl_dll.link )
		return false;

	if( Sys_CheckParm( "-gldebug" ) && host.developer >= 1 )
		GL_CheckExtension( "OpenGL Internal ProcAddress", wglproc_funcs, NULL, GL_WGL_PROCADDRESS );

	return VID_SetMode();
}

/*
==================
R_Free_OpenGL
==================
*/
void R_Free_OpenGL( void )
{
	VID_RestoreGamma ();

	GL_DeleteContext ();

	VID_DestroyWindow ();

	Sys_FreeLibrary( &opengl_dll );

	// now all extensions are disabled
	memset( glConfig.extension, 0, sizeof( glConfig.extension ));
	glw_state.initialized = false;
}

/*
===============
GL_SetDefaults
===============
*/
static void GL_SetDefaults( void )
{
	pglFinish();

	pglClearColor( 0.5f, 0.5f, 0.5f, 1.0f );

	pglDisable( GL_DEPTH_TEST );
	pglDisable( GL_CULL_FACE );
	pglDisable( GL_SCISSOR_TEST );
	pglDepthFunc( GL_LEQUAL );
	pglDepthMask( GL_FALSE );

	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	if( glState.stencilEnabled )
	{
		pglDisable( GL_STENCIL_TEST );
		pglStencilMask( ( GLuint ) ~0 );
		pglStencilFunc( GL_EQUAL, 0, ~0 );
		pglStencilOp( GL_KEEP, GL_INCR, GL_INCR );
	}

	pglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	pglPolygonOffset( -1.0f, -2.0f );

	GL_CleanupAllTextureUnits();

	pglDisable( GL_BLEND );
	pglDisable( GL_ALPHA_TEST );
	pglDisable( GL_POLYGON_OFFSET_FILL );
	pglAlphaFunc( GL_GEQUAL, 0.5f );
	pglEnable( GL_TEXTURE_2D );
	pglShadeModel( GL_SMOOTH );

	pglPointSize( 1.2f );
	pglLineWidth( 1.2f );

	GL_Cull( GL_NONE );
	GL_FrontFace( 0 );
}

/*
=================
R_RenderInfo_f
=================
*/
void R_RenderInfo_f( void )
{
	Msg( "\n" );
	Msg( "GL_VENDOR: %s\n", glConfig.vendor_string );
	Msg( "GL_RENDERER: %s\n", glConfig.renderer_string );
	Msg( "GL_VERSION: %s\n", glConfig.version_string );

	// don't spam about extensions
	if( host.developer >= D_REPORT )
	{
		Msg( "GL_EXTENSIONS: %s\n", glConfig.extensions_string );

		if( glConfig.wgl_extensions_string != NULL )
			Msg( "\nWGL_EXTENSIONS: %s\n", glConfig.wgl_extensions_string );
	}

	Msg( "GL_MAX_TEXTURE_SIZE: %i\n", glConfig.max_2d_texture_size );
	
	if( GL_Support( GL_ARB_MULTITEXTURE ))
		Msg( "GL_MAX_TEXTURE_UNITS_ARB: %i\n", glConfig.max_texture_units );
	if( GL_Support( GL_TEXTURE_CUBEMAP_EXT ))
		Msg( "GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB: %i\n", glConfig.max_cubemap_size );
	if( GL_Support( GL_ANISOTROPY_EXT ))
		Msg( "GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT: %.1f\n", glConfig.max_texture_anisotropy );
	if( GL_Support( GL_TEXTURE_2D_RECT_EXT ))
		Msg( "GL_MAX_RECTANGLE_TEXTURE_SIZE: %i\n", glConfig.max_2d_rectangle_size );
	if( GL_Support( GL_TEXTURE_ARRAY_EXT ))
		Msg( "GL_MAX_ARRAY_TEXTURE_LAYERS_EXT: %i\n", glConfig.max_2d_texture_layers );
	if( GL_Support( GL_SHADER_GLSL100_EXT ))
	{
		Msg( "GL_MAX_TEXTURE_COORDS_ARB: %i\n", glConfig.max_texture_coords );
		Msg( "GL_MAX_TEXTURE_IMAGE_UNITS_ARB: %i\n", glConfig.max_teximage_units );
		Msg( "GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB: %i\n", glConfig.max_vertex_uniforms );
		Msg( "GL_MAX_VERTEX_ATTRIBS_ARB: %i\n", glConfig.max_vertex_attribs );
	}

	Msg( "\n" );
	Msg( "MODE: %i, %i x %i %s\n", vid_mode->integer, r_width->integer, r_height->integer, vidmode[vid_mode->integer].desc );
	Msg( "GAMMA: %s\n", (glConfig.deviceSupportsGamma) ? "hardware" : "software" );
	Msg( "\n" );
	Msg( "PICMIP: %i\n", gl_picmip->integer );
	Msg( "SKYMIP: %i\n", gl_skymip->integer );
	Msg( "VERTICAL SYNC: %s\n", gl_swapInterval->integer ? "enabled" : "disabled" );
	Msg( "Color %d bits, Alpha %d bits, Depth %d bits, Stencil %d bits\n", glConfig.color_bits,
		glConfig.alpha_bits, glConfig.depth_bits, glConfig.stencil_bits );
}

//=======================================================================

/*
=================
GL_InitCommands
=================
*/
void GL_InitCommands( void )
{
	Cbuf_Execute();

	// system screen width and height (don't suppose for change from console at all)
	r_width = Cvar_Get( "width", "640", CVAR_READ_ONLY, "screen width" );
	r_height = Cvar_Get( "height", "480", CVAR_READ_ONLY, "screen height" );
	renderinfo = Cvar_Get( "@renderinfo", "0", CVAR_READ_ONLY, "" ); // use ->modified value only
	r_speeds = Cvar_Get( "r_speeds", "0", CVAR_ARCHIVE, "shows renderer speeds" );
	r_fullbright = Cvar_Get( "r_fullbright", "0", CVAR_CHEAT, "disable lightmaps, get fullbright for entities" );
	r_norefresh = Cvar_Get( "r_norefresh", "0", 0, "disable 3D rendering (use with caution)" );
	r_lighting_extended = Cvar_Get( "r_lighting_extended", "1", CVAR_ARCHIVE, "allow to get lighting from world and bmodels" );
	r_lighting_modulate = Cvar_Get( "r_lighting_modulate", "0.6", CVAR_ARCHIVE, "lightstyles modulate scale" );
	r_lighting_ambient = Cvar_Get( "r_lighting_ambient", "0.3", CVAR_ARCHIVE, "map ambient lighting scale" );
	r_adjust_fov = Cvar_Get( "r_adjust_fov", "1", CVAR_ARCHIVE, "making FOV adjustment for wide-screens" );
	r_novis = Cvar_Get( "r_novis", "0", 0, "ignore vis information (perfomance test)" );
	r_nocull = Cvar_Get( "r_nocull", "0", 0, "ignore frustrum culling (perfomance test)" );
	r_faceplanecull = Cvar_Get( "r_faceplanecull", "1", 0, "ignore face plane culling (perfomance test)" );
	r_detailtextures = Cvar_Get( "r_detailtextures", "1", CVAR_ARCHIVE, "enable detail textures support, use '2' for autogenerate detail.txt" );
	r_lockpvs = Cvar_Get( "r_lockpvs", "0", CVAR_CHEAT, "lockpvs area at current point (pvs test)" );
	r_lockcull = Cvar_Get( "r_lockcull", "0", CVAR_CHEAT, "lock frustrum area at current point (cull test)" );
	r_dynamic = Cvar_Get( "r_dynamic", "1", CVAR_ARCHIVE, "allow dynamic lighting (dlights, lightstyles)" );
	r_lightmap = Cvar_Get( "r_lightmap", "0", CVAR_CHEAT, "lightmap debugging tool" );
	r_fastsky = Cvar_Get( "r_fastsky", "0", CVAR_ARCHIVE, "enable algorhytm fo fast sky rendering (for old machines)" );
	r_drawentities = Cvar_Get( "r_drawentities", "1", CVAR_CHEAT|CVAR_ARCHIVE, "render entities" );
	r_flaresize = Cvar_Get( "r_flaresize", "200", CVAR_ARCHIVE, "set flares size" );
	r_lefthand = Cvar_Get( "hand", "0", CVAR_ARCHIVE, "viewmodel handedness" );
	r_decals = Cvar_Get( "r_decals", "4096", CVAR_ARCHIVE, "sets the maximum number of decals" );
	r_xpos = Cvar_Get( "r_xpos", "130", CVAR_GLCONFIG, "window position by horizontal" );
	r_ypos = Cvar_Get( "r_ypos", "48", CVAR_GLCONFIG, "window position by vertical" );
			
	gl_picmip = Cvar_Get( "gl_picmip", "0", CVAR_GLCONFIG, "reduces resolution of textures by powers of 2" );
	gl_skymip = Cvar_Get( "gl_skymip", "0", CVAR_GLCONFIG, "reduces resolution of skybox textures by powers of 2" );
	gl_ignorehwgamma = Cvar_Get( "gl_ignorehwgamma", "0", CVAR_GLCONFIG, "ignore hardware gamma" );
	gl_alphabits = Cvar_Get( "gl_alphabits", "8", CVAR_GLCONFIG, "pixelformat alpha bits (0 - auto)" );
	gl_texture_nearest = Cvar_Get( "gl_texture_nearest", "0", CVAR_ARCHIVE, "disable texture filter" );
	gl_round_down = Cvar_Get( "gl_round_down", "0", CVAR_GLCONFIG, "down size non-power of two textures" );
	gl_max_size = Cvar_Get( "gl_max_size", "512", CVAR_ARCHIVE, "no effect in Xash3D just a legacy" );
	gl_stencilbits = Cvar_Get( "gl_stencilbits", "8", CVAR_GLCONFIG, "pixelformat stencil bits (0 - auto)" );
	gl_check_errors = Cvar_Get( "gl_check_errors", "1", CVAR_ARCHIVE, "ignore video engine errors" );
	gl_swapInterval = Cvar_Get( "gl_swapInterval", "0", CVAR_ARCHIVE,  "time beetween frames (in msec)" );
	gl_extensions = Cvar_Get( "gl_extensions", "1", CVAR_GLCONFIG, "allow gl_extensions" );
	gl_detailscale = Cvar_Get( "gl_detailscale", "4.0", CVAR_ARCHIVE, "default scale applies while auto-generate list of detail textures" );
	gl_texture_anisotropy = Cvar_Get( "gl_anisotropy", "2.0", CVAR_ARCHIVE, "textures anisotropic filter" );
	gl_texture_lodbias =  Cvar_Get( "gl_texture_lodbias", "0.0", CVAR_ARCHIVE, "LOD bias for mipmapped textures (prefomance|quality)" );
	gl_compress_textures = Cvar_Get( "gl_compress_textures", "0", CVAR_GLCONFIG, "compress textures to safe video memory" ); 
	gl_compensate_gamma_screenshots = Cvar_Get( "gl_compensate_gamma_screenshots", "0", CVAR_ARCHIVE, "allow to apply gamma for screenshots" ); 
	gl_keeptjunctions = Cvar_Get( "gl_keeptjunctions", "1", CVAR_ARCHIVE, "but removing tjuncs causes blinking pixels" ); 
	gl_allow_static = Cvar_Get( "gl_allow_static", "0", CVAR_ARCHIVE, "force to drawing non-moveable brushes as part of world (save FPS)" );
	gl_allow_mirrors = Cvar_Get( "gl_allow_mirrors", "1", CVAR_ARCHIVE, "allow to draw mirror surfaces" );
	gl_showtextures = Cvar_Get( "r_showtextures", "0", CVAR_CHEAT, "show all uploaded textures" );
	gl_finish = Cvar_Get( "gl_finish", "0", CVAR_ARCHIVE, "use glFinish instead of glFlush" );
	gl_nosort = Cvar_Get( "gl_nosort", "0", CVAR_ARCHIVE, "disable sorting of translucent surfaces" );
	gl_clear = Cvar_Get( "gl_clear", "0", CVAR_ARCHIVE, "clearing screen after each frame" );
	gl_test = Cvar_Get( "gl_test", "0", 0, "engine developer cvar for quick testing new features" );
	gl_wireframe = Cvar_Get( "gl_wireframe", "0", CVAR_ARCHIVE, "show wireframe overlay" );
	gl_overview = Cvar_Get( "dev_overview", "0", 0, "show level overview" );

	// these cvar not used by engine but some mods requires this
	Cvar_Get( "gl_polyoffset", "-0.1", 0, "polygon offset for decals" );
 
	// make sure r_swapinterval is checked after vid_restart
	gl_swapInterval->modified = true;

	vid_gamma = Cvar_Get( "gamma", "1.0", CVAR_ARCHIVE, "gamma amount" );
	vid_mode = Cvar_Get( "vid_mode", VID_AUTOMODE, CVAR_RENDERINFO, "display resolution mode" );
	vid_fullscreen = Cvar_Get( "fullscreen", "0", CVAR_RENDERINFO, "set in 1 to enable fullscreen mode" );
	vid_displayfrequency = Cvar_Get ( "vid_displayfrequency", "0", CVAR_RENDERINFO, "fullscreen refresh rate" );

	Cmd_AddCommand( "r_info", R_RenderInfo_f, "display renderer info" );
}

/*
=================
GL_RemoveCommands
=================
*/
void GL_RemoveCommands( void )
{
	Cmd_RemoveCommand( "r_info");
}

/*
=================
GL_InitExtensions
=================
*/
void GL_InitExtensions( void )
{
	// initialize gl extensions
	GL_CheckExtension( "OpenGL 1.1.0", opengl_110funcs, NULL, GL_OPENGL_110 );

	// get our various GL strings
	glConfig.vendor_string = pglGetString( GL_VENDOR );
	glConfig.renderer_string = pglGetString( GL_RENDERER );
	glConfig.version_string = pglGetString( GL_VERSION );
	glConfig.extensions_string = pglGetString( GL_EXTENSIONS );
	MsgDev( D_INFO, "Video: %s\n", glConfig.renderer_string );

	// intialize wrapper type
	glConfig.context = CONTEXT_TYPE_GL;
	glConfig.wrapper = GLES_WRAPPER_NONE;

	if( Q_stristr( glConfig.renderer_string, "geforce" ))
		glConfig.hardware_type = GLHW_NVIDIA;
	else if( Q_stristr( glConfig.renderer_string, "quadro fx" ))
		glConfig.hardware_type = GLHW_NVIDIA;
	else if( Q_stristr(glConfig.renderer_string, "rv770" ))
		glConfig.hardware_type = GLHW_RADEON;
	else if( Q_stristr(glConfig.renderer_string, "radeon hd" ))
		glConfig.hardware_type = GLHW_RADEON;
	else if( Q_stristr( glConfig.renderer_string, "eah4850" ) || Q_stristr( glConfig.renderer_string, "eah4870" ))
		glConfig.hardware_type = GLHW_RADEON;
	else if( Q_stristr( glConfig.renderer_string, "radeon" ))
		glConfig.hardware_type = GLHW_RADEON;
	else glConfig.hardware_type = GLHW_GENERIC;

	// initalize until base opengl functions loaded (old-context)
	if( !Sys_CheckParm( "-gldebug" ) || host.developer < D_INFO )
		GL_CheckExtension( "OpenGL Internal ProcAddress", wglproc_funcs, NULL, GL_WGL_PROCADDRESS );

	// windows-specific extensions
	GL_CheckExtension( "WGL Extensions String", wglgetextensionsstring, NULL, GL_WGL_EXTENSIONS );

	if( pwglGetExtensionsStringEXT != NULL )
		glConfig.wgl_extensions_string = pwglGetExtensionsStringEXT();
	else glConfig.wgl_extensions_string = NULL;

	// initalize until base opengl functions loaded
	GL_CheckExtension( "WGL_EXT_swap_control", wglswapintervalfuncs, NULL, GL_WGL_SWAPCONTROL );

	GL_CheckExtension( "glDrawRangeElements", drawrangeelementsfuncs, "gl_drawrangeelments", GL_DRAW_RANGEELEMENTS_EXT );

	if( !GL_Support( GL_DRAW_RANGEELEMENTS_EXT ))
		GL_CheckExtension( "GL_EXT_draw_range_elements", drawrangeelementsextfuncs, "gl_drawrangeelments", GL_DRAW_RANGEELEMENTS_EXT );

	// we don't care if it's an extension or not, they are identical functions, so keep it simple in the rendering code
	if( pglDrawRangeElementsEXT == NULL ) pglDrawRangeElementsEXT = pglDrawRangeElements;

	// multitexture
	glConfig.max_texture_units = glConfig.max_texture_coords = glConfig.max_teximage_units = 1;
	GL_CheckExtension( "GL_ARB_multitexture", multitexturefuncs, "gl_arb_multitexture", GL_ARB_MULTITEXTURE );

	if( GL_Support( GL_ARB_MULTITEXTURE ))
		pglGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &glConfig.max_texture_units );

	if( glConfig.max_texture_units == 1 )
		GL_SetExtension( GL_ARB_MULTITEXTURE, false );

	// 3d texture support
	GL_CheckExtension( "GL_EXT_texture3D", texture3dextfuncs, "gl_texture_3d", GL_TEXTURE_3D_EXT );

	if( GL_Support( GL_TEXTURE_3D_EXT ))
	{
		pglGetIntegerv( GL_MAX_3D_TEXTURE_SIZE, &glConfig.max_3d_texture_size );

		if( glConfig.max_3d_texture_size < 32 )
		{
			GL_SetExtension( GL_TEXTURE_3D_EXT, false );
			MsgDev( D_ERROR, "GL_EXT_texture3D reported bogus GL_MAX_3D_TEXTURE_SIZE, disabled\n" );
		}
	}

	// 2d texture array support
	GL_CheckExtension( "GL_EXT_texture_array", texture3dextfuncs, "gl_texture_2d_array", GL_TEXTURE_ARRAY_EXT );

	if( GL_Support( GL_TEXTURE_ARRAY_EXT ))
		pglGetIntegerv( GL_MAX_ARRAY_TEXTURE_LAYERS_EXT, &glConfig.max_2d_texture_layers );

	// hardware cubemaps
	GL_CheckExtension( "GL_ARB_texture_cube_map", NULL, "gl_texture_cubemap", GL_TEXTURE_CUBEMAP_EXT );

	GL_CheckExtension( "GL_ARB_draw_buffers", drawbuffersfuncs, "gl_draw_buffers", GL_DRAW_BUFFERS_EXT );

	if( GL_Support( GL_TEXTURE_CUBEMAP_EXT ))
	{
		pglGetIntegerv( GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &glConfig.max_cubemap_size );

		// check for seamless cubemaps too
		GL_CheckExtension( "GL_ARB_seamless_cube_map", NULL, "gl_seamless_cubemap", GL_ARB_SEAMLESS_CUBEMAP );
	}

	GL_CheckExtension( "GL_ARB_texture_non_power_of_two", NULL, "gl_texture_npot", GL_ARB_TEXTURE_NPOT_EXT );
	GL_CheckExtension( "GL_ARB_texture_compression", texturecompressionfuncs, "gl_dds_hardware_support", GL_TEXTURE_COMPRESSION_EXT );

	GL_CheckExtension( "GL_EXT_texture_edge_clamp", NULL, "gl_clamp_to_edge", GL_CLAMPTOEDGE_EXT );

	if( !GL_Support( GL_CLAMPTOEDGE_EXT ))
		GL_CheckExtension("GL_SGIS_texture_edge_clamp", NULL, "gl_clamp_to_edge", GL_CLAMPTOEDGE_EXT );

	glConfig.max_texture_anisotropy = 0.0f;
	GL_CheckExtension( "GL_EXT_texture_filter_anisotropic", NULL, "gl_ext_anisotropic_filter", GL_ANISOTROPY_EXT );

	if( GL_Support( GL_ANISOTROPY_EXT ))
		pglGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glConfig.max_texture_anisotropy );

	GL_CheckExtension( "GL_EXT_texture_lod_bias", NULL, "gl_ext_texture_lod_bias", GL_TEXTURE_LOD_BIAS );

	if( GL_Support( GL_TEXTURE_LOD_BIAS ))
		pglGetFloatv( GL_MAX_TEXTURE_LOD_BIAS_EXT, &glConfig.max_texture_lod_bias );

	GL_CheckExtension( "GL_ARB_texture_border_clamp", NULL, "gl_ext_texborder_clamp", GL_CLAMP_TEXBORDER_EXT );

	GL_CheckExtension( "GL_ARB_vertex_buffer_object", vbofuncs, "gl_vertex_buffer_object", GL_ARB_VERTEX_BUFFER_OBJECT_EXT );
	GL_CheckExtension( "GL_ARB_vertex_array_object", vaofuncs, "gl_vertex_array_object", GL_ARB_VERTEX_ARRAY_OBJECT_EXT );
	GL_CheckExtension( "GL_ARB_half_float_pixel", NULL, "gl_half_float", GL_ARB_HALF_FLOAT_EXT );

	// vp and fp shaders
	GL_CheckExtension( "GL_ARB_shader_objects", shaderobjectsfuncs, "gl_shaderobjects", GL_SHADER_OBJECTS_EXT );
	GL_CheckExtension( "GL_ARB_shading_language_100", NULL, "gl_shading_language", GL_SHADER_GLSL100_EXT );

	GL_CheckExtension( "GL_ARB_depth_texture", NULL, "gl_depthtexture", GL_DEPTH_TEXTURE );
	GL_CheckExtension( "GL_ARB_shadow", NULL, "gl_arb_shadow", GL_SHADOW_EXT );

	GL_CheckExtension( "GL_ARB_texture_float", NULL, "gl_arb_texture_float", GL_ARB_TEXTURE_FLOAT_EXT );
	GL_CheckExtension( "GL_ARB_depth_buffer_float", NULL, "gl_arb_depth_float", GL_ARB_DEPTH_FLOAT_EXT );
	GL_CheckExtension( "GL_ARB_texture_rg", NULL, "gl_arb_texture_rg", GL_ARB_TEXTURE_RG );
	GL_CheckExtension( "GL_EXT_gpu_shader4", NULL, "gl_ext_gpu_shader4", GL_EXT_GPU_SHADER4 );

	// this won't work without extended context
	if( glw_state.extended )
		GL_CheckExtension( "GL_ARB_debug_output", debugoutputfuncs, "gl_debug_output", GL_DEBUG_OUTPUT );

	// FBO support
	GL_CheckExtension( "GL_ARB_framebuffer_object", arbfbofuncs, "gl_framebuffer_object", GL_FRAMEBUFFER_OBJECT );

	if( !GL_Support( GL_FRAMEBUFFER_OBJECT ))
		GL_CheckExtension( "GL_EXT_framebuffer_object", extfbofuncs, "gl_framebuffer_object", GL_FRAMEBUFFER_OBJECT );

	// occlusion queries
	GL_CheckExtension( "GL_ARB_occlusion_query", occlusionfunc, "gl_occlusion_queries", GL_OCCLUSION_QUERIES_EXT );

	// rectangle textures support
	GL_CheckExtension( "GL_ARB_texture_rectangle", NULL, "gl_texture_rectangle", GL_TEXTURE_2D_RECT_EXT );

	if( GL_Support( GL_SHADER_GLSL100_EXT ))
	{
		pglGetIntegerv( GL_MAX_TEXTURE_COORDS_ARB, &glConfig.max_texture_coords );
		pglGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &glConfig.max_teximage_units );

		// check for hardware skinning
		pglGetIntegerv( GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB, &glConfig.max_vertex_uniforms );
		pglGetIntegerv( GL_MAX_VERTEX_ATTRIBS_ARB, &glConfig.max_vertex_attribs );

		if( glConfig.hardware_type == GLHW_RADEON )
			glConfig.max_vertex_uniforms /= 4; // radeon returns not correct info
	}
	else
	{
		// just get from multitexturing
		glConfig.max_texture_coords = glConfig.max_teximage_units = glConfig.max_texture_units;
	}

	pglGetIntegerv( GL_MAX_TEXTURE_SIZE, &glConfig.max_2d_texture_size );
	if( glConfig.max_2d_texture_size <= 0 ) glConfig.max_2d_texture_size = 256;

	pglGetIntegerv( GL_MAX_DRAW_BUFFERS_ARB, &glConfig.max_draw_buffers );

	if( GL_Support( GL_TEXTURE_2D_RECT_EXT ))
		pglGetIntegerv( GL_MAX_RECTANGLE_TEXTURE_SIZE_EXT, &glConfig.max_2d_rectangle_size );

	Cvar_Get( "gl_max_texture_size", "0", CVAR_INIT, "opengl texture max dims" );
	Cvar_Set( "gl_max_texture_size", va( "%i", glConfig.max_2d_texture_size ));

	// MCD has buffering issues
	if( Q_stristr( glConfig.renderer_string, "gdi" ))
		Cvar_SetFloat( "gl_finish", 1 );

	Cvar_Set( "gl_anisotropy", va( "%f", bound( 0, gl_texture_anisotropy->value, glConfig.max_texture_anisotropy )));

	if( GL_Support( GL_TEXTURE_COMPRESSION_EXT ))
		Image_AddCmdFlags( IL_DDS_HARDWARE );

	// enable gldebug if allowed
	if( GL_Support( GL_DEBUG_OUTPUT ))
	{
		if( host.developer >= D_ERROR )
			pglDebugMessageCallbackARB( GL_DebugOutput, NULL );

		// force everything to happen in the main thread instead of in a separate driver thread
		if( host.developer >= D_WARN )
			pglEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB );

		// enable all the low priority messages
		if( host.developer >= D_REPORT )
			pglDebugMessageControlARB( GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW_ARB, 0, NULL, true );
	}

	glw_state.initialized = true;

	tr.framecount = tr.visframecount = 1;
}

/*
===============
R_Init
===============
*/
qboolean R_Init( void )
{
	if( glw_state.initialized )
		return true;

	// give initial OpenGL configuration
	Cbuf_AddText( "exec opengl.cfg\n" );

	GL_InitCommands();
	GL_SetDefaultState();

	// create the window and set up the context
	if( !R_Init_OpenGL( ))
	{
		GL_RemoveCommands();
		R_Free_OpenGL();

		Msg( "^1Error:^7 can't initialize video subsystem\n" );
		Host_NewInstance( va( "#%s", GI->gamefolder ), "stopped" );
		return false;
	}

	renderinfo->modified = false;
	r_temppool = Mem_AllocPool( "Render Zone" );

	GL_InitExtensions();
	GL_SetDefaults();
	R_InitImages();
	R_SpriteInit();
	R_StudioInit();
	R_ClearDecals();
	R_ClearScene();

	// initialize screen
	SCR_Init();

	return true;
}

/*
===============
R_Shutdown
===============
*/
void R_Shutdown( void )
{
	int	i;

	if( !glw_state.initialized )
		return;

	// release SpriteTextures
	for( i = 1; i < MAX_IMAGES; i++ )
	{
		if( !clgame.sprites[i].name[0] ) continue;
		Mod_UnloadSpriteModel( &clgame.sprites[i] );
	}
	memset( clgame.sprites, 0, sizeof( clgame.sprites ));

	GL_RemoveCommands();
	R_ShutdownImages();

	Mem_FreePool( &r_temppool );

	// shut down OS specific OpenGL stuff like contexts, etc.
	R_Free_OpenGL();
}

/*
=================
GL_CheckForErrors
=================
*/
void GL_CheckForErrors_( const char *filename, const int fileline )
{
	int	err;
	char	*str;

	if( !gl_check_errors->integer )
		return;

	if(( err = pglGetError( )) == GL_NO_ERROR )
		return;

	switch( err )
	{
	case GL_STACK_OVERFLOW:
		str = "GL_STACK_OVERFLOW";
		break;
	case GL_STACK_UNDERFLOW:
		str = "GL_STACK_UNDERFLOW";
		break;
	case GL_INVALID_ENUM:
		str = "GL_INVALID_ENUM";
		break;
	case GL_INVALID_VALUE:
		str = "GL_INVALID_VALUE";
		break;
	case GL_INVALID_OPERATION:
		str = "GL_INVALID_OPERATION";
		break;
	case GL_OUT_OF_MEMORY:
		str = "GL_OUT_OF_MEMORY";
		break;
	default:
		str = "UNKNOWN ERROR";
		break;
	}

	MsgDev( D_ERROR, "OpenGL: %s (called at %s:%i)\n", str, filename, fileline );
}