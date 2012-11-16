/* ratconf.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.in by autoheader.  */
/*
 * Define this if your C library doesn't have usleep.
 *
 * $Id: config.h.in 3672 2004-11-26 11:38:15Z ucaccsp $
 */
/* #undef NEED_USLEEP */
/* #undef NEED_SNPRINTF */

/*
 * Missing declarations
 */
/* #undef GETTOD_NOT_DECLARED */
/* #undef KILL_NOT_DECLARED */

/*
 * If you don't have these types in <inttypes.h>, #define these to be
 * the types you do have.
 */
/* #undef int16_t */
/* #undef int32_t */
/* #undef int64_t */
/* #undef int8_t */
/* #undef uint16_t */
/* #undef uint32_t */
/* #undef uint8_t */

/*
 * Debugging:
 * DEBUG: general debugging
 * DEBUG_MEM: debug memory allocation
 */
#define DEBUG 1
/* #undef DEBUG_MEM */

/*
 * Optimization
 */
/* #undef NDEBUG */

/* Audio device relate */
/* #undef HAVE_MACOSX_AUDIO */
/* #undef HAVE_SPARC_AUDIO */
/* #undef HAVE_SGI_AUDIO */
/* #undef HAVE_PCA_AUDIO */
/* #undef HAVE_LUIGI_AUDIO */
/* #undef HAVE_NEWPCM_AUDIO */
#define HAVE_OSS_AUDIO 1
/* #undef HAVE_HP_AUDIO */
/* #undef HAVE_NETBSD_AUDIO */
/* #undef HAVE_OSPREY_AUDIO */
/* #undef HAVE_MACHINE_PCAUDIOIO_H */
/* #undef HAVE_ALSA_AUDIO */
#define HAVE_IXJ_AUDIO 1

/* #undef HAVE_G728 */

/* #undef HAVE_IPv6 */

/* GSM related */
#define SASR 1
#define FAST 1
#define USE_FLOAT_MUL 1


/* Define to 1 if you have the <bstring.h> header file. */
/* #undef HAVE_BSTRING_H */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `asound' library (-lasound). */
/* #undef HAVE_LIBASOUND */

/* Define to 1 if you have the <machine/pcaudioio.h> header file. */
/* #undef HAVE_MACHINE_PCAUDIOIO_H */

/* Define to 1 if you have the <malloc.h> header file. */
#define HAVE_MALLOC_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <soundcard.h> header file. */
/* #undef HAVE_SOUNDCARD_H */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <stropts.h> header file. */
#define HAVE_STROPTS_H 1

/* Define to 1 if you have the <sys/filio.h> header file. */
/* #undef HAVE_SYS_FILIO_H */

/* Define to 1 if you have the <sys/sockio.h> header file. */
/* #undef HAVE_SYS_SOCKIO_H */

/* Define to 1 if you have the <sys/soundcard.h> header file. */
#define HAVE_SYS_SOUNDCARD_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
#define HAVE_SYS_WAIT_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
/* #undef WORDS_BIGENDIAN */

/* Define to 1 if the X Window System is missing or not being used. */
/* #undef X_DISPLAY_MISSING */

/* Define to 1 if type `char' is unsigned and you are not using gcc.  */
#ifndef __CHAR_UNSIGNED__
/* # undef __CHAR_UNSIGNED__ */
#endif

/* <stddef.h> defines the offsetof macro */
/* #undef HAVE_OFFSETOF */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `short' if <sys/types.h> does not define. */
/* #undef int16_t */

/* Define to `long' if <sys/types.h> does not define. */
/* #undef int32_t */

/* Define to `long long' if <sys/types.h> does not define. */
/* #undef int64_t */

/* Define to `signed char' if <sys/types.h> does not define. */
/* #undef int8_t */

/* Define to `unsigned' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to `unsigned short' if <sys/types.h> does not define. */
/* #undef uint16_t */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef uint32_t */

/* Define to `unsigned char' if <sys/types.h> does not define. */
/* #undef uint8_t */

#ifndef WORDS_BIGENDIAN
#define WORDS_SMALLENDIAN
#endif
