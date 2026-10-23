# Build log digest

- Log: `build.log`
- Lines: 1299
- Errors: 129
- Warnings: 197

**Verdict: the build failed, the errors are below.**

## Errors, first 40

```
code\client\cl_keys.cpp(1744,25): error: use of undeclared identifier 'uivm'
code\client\cl_keys.cpp(1744,31): error: use of undeclared identifier 'UI_GET_ACTIVE_MENU'
code\client\cl_keys.cpp(1759,13): error: use of undeclared identifier 'cgvm'
code\client\cl_keys.cpp(1759,19): error: use of undeclared identifier 'CG_EVENT_HANDLING'
code\client\cl_keys.cpp(1765,14): error: use of undeclared identifier 'uivm'
code\client\cl_keys.cpp(1765,20): error: use of undeclared identifier 'UI_SET_ACTIVE_MENU'
code\client\cl_keys.cpp(1769,14): error: use of undeclared identifier 'uivm'
code\client\cl_keys.cpp(1769,20): error: use of undeclared identifier 'UI_SET_ACTIVE_MENU'
code\client\cl_keys.cpp(1778,12): error: use of undeclared identifier 'uivm'
code\client\cl_keys.cpp(1778,18): error: use of undeclared identifier 'UI_KEY_EVENT'
code\client\cl_keys.cpp(1797,41): error: use of undeclared identifier 'uivm'
code\client\cl_keys.cpp(1798,13): error: use of undeclared identifier 'uivm'
code\client\cl_keys.cpp(1798,19): error: use of undeclared identifier 'UI_KEY_EVENT'
code\client\cl_keys.cpp(1799,51): error: use of undeclared identifier 'cgvm'
code\client\cl_keys.cpp(1800,13): error: use of undeclared identifier 'cgvm'
code\client\cl_keys.cpp(1800,19): error: use of undeclared identifier 'CG_KEY_EVENT'
code\client\cl_keys.cpp(1826,20): error: use of undeclared identifier 'uivm'
code\client\cl_keys.cpp(1826,26): error: use of undeclared identifier 'UI_GET_ACTIVE_MENU'
code\client\cl_keys.cpp(1838,8): error: use of undeclared identifier 'uivm'
code\client\cl_keys.cpp(1839,13): error: use of undeclared identifier 'uivm'
code\client\cl_keys.cpp(1839,19): error: use of undeclared identifier 'UI_KEY_EVENT'
code\client\cl_keys.cpp(1843,8): error: use of undeclared identifier 'cgvm'
code\client\cl_keys.cpp(1844,13): error: use of undeclared identifier 'cgvm'
code\client\cl_keys.cpp(1844,19): error: use of undeclared identifier 'CG_KEY_EVENT'
code\client\cl_keys.cpp(1891,12): error: use of undeclared identifier 'uivm'
code\client\cl_keys.cpp(1891,18): error: use of undeclared identifier 'UI_KEY_EVENT'
scons: *** [build\release\wolf\code\client\cl_keys.obj] Error 1
code\game\cgame\cg_import.cpp(41,37): error: use of undeclared identifier 'CG_MILLISECONDS'
code\game\cgame\cg_import.cpp(61,29): error: use of undeclared identifier 'CG_ARGC'
code\game\cgame\cg_import.cpp(101,30): error: use of undeclared identifier 'CG_UPDATESCREEN'
code\game\cgame\cg_import.cpp(109,43): error: use of undeclared identifier 'CG_CM_NUMINLINEMODELS'
code\game\cgame\cg_import.cpp(210,39): error: use of undeclared identifier 'PASSFLOAT'
code\game\cgame\cg_import.cpp(214,32): error: use of undeclared identifier 'PASSFLOAT'
code\game\cgame\cg_import.cpp(258,30): error: use of undeclared identifier 'CG_R_CLEARSCENE'
code\game\cgame\cg_import.cpp(278,40): error: use of undeclared identifier 'PASSFLOAT'
code\game\cgame\cg_import.cpp(278,64): error: use of undeclared identifier 'PASSFLOAT'
code\game\cgame\cg_import.cpp(278,80): error: use of undeclared identifier 'PASSFLOAT'
code\game\cgame\cg_import.cpp(278,96): error: use of undeclared identifier 'PASSFLOAT'
code\game\cgame\cg_import.cpp(282,41): error: use of undeclared identifier 'PASSFLOAT'
code\game\cgame\cg_import.cpp(282,57): error: use of undeclared identifier 'PASSFLOAT'
```

