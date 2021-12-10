#include <stdio.h>


#define _T(x) #x
#define X(x) x
#define T(x) _T(x)
 // #define T(x) #x


#define C(x) D##x

#define FP_PREC 10000ULL

#define D0 1ULL
#define D1 10ULL
#define D2 100ULL
#define D3 1000ULL
#define D4 10000ULL
#define D5 100000ULL
#define D6 1000000ULL
#define D7 10000000ULL
#define D8 100000000ULL
#define D9 1000000000ULL


int main(int argc, char const *argv[])
{
	printf("DEF: %llu\n", (C(8) / C(6)));
	return 0;
}