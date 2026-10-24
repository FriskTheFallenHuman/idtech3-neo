# Build log digest

- Log: `build.log`
- Lines: 509
- Errors: 19
- Warnings: 1

**Verdict: the build failed, the errors are below.**

## Errors, first 40

```
renderer.lib(tr_init.obj) : error LNK2019: unresolved external symbol "void __cdecl R_SkinList_f(void)" (?R_SkinList_f@@YAXXZ) referenced in function "void __cdecl R_Register(void)" (?R_Register@@YAXXZ)
renderer.lib(tr_init.obj) : error LNK2019: unresolved external symbol "void __cdecl R_CropImages_f(void)" (?R_CropImages_f@@YAXXZ) referenced in function "void __cdecl R_Register(void)" (?R_Register@@YAXXZ)
renderer.lib(tr_init.obj) : error LNK2019: unresolved external symbol "void __cdecl R_InitSkins(void)" (?R_InitSkins@@YAXXZ) referenced in function "void __cdecl R_Init(void)" (?R_Init@@YAXXZ)
renderer.lib(tr_init.obj) : error LNK2019: unresolved external symbol "void __cdecl R_PurgeBackupImages(int)" (?R_PurgeBackupImages@@YAXH@Z) referenced in function "void __cdecl RE_Shutdown(int)" (?RE_Shutdown@@YAXH@Z)
renderer.lib(tr_main.obj) : error LNK2001: unresolved external symbol "void __cdecl R_PurgeBackupImages(int)" (?R_PurgeBackupImages@@YAXH@Z)
renderer.lib(tr_init.obj) : error LNK2019: unresolved external symbol "void __cdecl R_BackupImages(void)" (?R_BackupImages@@YAXXZ) referenced in function "void __cdecl RE_Shutdown(int)" (?RE_Shutdown@@YAXH@Z)
renderer.lib(tr_init.obj) : error LNK2019: unresolved external symbol "int __cdecl RE_RegisterSkin(char const *)" (?RE_RegisterSkin@@YAHPBD@Z) referenced in function "struct refexport_t * __cdecl GetRefAPI(int,struct refimport_t *)" (?GetRefAPI@@YAPAUrefexport_t@@HPAUrefimport_t@@@Z)
renderer.lib(tr_image.obj) : error LNK2019: unresolved external symbol "void * __cdecl R_CacheImageAlloc(int)" (?R_CacheImageAlloc@@YAPAXH@Z) referenced in function "struct image_s * __cdecl R_CreateImageExt(char const *,unsigned char const *,int,int,int,int,int,int)" (?R_CreateImageExt@@YAPAUimage_s@@PBDPBEHHHHHH@Z)
renderer.lib(tr_image.obj) : error LNK2019: unresolved external symbol "void __cdecl R_FindFreeTexnum(struct image_s *)" (?R_FindFreeTexnum@@YAXPAUimage_s@@@Z) referenced in function "struct image_s * __cdecl R_CreateImageExt(char const *,unsigned char const *,int,int,int,int,int,int)" (?R_CreateImageExt@@YAPAUimage_s@@PBDPBEHHHHHH@Z)
renderer.lib(tr_image.obj) : error LNK2019: unresolved external symbol "struct image_s * __cdecl R_FindCachedImage(char const *,int)" (?R_FindCachedImage@@YAPAUimage_s@@PBDH@Z) referenced in function "struct image_s * __cdecl R_FindImageFileExt(char const *,int,int,int,int)" (?R_FindImageFileExt@@YAPAUimage_s@@PBDHHHH@Z)
renderer.lib(tr_image.obj) : error LNK2019: unresolved external symbol "void __cdecl R_InitTexnumImages(int)" (?R_InitTexnumImages@@YAXH@Z) referenced in function "void __cdecl R_InitImages(void)" (?R_InitImages@@YAXXZ)
renderer.lib(tr_image.obj) : error LNK2019: unresolved external symbol "void __cdecl R_LoadCacheImages(void)" (?R_LoadCacheImages@@YAXXZ) referenced in function "void __cdecl R_InitImages(void)" (?R_InitImages@@YAXXZ)
renderer.lib(tr_shader.obj) : error LNK2019: unresolved external symbol "int __cdecl R_TouchImage(struct image_s *)" (?R_TouchImage@@YAHPAUimage_s@@@Z) referenced in function "struct shader_s * __cdecl R_FindCachedShader(char const *,int,int)" (?R_FindCachedShader@@YAPAUshader_s@@PBDHH@Z)
renderer.lib(tr_cmesh.obj) : error LNK2019: unresolved external symbol "struct skin_s * __cdecl R_GetSkinByHandle(int)" (?R_GetSkinByHandle@@YAPAUskin_s@@H@Z) referenced in function "void __cdecl R_AddMDCSurfaces(struct trRefEntity_t *)" (?R_AddMDCSurfaces@@YAXPAUtrRefEntity_t@@@Z)
renderer.lib(tr_mesh.obj) : error LNK2001: unresolved external symbol "struct skin_s * __cdecl R_GetSkinByHandle(int)" (?R_GetSkinByHandle@@YAPAUskin_s@@H@Z)
renderer.lib(tr_animation.obj) : error LNK2001: unresolved external symbol "struct skin_s * __cdecl R_GetSkinByHandle(int)" (?R_GetSkinByHandle@@YAPAUskin_s@@H@Z)
renderer.lib(sys_png.obj) : error LNK2019: unresolved external symbol "unsigned char * __cdecl R_AllocateImagePixels(int)" (?R_AllocateImagePixels@@YAPAEH@Z) referenced in function "void __cdecl R_LoadPNG(char const *,unsigned char * *,int *,int *)" (?R_LoadPNG@@YAXPBDPAPAEPAH2@Z)
bin\Doom.exe : fatal error LNK1120: 14 unresolved externals
scons: *** [bin\Doom.exe] Error 1120
```