## Errors that repeat, top 30

| Count | Error |
| --- | --- |
| 40 | code\game\cgame\cg_import.cpp(N,N): error: use of undeclared identifier 'file' |
| 30 | code\game\ui\ui_import.cpp(N,N): error: use of undeclared identifier 'file' |
| 26 | code\client\cl_keys.cpp(N,N): error: use of undeclared identifier 'file' |
| 23 | code\game\game\g_import.cpp(N,N): error: use of undeclared identifier 'file' |
| 1 | scons: *** [build\release\wolf\code\client\cl_keys.obj] Error N |
| 1 | code\game\cgame\cg_import.cpp(N,N): error: expected unqualified-id |
| 1 | scons: *** [build\release\cgame\code\game\cgame\cg_import.obj] Error N |
| 1 | code\game\game\g_import.cpp(N,N): error: use of undeclared identifier 'file'; did you mean 'file'? |
| 1 | code\game\game\g_import.cpp(N,N): error: too many arguments to function call, expected N, have N |
| 1 | code\game\game\g_import.cpp(N,N): error: expected unqualified-id |
| 1 | Ncode\game\game\g_import.cpp(N,N): error: use of undeclared identifier 'file' |
| 1 | scons: *** [build\release\qagame\code\game\game\g_import.obj] Error N |
| 1 | N \| static voidcode\game\ui\ui_import.cpp(N,N): error: use of undeclared identifier 'file' |
| 1 | scons: *** [build\release\ui\code\game\ui\ui_import.obj] Error N |

## Warnings that repeat, top 30

| Count | Warning |
| --- | --- |
| 19 | code\game\ui\ui_main.cpp(N,N): warning: unused function 'file' [-Wunused-function] |
| 18 | code\client\cl_keys.cpp(N,N): warning: illegal character encoding in string literal [-Winvalid-source-encoding] |
| 11 | code\game\cgame\cg_weapons.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 8 | code\qcommon\unzip.cpp(N,N): warning: misleading indentation; statement is not part of the previous 'file' [-Wmisleading-indentation] |
| 6 | code\client\cl_main.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 6 | code\game\ui\ui_main.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 6 | code\game\ui\ui_players.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 5 | code\game\cgame\cg_ents.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 5 | code\game\cgame\cg_players.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 4 | code\qcommon\cm_trace.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 4 | code\sys\winN\win_input.cpp(N,N): warning: unused variable 'file' [-Wunused-variable] |
| 4 | code\game\game\g_weapon.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 3 | code\qcommon\common.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 3 | code\game\cgame\cg_event.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 3 | code\game\cgame\cg_flamethrower.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 3 | code\game\cgame\cg_particles.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 3 | code\game\cgame\cg_servercmds.cpp(N,N): warning: address of array 'file' will always evaluate to 'file' [-Wpointer-bool-conversion] |
| 3 | code\renderer\tr_image.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 2 | code\client\cl_input.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 2 | code\server\sv_game.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 2 | code\server\sv_world.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 2 | code\qcommon\unzip.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 2 | code\qcommon\unzip.cpp(N,N): warning: unused variable 'file' [-Wunused-const-variable] |
| 2 | code\sys\winN\win_net.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 2 | code\game\cgame\cg_localents.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 2 | code\game\cgame\cg_main.cpp(N,N): warning: unused function 'file' [-Wunused-function] |
| 2 | code\game\cgame\cg_newDraw.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 2 | code\game\cgame\cg_view.cpp(N,N): warning: explicitly assigning value of variable of type 'file' to itself [-Wself-assign] |
| 2 | code\game\game\g_active.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |
| 2 | code\game\game\g_main.cpp(N,N): warning: variable 'file' set but not used [-Wunused-but-set-variable] |

