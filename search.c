#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>

#define VERSION "0.1"

const char * search_term;
      size_t search_term_len;
const char * filename;

int          case_insensitive;

void handle_error(const char * msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void usage()
{
    fprintf(stderr, "usage: [-i] TERM FILE");
    exit(1);
}

void version()
{
    fprintf(stderr, "version: %s\n", VERSION);
    exit(1);
}

void parse_opts(int argc, char ** argv)
{
    int c;

    while ((c = getopt(argc, argv, "hvi")) != -1)
    {
        switch (c)
        {
            case 'h':
                usage();
                break;
            case 'v':
                version();
                break;
            case 'i':
                case_insensitive = 1;
                break;
            default:
                usage(argv);
        }
    }
}

/* modified from https://sourceware.org/viewvc/src/newlib/libc/string/memchr.c?revision=1.4&view=markup */
void * memchr2(const void * s, char c1, char c2, size_t length)
{
    /* Nonzero if either X or Y is not aligned on a "long" boundary.  */
    #define UNALIGNED(X) ((long)X & (sizeof (long) - 1))
    /* How many bytes are loaded each iteration of the word copy loop.  */
    #define LBLOCKSIZE (sizeof (long))
    /* Threshhold for punting to the bytewise iterator.  */
    #define TOO_SMALL(LEN)  ((LEN) < LBLOCKSIZE)

    #if LONG_MAX == 2147483647L
    #define DETECTNULL(X) (((X) - 0x01010101) & ~(X) & 0x80808080)
    #else
    #if LONG_MAX == 9223372036854775807L
    /* Nonzero if X (a long int) contains a NULL byte. */
    #define DETECTNULL(X) (((X) - 0x0101010101010101) & ~(X) & 0x8080808080808080)
    #else
    #error long int is not a 32bit or 64bit type.
    #endif
    #endif

    #ifndef DETECTNULL
    #error long int is not a 32bit or 64bit byte
    #endif

    /* DETECTCHAR returns nonzero if (long)X contains the byte used
       to fill (long)MASK. */
    #define DETECTCHAR(X,MASK) (DETECTNULL(X ^ MASK))

    const unsigned char *src = s;
    unsigned char d1 = (unsigned char) c1;
    unsigned char d2 = (unsigned char) c2;


    unsigned long *asrc;
    unsigned long  mask1;
    unsigned long  mask2;
    unsigned int i;
  
    while (UNALIGNED(src))
    {
        if (! length--)
        {
            return NULL;
        }

        if (*src == d1 || *src == d2)
        {
            return (void *) src;
        }

        ++src;
    }


    if (!TOO_SMALL(length))
    {
        /* If we get this far, we know that length is large and src is
        word-aligned. */
        /* The fast code reads the source one word at a time and only
        performs the bytewise search on word-sized segments if they
        contain the search character, which is detected by XORing
        the word-sized segment with a word-sized block of the search
        character and then detecting for the presence of NUL in the
        result.  */
        asrc = (unsigned long *) src;

        mask1 = d1 << 8 | d1;
        mask1 = mask1 << 16 | mask1;

        mask2 = d2 << 8 | d2;
        mask2 = mask2 << 16 | mask2;

        for (i = 32; i < LBLOCKSIZE * 8; i <<= 1)
        {
            mask1 = (mask1 << i) | mask1;
            mask2 = (mask2 << i) | mask2;
        }

        while (length >= LBLOCKSIZE)
        {
            if (DETECTCHAR(*asrc, mask1) || DETECTCHAR(*asrc, mask2))
            {
                break;
            }

            length -= LBLOCKSIZE;
            ++asrc;
        }

        /* If there are fewer than LBLOCKSIZE characters left,
        then we resort to the bytewise loop.  */

        src = (unsigned char *) asrc;
    }

    while (length--)
    {
        if (*src == d1 || *src == d2)
        {
            return (void *) src;
        }
            
        ++src;
    }

    return NULL;
}

void search_case_sensitive(const char * fbegin, const char * fend)
{
    size_t search_level = 0;
    const char * fptr   = fbegin;

    while (fptr)
    {
        if (search_level == 0)
        {
            if ((fptr = memchr(fptr, search_term[0], (size_t) (fend - fptr))))
            {
                ++fptr;
                ++search_level;
            }
        }
        else if (*fptr == search_term[search_level])
        {
            ++fptr;
            ++search_level;

            if (search_level == search_term_len)
            {
                printf("%lu\n", fptr - fbegin - (ssize_t) search_term_len);
            }
        }
        else
        {
            search_level = 0;
        }
    }
}

void search_case_insensitive(const char * fbegin, const char * fend)
{
    size_t search_level = 0;
    const char * fptr   = fbegin;
          char * search_term_inv = malloc(search_term_len);

    size_t i;
    for (i = 0; i<search_term_len; ++i)
    {
        search_term_inv[i] = isalpha(search_term[i])
            ? search_term[i] ^ 32
            : search_term[i];
    }

    while (fptr)
    {
        if (search_level == 0)
        {
            if ((fptr = memchr2(fptr, search_term[0], search_term_inv[0], (size_t) (fend - fptr))))
            {
                ++fptr;
                ++search_level;
            }
        }
        else if (*fptr == search_term[search_level] || *fptr == search_term_inv[search_level])
        {
            ++fptr;
            ++search_level;

            if (search_level == search_term_len)
            {
                printf("%lu\n", fptr - fbegin - (ssize_t) search_term_len);
            }
        }
        else
        {
            search_level = 0;
        }
    }
}

int main(int argc, char ** argv)
{
    int fd;
    struct stat sb;

    const char * fbegin;
    const char * fend;

    parse_opts(argc, argv);

    if (argc - optind < 1)
    {
        handle_error("need search term");
    }

    if (argc - optind < 2)
    {
        handle_error("need filename");
    }

    search_term = argv[optind];
    if ((search_term_len = strlen(search_term)) == 0)
    {
        handle_error("need search term");
    }

    filename = argv[optind + 1];

    if ((fd = open(filename, O_RDONLY)) == -1)
    {
        handle_error("open failed");
    }

    if (fstat(fd, &sb) == -1)
    {
        handle_error("fstat failed");
    }

    if ((fbegin = mmap(NULL, (size_t) sb.st_size, PROT_READ, MAP_PRIVATE|MAP_POPULATE, fd, 0)) == MAP_FAILED)
    {
        handle_error("mmap failed");
    }

    fend = fbegin + (size_t) sb.st_size;

    if (case_insensitive) 
    {
        search_case_insensitive(fbegin, fend);
    }
    else
    {
        search_case_sensitive(fbegin, fend);
    }


    return 0;
}

