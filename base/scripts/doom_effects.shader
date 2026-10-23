// Generic particle and blood media ported from Hexen2.

doom/fx/glow
{
    cull disable
    surfaceparm trans
    surfaceparm nonsolid
    surfaceparm nolightmap
    sort additive
    {
        map gfx/doom/effects/glow.png
        blendFunc GL_SRC_ALPHA GL_ONE
        rgbGen vertex
        alphaGen vertex
    }
}

doom/fx/core
{
    cull disable
    surfaceparm trans
    surfaceparm nonsolid
    surfaceparm nolightmap
    sort additive
    {
        map gfx/doom/effects/core.png
        blendFunc GL_SRC_ALPHA GL_ONE
        rgbGen vertex
        alphaGen vertex
    }
}

doom/fx/spark
{
    cull disable
    surfaceparm trans
    surfaceparm nonsolid
    surfaceparm nolightmap
    sort additive
    {
        map gfx/doom/effects/spark.png
        blendFunc GL_SRC_ALPHA GL_ONE
        rgbGen vertex
        alphaGen vertex
    }
}

doom/fx/streak
{
    cull disable
    surfaceparm trans
    surfaceparm nonsolid
    surfaceparm nolightmap
    sort additive
    {
        map gfx/doom/effects/streak.png
        blendFunc GL_SRC_ALPHA GL_ONE
        rgbGen vertex
        alphaGen vertex
    }
}

doom/fx/smoke0
{
    cull disable
    surfaceparm trans
    surfaceparm nonsolid
    surfaceparm nolightmap
    sort additive
    {
        map gfx/doom/effects/smoke_0.png
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen vertex
        alphaGen vertex
    }
}

doom/fx/smoke1
{
    cull disable
    surfaceparm trans
    surfaceparm nonsolid
    surfaceparm nolightmap
    sort additive
    {
        map gfx/doom/effects/smoke_1.png
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen vertex
        alphaGen vertex
    }
}

doom/fx/blood_mist
{
    cull disable
    surfaceparm trans
    surfaceparm nonsolid
    surfaceparm nolightmap
    sort additive
    {
        map gfx/doom/effects/blood_mist.png
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen vertex
        alphaGen vertex
    }
}

doom/fx/droplet
{
    cull disable
    surfaceparm trans
    surfaceparm nonsolid
    surfaceparm nolightmap
    sort additive
    {
        map gfx/doom/effects/droplet.png
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen vertex
        alphaGen vertex
    }
}

doom/fx/blood_splat0
{
    cull disable
    surfaceparm trans
    surfaceparm nonsolid
    surfaceparm nolightmap
    polygonOffset
    sort decal
    {
        map gfx/doom/effects/blood_splat_0.png
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen vertex
        alphaGen vertex
    }
}

doom/fx/blood_splat1
{
    cull disable
    surfaceparm trans
    surfaceparm nonsolid
    surfaceparm nolightmap
    polygonOffset
    sort decal
    {
        map gfx/doom/effects/blood_splat_1.png
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen vertex
        alphaGen vertex
    }
}

doom/fx/blood_splat2
{
    cull disable
    surfaceparm trans
    surfaceparm nonsolid
    surfaceparm nolightmap
    polygonOffset
    sort decal
    {
        map gfx/doom/effects/blood_splat_2.png
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen vertex
        alphaGen vertex
    }
}

doom/fx/blood_splat3
{
    cull disable
    surfaceparm trans
    surfaceparm nonsolid
    surfaceparm nolightmap
    polygonOffset
    sort decal
    {
        map gfx/doom/effects/blood_splat_3.png
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen vertex
        alphaGen vertex
    }
}

doom/fx/bullet_mark
{
    cull disable
    sort decal
    {
        map gfx/damage/bullet_mrk.tga
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen vertex
        alphaGen vertex
    }
}
