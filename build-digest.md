# Build log digest

- Log: `build.log`
- Lines: 17
- Errors: 2
- Warnings: 0

**Verdict: the build failed, the errors are below.**

## Errors, first 40

```
code\client\cl_cgame.cpp(312,3): error: use of undeclared identifier 'Com_Sprintf'
scons: *** [build\release\wolf\code\client\cl_cgame.obj] Error 1
```

## Errors that repeat, top 30

| Count | Error |
| --- | --- |
| 1 | code\client\cl_cgame.cpp(N,N): error: use of undeclared identifier 'file' |
| 1 | scons: *** [build\release\wolf\code\client\cl_cgame.obj] Error N |

