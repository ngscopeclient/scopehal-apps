#if defined(unix) || defined(__unix__) || defined(__unix)
#include <pthread.h>
#endif

void pthread_setname_np_compat(const char *name) {
#if defined(unix) || defined(__unix__) || defined(__unix)
	#if __linux__
		// on Linux, max 16 chars including \0, see man page
		pthread_setname_np(pthread_self(), name);
	#else
		// BSD, including Apple
		pthread_setname_np(name);
	#endif
#endif
}
