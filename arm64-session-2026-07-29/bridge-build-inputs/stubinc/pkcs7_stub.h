#ifndef PKCS7_STUB_H
#define PKCS7_STUB_H
// boringssl removed PKCS7_verify; adapter used it with NOVERIFY|NOSIGS (no real check).
#define PKCS7_verify(a,b,c,d,e,f) (1)
#endif
