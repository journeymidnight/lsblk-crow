/*
 * Fundamental C definitions.
 */

#ifndef UTIL_LINUX_C_H
#define UTIL_LINUX_C_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <assert.h>

#ifdef HAVE_ERR_H
# include <err.h>
#endif

#ifdef HAVE_SYS_SYSMACROS_H
# include <sys/sysmacros.h>     /* for major, minor */
#endif

#ifndef LOGIN_NAME_MAX
# define LOGIN_NAME_MAX 256
#endif

/*
 * Compiler-specific stuff
 */
#ifndef __GNUC_PREREQ
# if defined __GNUC__ && defined __GNUC_MINOR__
#  define __GNUC_PREREQ(maj, min) \
	((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
# else
#  define __GNUC_PREREQ(maj, min) 0
# endif
#endif

#ifdef __GNUC__

/* &a[0] degrades to a pointer: a different type from an array */
# define __must_be_array(a) \
	UL_BUILD_BUG_ON_ZERO(__builtin_types_compatible_p(__typeof__(a), __typeof__(&a[0])))

# define ignore_result(x) __extension__ ({ \
	__typeof__(x) __dummy __attribute__((__unused__)) = (x); (void) __dummy; \
})

#else /* !__GNUC__ */
# define __must_be_array(a)	0
# define __attribute__(_arg_)
# define ignore_result(x) ((void) (x))
#endif /* !__GNUC__ */

/*
 * Function attributes
 */
#ifndef __ul_alloc_size
# if __GNUC_PREREQ (4, 3)
#  define __ul_alloc_size(s) __attribute__((alloc_size(s), warn_unused_result))
# else
#  define __ul_alloc_size(s)
# endif
#endif

#ifndef __ul_calloc_size
# if __GNUC_PREREQ (4, 3)
#  define __ul_calloc_size(n, s) __attribute__((alloc_size(n, s), warn_unused_result))
# else
#  define __ul_calloc_size(n, s)
# endif
#endif

/*
 * Force a compilation error if condition is true, but also produce a
 * result (of value 0 and type size_t), so the expression can be used
 * e.g. in a structure initializer (or wherever else comma expressions
 * aren't permitted).
 */
#define UL_BUILD_BUG_ON_ZERO(e) __extension__ (sizeof(struct { int:-!!(e); }))
#define BUILD_BUG_ON_NULL(e) ((void *)sizeof(struct { int:-!!(e); }))

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))
#endif

#ifndef PATH_MAX
# define PATH_MAX 4096
#endif

#ifndef TRUE
# define TRUE 1
#endif

#ifndef FALSE
# define FALSE 0
#endif

#ifndef min
# define min(x, y) __extension__ ({		\
	__typeof__(x) _min1 = (x);		\
	__typeof__(y) _min2 = (y);		\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })
#endif

#ifndef max
# define max(x, y) __extension__ ({		\
	__typeof__(x) _max1 = (x);		\
	__typeof__(y) _max2 = (y);		\
	(void) (&_max1 == &_max2);		\
	_max1 > _max2 ? _max1 : _max2; })
#endif

#ifndef cmp_numbers
# define cmp_numbers(x, y) __extension__ ({	\
	__typeof__(x) _a = (x);			\
	__typeof__(y) _b = (y);			\
	(void) (&_a == &_b);			\
	_a == _b ? 0 : _a > _b ? 1 : -1; })
#endif

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#ifndef container_of
#define container_of(ptr, type, member) __extension__ ({	 \
	const __typeof__( ((type *)0)->member ) *__mptr = (ptr); \
	(type *)( (char *)__mptr - offsetof(type,member) );})
#endif

#ifndef HAVE_PROGRAM_INVOCATION_SHORT_NAME
# ifdef HAVE___PROGNAME
extern char *__progname;
#  define program_invocation_short_name __progname
# else
#  ifdef HAVE_GETEXECNAME
#   define program_invocation_short_name \
		prog_inv_sh_nm_from_file(getexecname(), 0)
#  else
#   define program_invocation_short_name \
		prog_inv_sh_nm_from_file(__FILE__, 1)
#  endif
static char prog_inv_sh_nm_buf[256];
static inline char *
prog_inv_sh_nm_from_file(char *f, char stripext)
{
	char *t;

	if ((t = strrchr(f, '/')) != NULL)
		t++;
	else
		t = f;

	strncpy(prog_inv_sh_nm_buf, t, sizeof(prog_inv_sh_nm_buf) - 1);
	prog_inv_sh_nm_buf[sizeof(prog_inv_sh_nm_buf) - 1] = '\0';

	if (stripext && (t = strrchr(prog_inv_sh_nm_buf, '.')) != NULL)
		*t = '\0';

	return prog_inv_sh_nm_buf;
}
# endif
#endif