## Errors that repeat, top 30

| Count | Error |
| --- | --- |
| 1 | renderer.lib(tr_init.obj) : error LNKN: unresolved external symbol 'file' (?R_SkinList_f@@YAXXZ) referenced in function 'file' (?R_Register@@YAXXZ) |
| 1 | renderer.lib(tr_init.obj) : error LNKN: unresolved external symbol 'file' (?R_CropImages_f@@YAXXZ) referenced in function 'file' (?R_Register@@YAXXZ) |
| 1 | renderer.lib(tr_init.obj) : error LNKN: unresolved external symbol 'file' (?R_InitSkins@@YAXXZ) referenced in function 'file' (?R_Init@@YAXXZ) |
| 1 | renderer.lib(tr_init.obj) : error LNKN: unresolved external symbol 'file' (?R_PurgeBackupImages@@YAXH@Z) referenced in function 'file' (?RE_Shutdown@@YAXH@Z) |
| 1 | renderer.lib(tr_main.obj) : error LNKN: unresolved external symbol 'file' (?R_PurgeBackupImages@@YAXH@Z) |
| 1 | renderer.lib(tr_init.obj) : error LNKN: unresolved external symbol 'file' (?R_BackupImages@@YAXXZ) referenced in function 'file' (?RE_Shutdown@@YAXH@Z) |
| 1 | renderer.lib(tr_init.obj) : error LNKN: unresolved external symbol 'file' (?RE_RegisterSkin@@YAHPBD@Z) referenced in function 'file' (?GetRefAPI@@YAPAUrefexport_t@@HPAUrefimport_t@@@Z) |
| 1 | renderer.lib(tr_image.obj) : error LNKN: unresolved external symbol 'file' (?R_CacheImageAlloc@@YAPAXH@Z) referenced in function 'file' (?R_CreateImageExt@@YAPAUimage_s@@PBDPBEHHHHHH@Z) |
| 1 | renderer.lib(tr_image.obj) : error LNKN: unresolved external symbol 'file' (?R_FindFreeTexnum@@YAXPAUimage_s@@@Z) referenced in function 'file' (?R_CreateImageExt@@YAPAUimage_s@@PBDPBEHHHHHH@Z) |
| 1 | renderer.lib(tr_image.obj) : error LNKN: unresolved external symbol 'file' (?R_FindCachedImage@@YAPAUimage_s@@PBDH@Z) referenced in function 'file' (?R_FindImageFileExt@@YAPAUimage_s@@PBDHHHH@Z) |
| 1 | renderer.lib(tr_image.obj) : error LNKN: unresolved external symbol 'file' (?R_InitTexnumImages@@YAXH@Z) referenced in function 'file' (?R_InitImages@@YAXXZ) |
| 1 | renderer.lib(tr_image.obj) : error LNKN: unresolved external symbol 'file' (?R_LoadCacheImages@@YAXXZ) referenced in function 'file' (?R_InitImages@@YAXXZ) |
| 1 | renderer.lib(tr_shader.obj) : error LNKN: unresolved external symbol 'file' (?R_TouchImage@@YAHPAUimage_s@@@Z) referenced in function 'file' (?R_FindCachedShader@@YAPAUshader_s@@PBDHH@Z) |
| 1 | renderer.lib(tr_cmesh.obj) : error LNKN: unresolved external symbol 'file' (?R_GetSkinByHandle@@YAPAUskin_s@@H@Z) referenced in function 'file' (?R_AddMDCSurfaces@@YAXPAUtrRefEntity_t@@@Z) |
| 1 | renderer.lib(tr_mesh.obj) : error LNKN: unresolved external symbol 'file' (?R_GetSkinByHandle@@YAPAUskin_s@@H@Z) |
| 1 | renderer.lib(tr_animation.obj) : error LNKN: unresolved external symbol 'file' (?R_GetSkinByHandle@@YAPAUskin_s@@H@Z) |
| 1 | renderer.lib(sys_png.obj) : error LNKN: unresolved external symbol 'file' (?R_AllocateImagePixels@@YAPAEH@Z) referenced in function 'file' (?R_LoadPNG@@YAXPBDPAPAEPAHN@Z) |
| 1 | bin\Doom.exe : fatal error LNKN: N unresolved externals |
| 1 | scons: *** [bin\Doom.exe] Error N |

## Warnings that repeat, top 30

| Count | Warning |
| --- | --- |
| 1 | code\renderer\tr_image.cpp(N,N): warning: unused function 'file' [-Wunused-function] |

