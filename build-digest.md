# Build log digest

- Log: `build.log`
- Lines: 32
- Errors: 7
- Warnings: 0

**Verdict: the build failed, the errors are below.**

## Errors, first 40

```
code\client\cl_cgame.cpp(940,2): error: cannot initialize a member subobject of type 'void (*)(trace_t *, const vec3_t *, const vec3_t *, const vec3_t *, const vec3_t *, clipHandle_t, int)' (aka 'void (*)(trace_t *, const float (*)[3], const float (*)[3], const float (*)[3], const float (*)[3], int, int)') with an lvalue of type 'void (trace_t *, const vec_t *, const vec_t *, const vec_t *, const vec_t *, clipHandle_t, int)' (aka 'void (trace_t *, const float *, const float *, const float *, const float *, int, int)'): type mismatch at 2nd parameter ('const vec3_t *' (aka 'const float (*)[3]') vs 'const vec_t *' (aka 'const float *'))
code\client\cl_cgame.cpp(941,2): error: cannot initialize a member subobject of type 'void (*)(trace_t *, const vec3_t *, const vec3_t *, const vec3_t *, const vec3_t *, clipHandle_t, int, const vec3_t *, const vec3_t *)' (aka 'void (*)(trace_t *, const float (*)[3], const float (*)[3], const float (*)[3], const float (*)[3], int, int, const float (*)[3], const float (*)[3])') with an lvalue of type 'void (trace_t *, const vec_t *, const vec_t *, const vec_t *, const vec_t *, clipHandle_t, int, const vec_t *, const vec_t *)' (aka 'void (trace_t *, const float *, const float *, const float *, const float *, int, int, const float *, const float *)'): type mismatch at 2nd parameter ('const vec3_t *' (aka 'const float (*)[3]') vs 'const vec_t *' (aka 'const float *'))
code\client\cl_cgame.cpp(942,2): error: cannot initialize a member subobject of type 'void (*)(trace_t *, const vec3_t *, const vec3_t *, const vec3_t *, const vec3_t *, clipHandle_t, int)' (aka 'void (*)(trace_t *, const float (*)[3], const float (*)[3], const float (*)[3], const float (*)[3], int, int)') with an lvalue of type 'void (trace_t *, const vec_t *, const vec_t *, const vec_t *, const vec_t *, clipHandle_t, int)' (aka 'void (trace_t *, const float *, const float *, const float *, const float *, int, int)'): type mismatch at 2nd parameter ('const vec3_t *' (aka 'const float (*)[3]') vs 'const vec_t *' (aka 'const float *'))
code\client\cl_cgame.cpp(943,2): error: cannot initialize a member subobject of type 'void (*)(trace_t *, const vec3_t *, const vec3_t *, const vec3_t *, const vec3_t *, clipHandle_t, int, const vec3_t *, const vec3_t *)' (aka 'void (*)(trace_t *, const float (*)[3], const float (*)[3], const float (*)[3], const float (*)[3], int, int, const float (*)[3], const float (*)[3])') with an lvalue of type 'void (trace_t *, const vec_t *, const vec_t *, const vec_t *, const vec_t *, clipHandle_t, int, const vec_t *, const vec_t *)' (aka 'void (trace_t *, const float *, const float *, const float *, const float *, int, int, const float *, const float *)'): type mismatch at 2nd parameter ('const vec3_t *' (aka 'const float (*)[3]') vs 'const vec_t *' (aka 'const float *'))
code\client\cl_cgame.cpp(944,2): error: cannot initialize a member subobject of type 'int (*)(int, const vec3_t *, const vec3_t *, int, vec_t *, int, markFragment_t *)' (aka 'int (*)(int, const float (*)[3], const float (*)[3], int, float *, int, markFragment_t *)') with an lvalue of type 'int (int, const vec3_t *, const vec_t *, int, vec_t *, int, markFragment_t *)' (aka 'int (int, const float (*)[3], const float *, int, float *, int, markFragment_t *)'): type mismatch at 3rd parameter ('const vec3_t *' (aka 'const float (*)[3]') vs 'const vec_t *' (aka 'const float *'))
code\client\cl_cgame.cpp(1022,2): error: cannot initialize a member subobject of type 'void (*)(int, int, int, int, const byte *)' (aka 'void (*)(int, int, int, int, const unsigned char *)') with an lvalue of type 'void (int, int, int, const byte *)' (aka 'void (int, int, int, const unsigned char *)'): different number of parameters (5 vs 4)
scons: *** [build\release\wolf\code\client\cl_cgame.obj] Error 1
```

## Errors that repeat, top 30

| Count | Error |
| --- | --- |
| 4 | code\client\cl_cgame.cpp(N,N): error: cannot initialize a member subobject of type 'file' (aka 'file') with an lvalue of type 'file' (aka 'file'): type mismatch at Nnd parameter ('file' (aka 'file') vs 'file' (aka 'file')) |
| 1 | code\client\cl_cgame.cpp(N,N): error: cannot initialize a member subobject of type 'file' (aka 'file') with an lvalue of type 'file' (aka 'file'): type mismatch at Nrd parameter ('file' (aka 'file') vs 'file' (aka 'file')) |
| 1 | code\client\cl_cgame.cpp(N,N): error: cannot initialize a member subobject of type 'file' (aka 'file') with an lvalue of type 'file' (aka 'file'): different number of parameters (N vs N) |
| 1 | scons: *** [build\release\wolf\code\client\cl_cgame.obj] Error N |

