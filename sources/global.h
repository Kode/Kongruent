#ifndef KONG_GLOBAL_HEADER
#define KONG_GLOBAL_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
#define INIT_ZERO \
	{}
#else
#define INIT_ZERO \
	{ 0 }
#endif

#ifdef __cplusplus
}
#endif

#endif
