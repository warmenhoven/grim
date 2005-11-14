/***************************************************************************

 sha1.h -- This file comes from RFC 3174

 ***************************************************************************/

#ifndef _SHA1_H_
#define _SHA1_H_

#if 0 /* automatic detection always fails on some systems */
#ifdef HAVE_STDINT_H

#include <stdint.h>

#else /* HAVE_STDINT_H */
#ifdef HAVE_SYS_INTTYPES_H

#include <sys/inttypes.h>

#else /* HAVE_SYS_INTTYPES_H */

typedef unsigned long uint32_t;
typedef unsigned char uint8_t;

#endif /* HAVE_SYS_INTTYPES_H */
#endif /* HAVE_STDINT_H */
#endif /* 0 */

/*
 * If you do not have the ISO standard stdint.h header file, then you
 * must typdef the following:
 *    name              meaning
 *  m_uint32_t         unsigned 32 bit integer
 *  m_uint8_t          unsigned 8 bit integer (i.e., unsigned char)
 *  int              integer of >= 16 bits
 *
 */

/* we define types manually, anyway packet.h does that already */
typedef unsigned long m_uint32_t;
typedef unsigned char m_uint8_t;

#ifndef _SHA_enum_
#define _SHA_enum_
enum
{
    shaSuccess = 0,
    shaNull,            /* Null pointer parameter */
    shaInputTooLong,    /* input data too long */
    shaStateError       /* called Input after Result */
};
#endif
#define SHA1HashSize 20

/*
 *  This structure will hold context information for the SHA-1
 *  hashing operation
 */
typedef struct SHA1Context
{
    m_uint32_t Intermediate_Hash[SHA1HashSize/4]; /* Message Digest  */

    m_uint32_t Length_Low;            /* Message length in bits      */
    m_uint32_t Length_High;           /* Message length in bits      */

                               /* Index into message block array   */
    int Message_Block_Index;
    m_uint8_t Message_Block[64];      /* 512-bit message blocks      */

    int Computed;               /* Is the digest computed?         */
    int Corrupted;             /* Is the message digest corrupted? */
} SHA1Context;

/*
 *  Function Prototypes
 */

#ifdef __cplusplus
extern "C" {
#endif

int SHA1Reset(  SHA1Context     *);
int SHA1Input(  SHA1Context     *,
                const m_uint8_t *,
                unsigned int);
int SHA1Result( SHA1Context *,
                m_uint8_t   Message_Digest[SHA1HashSize]);

#ifdef __cplusplus
};
#endif

#endif
