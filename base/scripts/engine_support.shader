// Minimal RTCW engine support. Sources recorded in engine-support.json.

// From scripts/gfx.shader
lagometer
{
	nopicmip
	{
		map gfx/2d/lag.tga
	}
}

// From scripts/gfx.shader
disconnected
{
	nopicmip
	{
		map gfx/2d/net.tga
	}
}

// From scripts/gfx.shader
white
{
	{
		map *white
		blendfunc	GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbgen vertex
	}
}

// From scripts/gfx.shader
spotLight
{
	nocompress
	polygonOffset
	{
		clampmap sprites/spotlight.tga
		blendfunc	GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbgen vertex
	}
}

// From scripts/gfx.shader
dlightshader 	// this one is the 'default' dlight shader blob
{
	{
		map $dlight
		blendfunc GL_DST_COLOR GL_ONE
	}
}

// From scripts/gfx.shader
markShadow
{
	nofog
	polygonOffset
	{
		map sprites/shadow_soft.tga
		blendFunc GL_ZERO GL_ONE_MINUS_SRC_COLOR
		rgbGen exactVertex
	}	
}

// From scripts/gfx.shader
wake
{


	{
		clampmap sprites/splashalpha.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
//		blendfunc blend
		rgbGen vertex
		tcMod stretch sin 1 0.2 0 0.2 
//		alphaGen sin 0.5 0.3 0 0.2 
//		rgbGen wave sin .7 .07 .25 .5
		rgbGen wave sin 0.3 0.3 0 0.2
	}

//	{
//		clampmap sprites/splashalpha.tga
//		//		blendFunc GL_ONE GL_ONE
//		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
//		rgbGen vertex
//		//		tcMod stretch sin .9 0.05 0 0.5
//		//		rgbGen wave sin .7 .3 .25 .5
//		tcMod stretch sin .9 0.03 0 0.5
//		rgbGen wave sin .7 .07 .25 .5
//	}	
}

// From scripts/gfx.shader
waterBubble
{
	sort	underwater
	cull none
	entityMergable		// allow all the sprites to be merged together
	{
		map sprites/bubble.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen		vertex
		alphaGen	vertex
	}
}

// From scripts/gfx.shader
flareShader
{
	nofog		// don't fog
	nopicmip	// use hi-res tex all the time
	cull none
	{

	///////////
	//
	//	IF YOU CHANGE SHIT, COMMENT WHAT THE xxxx YOU ARE DOING!!!!!!!!
	//
	//////////

//		map gfx/misc/flare.tga
//		map gfx/misc/flare2.tga
//		map gfx/misc/flare3.tga
		map gfx/misc/flare5.tga	// new one made by SA
//		map gfx/misc/flare5b.tga	// same, just made for additive blend (nofog required)

//		blendFunc GL_ONE GL_ONE
///		blendFunc blend
//		blendFunc GL_ONE_MINUS_SRC_ALPHA GL_SRC_ALPHA
		blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
	}
}

// From scripts/gfx.shader
gfx/2d/crosshairb
{
	nopicmip
	{
		map gfx/2d/crosshairb.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
//		rgbGen identity
		rgbGen vertex
	}
}

// From scripts/gfx.shader
gfx/2d/crosshairc
{
	nopicmip
	{
		map gfx/2d/crosshairc.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
//		rgbGen identity
		rgbGen vertex
	}
}

// From scripts/gfx.shader
gfx/2d/crosshaird
{
	nopicmip
	{
		map gfx/2d/crosshaird.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
//		rgbGen identity
		rgbGen vertex
	}
}

// From scripts/gfx.shader
gfx/2d/crosshaire
{
	nopicmip
	{
		map gfx/2d/crosshaire.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
//		rgbGen identity
		rgbGen vertex
	}
}

// From scripts/gfx.shader
gfx/2d/crosshairf
{
	nopicmip
	{
		map gfx/2d/crosshairf.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
//		rgbGen identity
		rgbGen vertex
	}
}

// From scripts/gfx.shader
gfx/2d/crosshairg
{
	nopicmip
	{
		map gfx/2d/crosshairg.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
//		rgbGen identity
		rgbGen vertex
	}
}

// From scripts/gfx.shader
gfx/2d/crosshairh
{
	nopicmip
	{
		map gfx/2d/crosshairh.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
//		rgbGen identity
		rgbGen vertex
	}
}

// From scripts/gfx.shader
gfx/2d/crosshairi
{
	nopicmip
	{
		map gfx/2d/crosshairi.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
//		rgbGen identity
		rgbGen vertex
	}

}

// From scripts/gfx.shader
gfx/2d/crosshairj
{
	nopicmip
	{
		map gfx/2d/crosshairj.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
//		rgbGen identity
		rgbGen vertex
	}
}

// From scripts/gfx.shader
gfx/2d/hudchars
{
	nopicmip
	nomipmaps
	{
		map gfx/2d/hudchars.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbgen vertex
	}
}

// From scripts/gfx.shader
gfx/2d/bigchars
{
	nopicmip
	nomipmaps
	{
		map gfx/2d/bigchars.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
	}
}

// From scripts/menu.shader
menu/art/unknownmap
{
	nocompress
	nopicmip
	nomipmaps
	{
		map levelshots/unknownmap.tga
	}
}

// From scripts/solo.shader
sunflare1
{
	cull none
	{
		map gfx/misc/sunflare1.tga
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}
