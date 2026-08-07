typedef unsigned long long uint64_t;

static union {
	double d;
	uint64_t u;
} value = {3.01029995663611771306e-01};

int main(void)
{
	return value.u != 0x3fd34413509f6000ULL;
}
