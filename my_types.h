#ifndef __MY_TYPES__
#define __MY_TYPES__

/*
64 bit machine?
*/
#if defined(__LP64__) || defined(_LP64) || defined(__64BIT__) || defined(__powerpc64__) \
	|| defined(_WIN64) || defined(_M_IA64) || (defined(_MIPS_SZLONG) && _MIPS_SZLONG == 64)
#    define ARC_64BIT
#endif

/*
int types
*/
#ifdef _MSC_VER
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#    include <inttypes.h>
#endif

typedef int8_t BMP8;
typedef uint8_t UBMP8;
typedef int16_t BMP16;
typedef uint16_t UBMP16;
typedef int32_t BMP32;
typedef uint32_t UBMP32;
typedef int64_t BMP64;
typedef uint64_t UBMP64;

/*
cache line memory alignment (64 bytes)
*/
#if defined (__GNUC__)
#	define CACHE_ALIGN  __attribute__ ((aligned(64)))
#else
#	define CACHE_ALIGN __declspec(align(64))
#endif

/*
Intrinsic popcnt
*/
#if defined(HAS_POPCNT) && defined(ARC_64BIT)
#   if defined (__GNUC__)
#       define popcnt(x)								\
	({													\
	typeof(x) __ret;									\
	__asm__("popcnt %1, %0" : "=r" (__ret) : "r" (x));	\
	__ret;							                    \
})
#   elif defined (_MSC_VER)
#       include<intrin.h>
#       define popcnt(b) __popcnt64(b)
#   else
#       include <nmmintrin.h>
#       define popcnt(b) _mm_popcnt_u64(b)
#   endif
#   define popcnt_sparse(b) popcnt(b)
#endif

/*
Os stuff
*/
#ifdef _MSC_VER
#    include <windows.h>
#    include <io.h>
#    include <conio.h>
#    undef CDECL
#    define CDECL __cdecl
#    define UINT64(x) (x##ui64)
#    define FMTU64   "%016I64x"
#    define FMT64    "%I64d"
#    define FMT64W   "%20I64d"
#    define FORCEINLINE __forceinline
#else
#    include <unistd.h>
#    include <dlfcn.h>
#    define CDECL
#    ifdef ARC_64BIT
#        define UINT64
#    else
#        define UINT64(x) (x##ULL)
#    endif
#    define FMTU64     "%016llx"
#    define FMT64      "%lld"
#    define FMT64W     "%20lld"
#    define FORCEINLINE __inline
#endif

/*threads*/
#    ifdef _MSC_VER
#        include <process.h>
#        define pthread_t HANDLE
#        define t_create(f,p,t) t = (pthread_t) _beginthread(f,0,(void*)p)
#        define t_sleep(x)    Sleep(x)
#    else
#        include <pthread.h>
#        define t_create(f,p,t) pthread_create(&t,0,(void*(*)(void*))&f,(void*)p)
#        define t_sleep(x)    usleep((x) * 1000)
#    endif

/*optional code*/
#ifdef PARALLEL
#	 define SMP_CODE(x) x
#else
#    define SMP_CODE(x)
#endif
#ifdef CLUSTER
#	 define CLUSTER_CODE(x) x
#else
#    define CLUSTER_CODE(x)
#endif
#ifdef _DEBUG
#	 define DEBUG_CODE(x) x
#else
#    define DEBUG_CODE(x)
#endif

#ifdef PARALLEL
#    define VOLATILE volatile
/*locks*/
#    ifdef _MSC_VER
#        ifdef USE_SPINLOCK
#             define LOCK VOLATILE int
#             define l_create(x)   ((x) = 0)
#             define l_lock(x)     while(InterlockedExchange((LPLONG)&(x),1) != 0) {while((x) != 0);}
#             define l_unlock(x)   ((x) = 0)
#        else
#             define LOCK CRITICAL_SECTION
#             define l_create(x)   InitializeCriticalSection(&x)
#             define l_lock(x)     EnterCriticalSection(&x)
#             define l_unlock(x)   LeaveCriticalSection(&x)
#        endif   
#    else
#			  define LOCK pthread_mutex_t
#			  define l_create(x)   pthread_mutex_init(&(x),0)
#			  define l_lock(x)     pthread_mutex_lock(&(x))
#			  define l_unlock(x)   pthread_mutex_unlock(&(x))
#    endif
#else
#    define VOLATILE
#    define l_lock(x)
#    define l_unlock(x)
#endif
/*
end
*/
#endif
