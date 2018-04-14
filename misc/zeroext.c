#include <assert.h>
#include <stdint.h>

#include "macros.h"

#define U_LEN_8        0
#define U_LEN_16       1
#define U_LEN_32       2
#define U_LEN_64       3
#define U_LEN_DATALEN  4   /* <datalen> */

int32_t zeroext(int32_t x, int len)
{
	switch (len) {
	case U_LEN_8:
		/* FIXME use double cast trick here, too?

		    return (int32_t)(uint8_t) x;
		    return (int32_t)(uint16_t) x;
		 */
		return x &   0xFF;
		break;
	case U_LEN_16:
		return x & 0xFFFF;
		break;
	case U_LEN_32:
		return x;
		break;
	default:
		/* U_LEN_64, U_LEN_DATALEN + illegal len values */
		UNREACHABLE();
	}
}


int32_t zeroext2(int32_t x, int len)
{
	switch (len) {
	case U_LEN_8:
		/* FIXME use double cast trick here, too?

		    return (int32_t)(uint8_t) x;
		    return (int32_t)(uint16_t) x;
		 */
		return (int32_t)(uint8_t) x;
		break;
	case U_LEN_16:
		return (int32_t)(uint16_t) x;
		break;
	case U_LEN_32:
		return x;
		break;
	default:
		/* U_LEN_64, U_LEN_DATALEN + illegal len values */
		UNREACHABLE();
	}
}


