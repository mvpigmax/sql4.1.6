#include "utf16.h"
#include <stdint.h>

static const int halfShift = 10; /* used for shifting by 10 bits */

static const UTF32 halfBase = 0x0010000UL;
static const UTF32 halfMask = 0x3FFUL;

#define UNI_REPLACEMENT_CHAR (UTF32)0x0000FFFD
#define UNI_MAX_BMP (UTF32)0x0000FFFF
#define UNI_MAX_UTF16 (UTF32)0x0010FFFF
#define UNI_MAX_UTF32 (UTF32)0x7FFFFFFF
#define UNI_MAX_LEGAL_UTF32 (UTF32)0x0010FFFF

#define UNI_SUR_HIGH_START  (UTF32)0xD800
#define UNI_SUR_HIGH_END    (UTF32)0xDBFF
#define UNI_SUR_LOW_START   (UTF32)0xDC00
#define UNI_SUR_LOW_END     (UTF32)0xDFFF

size_t utf16_strlen(const UTF16* in)
{
	size_t len;
	UTF16 *p = (UTF16*) in;
	for( len = 0; *p && len <= SIZE_MAX; ++p, ++len )
		;
	return len;
}

size_t utf32_to_utf16(const UTF32** in, size_t insize, UTF16* out, size_t outsize, int flags)
{
	UTF32 *w, *wlim, ch;
	UTF16 *p, *lim;
	size_t total;

	if( in == NULL || insize == 0 || (outsize == 0 && out != NULL) )
		return (0);

	w = (UTF32*)*in;
	wlim = w + insize;
	p = out;
	lim = p + outsize;
	total = 0;

	while( w < wlim )
	{
		if( p && p >= lim )
			break;

		ch = *w++;
		if( ch <= UNI_MAX_BMP )
		{
			if( ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END )
			{
				if( (flags & UTF16_IGNORE_ERROR) == 0 )
					break;
				else if( p )
					*p++ = UNI_REPLACEMENT_CHAR;
			}
			else if( p )
				*p++ = (UTF16) ch; /* normal case */
			++total;
		}
		else if( ch > UNI_MAX_LEGAL_UTF32 )
		{
			if( (flags & UTF16_IGNORE_ERROR) == 0 )
				break;
			else if( p )
				*p++ = UNI_REPLACEMENT_CHAR;
			++total;
		}
		else
		{
			/* target is a character in range 0xFFFF - 0x10FFFF. */
			if( p && (p + 1) >= lim )
				break;
			ch -= halfBase;
			if( p )
			{
				*p++ = (UTF16) ((ch >> halfShift) + UNI_SUR_HIGH_START);
				*p++ = (UTF16) ((ch & halfMask) + UNI_SUR_LOW_START);
			}
			total += 2;
		}
	}

	*in = w;
	return total;
}

size_t utf16_to_utf32(const UTF16** in, size_t insize, UTF32* out, size_t outsize, int flags)
{
	UTF32 *w, *wlim, ch, ch2;
	UTF16 *p, *lim;
	size_t total;

	if( in == NULL || insize == 0 || (outsize == 0 && out != NULL) )
		return (0);

	w = out;
	wlim = w + outsize;
	p = (UTF16*)*in;
	lim = p + insize;
	total = 0;

	while( p < lim )
	{
		//const UTF16* oldSource = source; /*  In case we have to back up because of target overflow. */
		ch = *p++;
		/* If we have a surrogate pair, convert to UTF32 first. */
		if( ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END )
		{
			/* If the 16 bits following the high surrogate are in the source buffer... */
			if( p < lim )
			{
				ch2 = *p;
				/* If it's a low surrogate, convert to UTF32. */
				if( ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END )
				{
					ch = ((ch - UNI_SUR_HIGH_START) << halfShift) + (ch2
							- UNI_SUR_LOW_START) + halfBase;
					++p;
				}
				else if( (flags & UTF16_IGNORE_ERROR) == 0 )
					/* it's an unpaired high surrogate */
					break;
			}
			else
				/* We don't have the 16 bits following the high surrogate. */
				break;
		}
		else if( (flags & UTF16_IGNORE_ERROR) == 0 )
		{
			/* UTF-16 surrogate values are illegal in UTF-32 */
			if( ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END )
				break;
		}
		if( w && w >= wlim )
			break;
		if( w )
			*w++ = ch;
		++total;
	}

	*in = p;
	return total;
}