#ifndef HAVE_ERR_H
static inline void
errmsg(char doexit, int excode, char adderr, const char *fmt, ...)
{
	fprintf(stderr, "%s: ", program_invocation_short_name);
	if (fmt != NULL) {
		va_list argp;
		va_start(argp, fmt);
		vfprintf(stderr, fmt, argp);
		va_end(argp);
		if (adderr)
			fprintf(stderr, ": ");
	}
	if (adderr)
		fprintf(stderr, "%m");
	fprintf(stderr, "\n");
	if (doexit)
		exit(excode);
}

#ifndef HAVE_ERR
# define err(E, FMT...) errmsg(1, E, 1, FMT)
#endif

#ifndef HAVE_ERRX
# define errx(E, FMT...) errmsg(1, E, 0, FMT)
#endif

#ifndef HAVE_WARN
# define warn(FMT...) errmsg(0, 0, 1, FMT)
#endif

#ifndef HAVE_WARNX
# define warnx(FMT...) errmsg(0, 0, 0, FMT)
#endif
#endif /* !HAVE_ERR_H */


/* Don't use inline function to avoid '#include "nls.h"' in c.h
 */
#define errtryhelp(eval) __extension__ ({ \
	fprintf(stderr, _("Try '%s --help' for more information.\n"), \
			program_invocation_short_name); \
	exit(eval); \
})

/* After failed execvp() */
#define EX_EXEC_FAILED		126	/* Program located, but not usable. */
#define EX_EXEC_ENOENT		127	/* Could not find program to exec.  */
#define errexec(name)	err(errno == ENOENT ? EX_EXEC_ENOENT : EX_EXEC_FAILED, \
			_("failed to execute %s"), name)


static inline __attribute__((const)) int is_power_of_2(unsigned long num)
{
	return (num != 0 && ((num & (num - 1)) == 0));
}

#ifndef HAVE_LOFF_T
typedef int64_t loff_t;
#endif

#if !defined(HAVE_DIRFD) && (!defined(HAVE_DECL_DIRFD) || HAVE_DECL_DIRFD == 0) && defined(HAVE_DIR_DD_FD)
#include <sys/types.h>
#include <dirent.h>
static inline int dirfd(DIR *d)
{
	return d->dd_fd;
}
#endif

/*
 * Fallback defines for old versions of glibc
 */
#include <fcntl.h>

#ifdef O_CLOEXEC
#define UL_CLOEXECSTR	"e"
#else
#define UL_CLOEXECSTR	""
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#ifdef __FreeBSD_kernel__
#ifndef F_DUPFD_CLOEXEC
#define F_DUPFD_CLOEXEC	17	/* Like F_DUPFD, but FD_CLOEXEC is set */
#endif
#endif


#ifndef AI_ADDRCONFIG
#define AI_ADDRCONFIG 0x0020
#endif

#ifndef IUTF8
#define IUTF8 0040000
#endif

/*
 * MAXHOSTNAMELEN replacement
 */
static inline size_t get_hostname_max(void)
{
	long len = sysconf(_SC_HOST_NAME_MAX);

	if (0 < len)
		return len;

#ifdef MAXHOSTNAMELEN
	return MAXHOSTNAMELEN;
#elif HOST_NAME_MAX
	return HOST_NAME_MAX;
#endif
	return 64;
}

/*
 * The usleep function was marked obsolete in POSIX.1-2001 and was removed
 * in POSIX.1-2008.  It was replaced with nanosleep() that provides more
 * advantages (like no interaction with signals and other timer functions).
 */
#include <time.h>

static inline int xusleep(useconds_t usec)
{
	struct timespec waittime = {
		.tv_sec   =  usec / 1000000L,
		.tv_nsec  = (usec % 1000000L) * 1000
	};
	return nanosleep(&waittime, NULL);
}

/*
 * Constant strings for usage() functions. For more info see
 * Documentation/{howto-usage-function.txt,boilerplate.c}
 */
#define USAGE_HEADER     _("\nUsage:\n")
#define USAGE_OPTIONS    _("\nOptions:\n")
#define USAGE_FUNCTIONS  _("\nFunctions:\n")
#define USAGE_COMMANDS   _("\nCommands:\n")
#define USAGE_COLUMNS    _("\nAvailable output columns:\n")
#define USAGE_SEPARATOR    "\n"

