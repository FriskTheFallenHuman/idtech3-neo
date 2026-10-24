# Build log digest

- Log: `build.log`
- Lines: 78
- Errors: 7
- Warnings: 0

**Verdict: the build failed, the errors are below.**

## Errors, first 40

```
code\sys\win32\win_net.cpp(31,10): error: '../../game/q_shared.h' file not found, did you mean 'game/q_shared.h'?
code\sys\sys_main.cpp(294,3): error: no matching function for call to 'Sys_ListFilteredFiles'
code\sys\sys_main.cpp(304,8): error: no matching function for call to 'Com_FilterPath'
code\sys\sys_main.cpp(581,2): error: no matching function for call to 'SDL_UnloadObject'
code\sys\sys_main.cpp(662,9): error: cannot initialize a variable of type 'char *' with an rvalue of type 'const char *'
scons: *** [build\release\wolf\code\sys\sys_main.obj] Error 1
scons: *** [build\release\wolf\code\sys\win32\win_net.obj] Error 1
```

## Errors that repeat, top 30

| Count | Error |
| --- | --- |
| 3 | code\sys\sys_main.cpp(N,N): error: no matching function for call to 'file' |
| 1 | code\sys\winN\win_net.cpp(N,N): error: 'file' file not found, did you mean 'file'? |
| 1 | code\sys\sys_main.cpp(N,N): error: cannot initialize a variable of type 'file' with an rvalue of type 'file' |
| 1 | scons: *** [build\release\wolf\code\sys\sys_main.obj] Error N |
| 1 | scons: *** [build\release\wolf\code\sys\winN\win_net.obj] Error N |

