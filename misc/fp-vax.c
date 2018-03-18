typedef unsigned char	uint8_t;

union fpd {
	double	d;
	uint8_t	x[8];
};

union fpd	data[] = {
	{.x[0] = 'A', .x[1] = 'B', .x[2] = 'C', .x[3] = 'D',
	 .x[4] = 'E', .x[5] = 'F', .x[6] = 'G', .x[7] = 'H'},
	{.d = 0.1},
	{.d = 1.0},
	{.d = 2.0},
	{.d = 10.},
	{.d = 20.},
	{.d = 3.1415},
	{.d = 31.415}
};