#define USAGE_OPTSTR_HELP     _("display this help")
#define USAGE_OPTSTR_VERSION  _("display version")

#define USAGE_HELP_OPTIONS(marg_dsc) \
		"%-" #marg_dsc "s%s\n" \
		"%-" #marg_dsc "s%s\n" \
		, " -h, --help",    USAGE_OPTSTR_HELP \
		, " -V, --version", USAGE_OPTSTR_VERSION

#define USAGE_MAN_TAIL(_man)   _("\nFor more details see %s.\n"), _man

#define UTIL_LINUX_VERSION _("%s from %s\n"), program_invocation_short_name, PACKAGE_STRING

/*
 * scanf modifiers for "strings allocation"
 */
#ifdef HAVE_SCANF_MS_MODIFIER
#define UL_SCNsA	"%ms"
#elif defined(HAVE_SCANF_AS_MODIFIER)
#define UL_SCNsA	"%as"
#endif

/*
 * seek stuff
 */
#ifndef SEEK_DATA
# define SEEK_DATA	3
#endif
#ifndef SEEK_HOLE
# define SEEK_HOLE	4
#endif


/*
 * Macros to convert #define'itions to strings, for example
 * #define XYXXY 42
 * printf ("%s=%s\n", stringify(XYXXY), stringify_value(XYXXY));
 */
#define stringify_value(s) stringify(s)
#define stringify(s) #s

/*
 * UL_ASAN_BLACKLIST is a macro to tell AddressSanitizer (a compile-time
 * instrumentation shipped with Clang and GCC) to not instrument the
 * annotated function.  Furthermore, it will prevent the compiler from
 * inlining the function because inlining currently breaks the blacklisting
 * mechanism of AddressSanitizer.
 */
#if defined(__has_feature)
# if __has_feature(address_sanitizer)
#  define UL_ASAN_BLACKLIST __attribute__((noinline)) __attribute__((no_sanitize_memory)) __attribute__((no_sanitize_address))
# else
#  define UL_ASAN_BLACKLIST	/* nothing */
# endif
#else
# define UL_ASAN_BLACKLIST	/* nothing */
#endif



/*
 * Note that sysconf(_SC_GETPW_R_SIZE_MAX) returns *initial* suggested size for
 * pwd buffer and in some cases it is not large enough. See POSIX and
 * getpwnam_r man page for more details.
 */
#define UL_GETPW_BUFSIZ	(16 * 1024)

/*
 * Darwin or other BSDs may only have MAP_ANON. To get it on Darwin we must
 * define _DARWIN_C_SOURCE before including sys/mman.h. We do this in config.h.
 */
#if !defined MAP_ANONYMOUS && defined MAP_ANON
# define MAP_ANONYMOUS  (MAP_ANON)
#endif



static inline int write_all(int fd, const void *buf, size_t count)
{
    while (count) {
        ssize_t tmp;

        errno = 0;
        tmp = write(fd, buf, count);
        if (tmp > 0) {
            count -= tmp;
            if (count)
                buf = (void *) ((char *) buf + tmp);
        } else if (errno != EINTR && errno != EAGAIN)
            return -1;
        if (errno == EAGAIN)    /* Try later, *sigh* */
            xusleep(250000);
    }
    return 0;
}

static inline ssize_t read_all(int fd, char *buf, size_t count)
{
    ssize_t ret;
    ssize_t c = 0;
    int tries = 0;

    memset(buf, 0, count);
    while (count > 0) {
        ret = read(fd, buf, count);
        if (ret <= 0) {
            if (ret < 0 && (errno == EAGAIN || errno == EINTR) && (tries++ < 5)) {
                xusleep(250000);
                continue;
            }
            return c ? c : -1;
        }
        if (ret > 0)
            tries = 0;
        count -= ret;
        buf += ret;
        c += ret;
    }
    return c;
}

static inline int dup_fd_cloexec(int oldfd, int lowfd)
{
    int fd, flags, errno_save;

#ifdef F_DUPFD_CLOEXEC
    fd = fcntl(oldfd, F_DUPFD_CLOEXEC, lowfd);
    if (fd >= 0)
        return fd;
#endif

    fd = dup(oldfd);
    if (fd < 0)
        return fd;

    flags = fcntl(fd, F_GETFD);
    if (flags < 0)
        goto unwind;
    if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0)
        goto unwind;

    return fd;

unwind:
    errno_save = errno;
    close(fd);
    errno = errno_save;

    return -1;
}

#endif /* UTIL_LINUX_C_H */
