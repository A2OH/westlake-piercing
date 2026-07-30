#ifndef MUSL_COMPAT_H
#define MUSL_COMPAT_H
/* glibc globals AOSP libbase/liblog reference; defined in musl_compat.c */
#ifdef __cplusplus
extern "C" {
#endif
extern char *program_invocation_name;
extern char *program_invocation_short_name;
#ifdef __cplusplus
}
#endif
#endif
