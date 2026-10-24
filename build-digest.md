# Build log digest

- Log: `build.log`
- Lines: 345
- Errors: 4
- Warnings: 0

**Verdict: the build failed, the errors are below.**

## Errors, first 40

```
code\renderer\tr_image.cpp(122,20): error: use of undeclared identifier 's_gammatable'
code\renderer\tr_image.cpp(123,20): error: use of undeclared identifier 's_gammatable'
code\renderer\tr_image.cpp(124,20): error: use of undeclared identifier 's_gammatable'
scons: *** [build\release\renderer\code\renderer\tr_image.obj] Error 1
```

## Errors that repeat, top 30

| Count | Error |
| --- | --- |
| 3 | code\renderer\tr_image.cpp(N,N): error: use of undeclared identifier 'file' |
| 1 | scons: *** [build\release\renderer\code\renderer\tr_image.obj] Error N |

