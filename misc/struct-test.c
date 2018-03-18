/* Copyright 2018  Peter Lund <firefly@vax64.dk>\

   Licensed under GPL v2.

 */

/* experiment with struct returns */

#include <stdint.h>
#include <stdio.h>


struct vars {
	unsigned	valid;	/* var_xx bits = which other fields are valid */

	unsigned	cc;

	uint32_t	imm;
	unsigned	Rn, Rx, opreg, dst;
	unsigned	datalen;
};


static struct vars test(int x)
{
	switch (x) {
	case 0:
		return (struct vars) {.Rn=1234, .Rx=5678};
	case 1:
		return (struct vars) {.Rn=2345, .Rx=9876, .dst=999};
	default:
		return (struct vars) {.dst=999};
	}
}


int main(int argc, char *argv[])
{
	(void) argv;
	struct vars v = test(argc);

	printf("%d %d %d\n", v.Rn, v.Rx, v.dst);
	return 0;
}
