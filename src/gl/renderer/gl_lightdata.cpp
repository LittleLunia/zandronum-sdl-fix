/*
** gl_light.cpp
** Light level / fog management / dynamic lights
**
**---------------------------------------------------------------------------
** Copyright 2002-2005 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/


#include "gl/system/gl_system.h"
#include "gl/data/gl_data.h"
#include "gl/renderer/gl_colormap.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/shaders/gl_shader.h"
#include "gl/scene/gl_portal.h"
#include "c_dispatch.h"
#include "p_local.h"
#include "g_level.h"

// [BB] New #includes.
#include "gl/gl_functions.h"

//

// externally settable lighting properties
static float distfogtable[2][256];	// light to fog conversion table for black fog
static int fogdensity;
static PalEntry outsidefogcolor;
static int outsidefogdensity;
int skyfog;

CVAR (Float, gl_light_ambient, 20.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR(Int, gl_weaponlight, 8, CVAR_ARCHIVE);
CVAR(Bool,gl_enhanced_nightvision,true,CVAR_ARCHIVE)


//==========================================================================
//
// Sets up the fog tables
//
//==========================================================================

CUSTOM_CVAR (Int, gl_distfog, 70, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	for (int i=0;i<256;i++)
	{

		if (i<164)
		{
			distfogtable[0][i]= (gl_distfog>>1) + (gl_distfog)*(164-i)/164;
		}
		else if (i<230)
		{											    
			distfogtable[0][i]= (gl_distfog>>1) - (gl_distfog>>1)*(i-164)/(230-164);
		}
		else distfogtable[0][i]=0;

		if (i<128)
		{
			distfogtable[1][i]= 6.f + (gl_distfog>>1) + (gl_distfog)*(128-i)/48;
		}
		else if (i<216)
		{											    
			distfogtable[1][i]= (216.f-i) / ((216.f-128.f)) * gl_distfog / 10;
		}
		else distfogtable[1][i]=0;
	}
}

CUSTOM_CVAR(Int,gl_fogmode,1,CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	if (self>2) self=2;
	if (self<0) self=0;
	if (self == 2 && !gl_ExtFogActive()) self = 1;	// mode 2 requires SM4
}

CUSTOM_CVAR(Int, gl_lightmode, 3 ,CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	if (self>4) self=4;
	if (self<0) self=0;
	if (self == 2 && !gl_ExtFogActive()) self = 3;	// mode 2 requires SM4

	// [BB] Enforce Doom lighting if requested by the dmflags.
	if ( dmflags2 & DF2_FORCE_GL_DEFAULTS )
		glset.lightmode = 3;
	else
		glset.lightmode = self;
}




//==========================================================================
//
// Sets render state to draw the given render style
// includes: Texture mode, blend equation and blend mode
//
//==========================================================================

void gl_GetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending,
					   int *tm, int *sb, int *db, int *be)
{
	static int blendstyles[] = { GL_ZERO, GL_ONE, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA };
	static int renderops[] = { 0, GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, -1, -1, -1, -1};

	int srcblend = blendstyles[style.SrcAlpha&3];
	int dstblend = blendstyles[style.DestAlpha&3];
	int blendequation = renderops[style.BlendOp&7];
	int texturemode = drawopaque? TM_OPAQUE : TM_MODULATE;

	if (style.Flags & STYLEF_ColorIsFixed)
	{
		texturemode = TM_MASK;
	}
	else if (style.Flags & STYLEF_InvertSource)
	{
		texturemode = drawopaque? TM_INVERTOPAQUE : TM_INVERT;
	}

	if (blendequation == -1)
	{
		srcblend = GL_DST_COLOR;
		dstblend = GL_ONE_MINUS_SRC_ALPHA;
		blendequation = GL_FUNC_ADD;
	}

	if (allowcolorblending && srcblend == GL_SRC_ALPHA && dstblend == GL_ONE && blendequation == GL_FUNC_ADD)
	{
		srcblend = GL_SRC_COLOR;
	}

	*tm = texturemode;
	*be = blendequation;
	*sb = srcblend;
	*db = dstblend;
}


//==========================================================================
//
// Set fog parameters for the level
//
//==========================================================================
void gl_SetFogParams(int _fogdensity, PalEntry _outsidefogcolor, int _outsidefogdensity, int _skyfog)
{
	fogdensity=_fogdensity;
	outsidefogcolor=_outsidefogcolor;
	outsidefogdensity=_outsidefogdensity? _outsidefogdensity : _fogdensity? _fogdensity:70;
	skyfog=_skyfog;

	outsidefogdensity>>=1;
	fogdensity>>=1;
}


//==========================================================================
//
// Get current light level
//
//==========================================================================

int gl_CalcLightLevel(int lightlevel, int rellight, bool weapon)
{
	int light;

	if (glset.lightmode&2 && lightlevel<192) 
	{
		light = (192.f - (192-lightlevel)* (weapon? 1.5f : 1.95f));
	}
	else
	{
		light=lightlevel;
	}

	if (light<gl_light_ambient) 
	{
		light=gl_light_ambient;
		if (rellight<0) rellight>>=1;
	}
	return clamp(quickertoint(light+rellight), 0, 255);
}

//==========================================================================
//
// Get current light color
//
//==========================================================================

PalEntry gl_CalcLightColor(int light, PalEntry pe, int blendfactor)
{
	int r,g,b;

	if (blendfactor == 0)
	{
		r = pe.r * light / 255;
		g = pe.g * light / 255;
		b = pe.b * light / 255;
	}
	else
	{
		int mixlight = light * (255 - blendfactor);

		r = (mixlight + pe.r * blendfactor) / 255;
		g = (mixlight + pe.g * blendfactor) / 255;
		b = (mixlight + pe.b * blendfactor) / 255;
	}
	return PalEntry(BYTE(r), BYTE(g), BYTE(b));
}

// [BB]
int gl_GetLightMode () {
	return ( dmflags2 & DF2_FORCE_GL_DEFAULTS ) ? 3 : gl_lightmode;
}

//==========================================================================
//
// Get current light color
//
//==========================================================================
void gl_GetLightColor(int lightlevel, int rellight, const FColormap * cm, float * pred, float * pgreen, float * pblue, bool weapon)
{
	// [BB] This construction purposely overrides the CVAR gl_light_ambient with a local variable of the same name.
	// This allows to implement DF2_FORCE_GL_DEFAULTS without any further changes in this function.
	const float gl_light_ambient_CVAR_value = gl_light_ambient;
	const float gl_light_ambient = ( dmflags2 & DF2_FORCE_GL_DEFAULTS ) ? 20.f : gl_light_ambient_CVAR_value;

	float & r=*pred,& g=*pgreen,& b=*pblue;
	int torch=0;

	if (gl_fixedcolormap) 
	{
		if (gl_fixedcolormap==CM_LITE)
		{
			if (gl_enhanced_nightvision) r=0.375f, g=1.0f, b=0.375f;
			else r=g=b=1.0f;
		}
		else if (gl_fixedcolormap>=CM_TORCH)
		{
			int flicker=gl_fixedcolormap-CM_TORCH;
			r=(0.8f+(7-flicker)/70.0f);
			if (r>1.0f) r=1.0f;
			b=g=r;
			if (gl_enhanced_nightvision) b*=0.75f;
		}
		else r=g=b=1.0f;
		return;
	}

	PalEntry lightcolor = cm? cm->LightColor : PalEntry(255,255,255);
	int blendfactor = cm? cm->blendfactor : 0;

	lightlevel = gl_CalcLightLevel(lightlevel, rellight, weapon);
	PalEntry pe = gl_CalcLightColor(lightlevel, lightcolor, blendfactor);
	r = pe.r/255.f;
	g = pe.g/255.f;
	b = pe.b/255.f;
}

//==========================================================================
//
// set current light color
//
//==========================================================================
void gl_SetColor(int light, int rellight, const FColormap * cm, float *red, float *green, float *blue, PalEntry ThingColor, bool weapon)
{ 
	float r,g,b;
	gl_GetLightColor(light, rellight, cm, &r, &g, &b, weapon);

	*red = r * ThingColor.r/255.0f;
	*green = g * ThingColor.g/255.0f;
	*blue = b * ThingColor.b/255.0f;
}

//==========================================================================
//
// set current light color
//
//==========================================================================
void gl_SetColor(int light, int rellight, const FColormap * cm, float alpha, PalEntry ThingColor, bool weapon)
{ 
	float r,g,b;
	gl_GetLightColor(light, rellight, cm, &r, &g, &b, weapon);
	gl.Color4f(r * ThingColor.r/255.0f, g * ThingColor.g/255.0f, b * ThingColor.b/255.0f, alpha);
}

//==========================================================================
//
// calculates the current fog density
//
//	Rules for fog:
//
//  1. If bit 4 of gl_lightmode is set always use the level's fog density. 
//     This is what Legacy's GL render does.
//	2. black fog means no fog and always uses the distfogtable based on the level's fog density setting
//	3. If outside fog is defined and the current fog color is the same as the outside fog
//	   the engine always uses the outside fog density to make the fog uniform across the level.
//	   If the outside fog's density is undefined it uses the level's fog density and if that is
//	   not defined it uses a default of 70.
//	4. If a global fog density is specified it is being used for all fog on the level
//	5. If none of the above apply fog density is based on the light level as for the software renderer.
//
//==========================================================================

float gl_GetFogDensity(int lightlevel, PalEntry fogcolor)
{
	float density;

	if (glset.lightmode&4)
	{
		// uses approximations of Legacy's default settings.
		density = fogdensity? fogdensity : 18;
	}
	else if ((fogcolor.d & 0xffffff) == 0)
	{
		// case 1: black fog
		density=distfogtable[glset.lightmode!=0][lightlevel];
	}
	else if (outsidefogcolor.a!=0xff && 
			fogcolor.r==outsidefogcolor.r && 
			fogcolor.g==outsidefogcolor.g &&
			fogcolor.b==outsidefogcolor.b) 
	{
		// case 2. outsidefogdensity has already been set as needed
		density=outsidefogdensity;
	}
	else  if (fogdensity!=0)
	{
		// case 3: level has fog density set
		density=fogdensity;
	}
	else 
	{
		// case 4: use light level
		density=clamp<int>(255-lightlevel,30,255);
	}
	return density;
}



PalEntry gl_CurrentFogColor=-1;
static float gl_CurrentFogDensity=-1;

//==========================================================================
//
//
//
//==========================================================================

void gl_InitFog()
{
	gl_CurrentFogColor=-1;
	gl_CurrentFogDensity=-1;
	gl_EnableFog(true);
	gl_EnableFog(false);
	gl.Hint(GL_FOG_HINT, GL_FASTEST);
	gl.Fogi(GL_FOG_MODE, GL_EXP);

}


//==========================================================================
//
// Sets the fog for the current polygon
//
//==========================================================================

void gl_SetFog(int lightlevel, int rellight, const FColormap *cmap, bool isadditive)
{
	// [BB] This construction purposely overrides the CVAR gl_light_ambient with a local variable of the same name.
	// This allows to implement DF2_FORCE_GL_DEFAULTS without any further changes in this function.
	const float gl_light_ambient_CVAR_value = gl_light_ambient;
	const float gl_light_ambient = ( dmflags2 & DF2_FORCE_GL_DEFAULTS ) ? 20.f : gl_light_ambient_CVAR_value;
	// [BB] Take care of gl_fogmode and DF2_FORCE_GL_DEFAULTS.
	OVERRIDE_FOGMODE_IF_NECESSARY

	PalEntry fogcolor;
	float fogdensity;

	if (level.flags&LEVEL_HASFADETABLE)
	{
		fogdensity=70;
		fogcolor=0x808080;
	}
	else if (cmap != NULL && gl_fixedcolormap == 0)
	{
		fogcolor = cmap->FadeColor;
		fogdensity = gl_GetFogDensity(lightlevel, fogcolor);
		fogcolor.a=0;
	}
	else
	{
		fogcolor = 0;
		fogdensity = 0;
	}

	// Make fog a little denser when inside a skybox
	if (GLPortal::inskybox) fogdensity+=fogdensity/2;


	// no fog in enhanced vision modes!
	if (fogdensity==0 || gl_fogmode == 0)
	{
		gl_CurrentFogColor=-1;
		gl_CurrentFogDensity=-1;
		gl_EnableFog(false);
	}
	else
	{
		if (glset.lightmode == 2 && fogcolor == 0)
		{
			float light = gl_CalcLightLevel(lightlevel, rellight, false);
			gl_SetShaderLight(light, lightlevel);
		}
		else
		{
			gl_SetShaderLight(1.f, 1.f);
		}

		// For additive rendering using the regular fog color here would mean applying it twice
		// so always use black
		if (isadditive)
		{
			fogcolor=0;
		}
		// Handle desaturation
		gl_ModifyColor(fogcolor.r, fogcolor.g, fogcolor.b, cmap->colormap);

		gl_EnableFog((int)fogcolor!=-1);
		if (fogcolor!=gl_CurrentFogColor)
		{
			if ((int)fogcolor!=-1)
			{
				GLfloat FogColor[4]={fogcolor.r/255.0f,fogcolor.g/255.0f,fogcolor.b/255.0f,0.0f};
				gl.Fogfv(GL_FOG_COLOR, FogColor);
			}
			gl_CurrentFogColor=fogcolor;
		}
		if (fogdensity!=gl_CurrentFogDensity)
		{
			gl.Fogf(GL_FOG_DENSITY, fogdensity/64000.f);
			gl_CurrentFogDensity=fogdensity;
		}
	}
}

//==========================================================================
//
// Modifies a color according to a specified colormap
//
//==========================================================================

void gl_ModifyColor(BYTE & red, BYTE & green, BYTE & blue, int cm)
{
	int gray = (red*77 + green*143 + blue*36)>>8;
	if (cm >= CM_FIRSTSPECIALCOLORMAP && cm < CM_FIRSTSPECIALCOLORMAP + SpecialColormaps.Size())
	{
		PalEntry pe = SpecialColormaps[cm - CM_FIRSTSPECIALCOLORMAP].GrayscaleToColor[gray];
		red = pe.r;
		green = pe.g;
		blue = pe.b;
	}
	else if (cm >= CM_DESAT1 && cm <= CM_DESAT31)
	{
		gl_Desaturate(gray, red, green, blue, red, green, blue, cm - CM_DESAT0);
	}
}



//==========================================================================
//
// For testing sky fog sheets
//
//==========================================================================
CCMD(skyfog)
{
	if (argv.argc()>1)
	{
		skyfog=strtol(argv[1],NULL,0);
	}
}

