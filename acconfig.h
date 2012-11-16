/*
 * Define this if your C library doesn't have usleep.
 *
 * $Id: acconfig.h 3654 2004-06-29 22:12:44Z ucaccsp $
 */
#undef NEED_USLEEP
#undef NEED_SNPRINTF

/*
 * Missing declarations
 */
#undef GETTOD_NOT_DECLARED
#undef KILL_NOT_DECLARED

/*
 * If you don't have these types in <inttypes.h>, #define these to be
 * the types you do have.
 */
#undef int16_t
#undef int32_t
#undef int64_t
#undef int8_t
#undef uint16_t
#undef uint32_t
#undef uint8_t

/*
 * Debugging:
 * DEBUG: general debugging
 * DEBUG_MEM: debug memory allocation
 */
#undef DEBUG
#undef DEBUG_MEM

/*
 * Optimization
 */
#undef NDEBUG

/* Audio device relate */
#undef HAVE_MACOSX_AUDIO
#undef HAVE_SPARC_AUDIO
#undef HAVE_SGI_AUDIO
#undef HAVE_PCA_AUDIO
#undef HAVE_LUIGI_AUDIO
#undef HAVE_NEWPCM_AUDIO
#undef HAVE_OSS_AUDIO
#undef HAVE_HP_AUDIO
#undef HAVE_NETBSD_AUDIO
#undef HAVE_OSPREY_AUDIO
#undef HAVE_MACHINE_PCAUDIOIO_H
#undef HAVE_ALSA_AUDIO
#undef HAVE_IXJ_AUDIO

#undef HAVE_G728

#undef HAVE_IPv6

/* GSM related */
#undef SASR
#undef FAST
#undef USE_FLOAT_MUL

@BOTTOM@

#ifndef WORDS_BIGENDIAN
#define WORDS_SMALLENDIAN
#endif
