////////////////////////////////////////////////////////////////////////////////
// Steps to add a new PRNG
//
// 1. Add a Perl_{name}_seed() and a Perl_{name}_random_double() function below
// 2. Add lines to embed.fnc with prototype information for these functions:
//      TXop    |double |{name}_random_double
//      TXop    |void   |{name}_seed|U64 seed1
// 3. Update Configure to use the newly added PRNG:
//      randfunc=Perl_{name}_random_double
//      drand01="Perl_{name}_random_double()"
//      seedfunc="Perl_{name}_seed"
//      randseedtype=U64
// 4. Compile: /bin/bash ./Configure -DDEBUGGING -des && make -j8
////////////////////////////////////////////////////////////////////////////////

#include <math.h>
#include <stdint.h>

typedef struct { uint64_t state;  uint64_t inc; } pcg32_random_t;
// Global PRNG object
pcg32_random_t prng;

// Multiply-Shift Hash: 64bits in => 64bits out
U64
hash_msh(U64 x)
{
	U64 prime = 0x9e3779b97f4a7c15; // A large prime constant
	x ^= (x >> 30);
	x *= prime;
	x ^= (x >> 27);
	x *= prime;
	x ^= (x >> 31);

	return x;
}

// https://prng.di.unimi.it/#remarks
double
uint64_to_double(U64 num)
{
	// A standard 64bit double floating-point number in IEEE floating point
	// format has 52 bits of significand. Thus, the representation can actually
	// store numbers with 53 significant binary digits.
	double ret   = ldexp(num >> 11, -53);

	/*DEBUG_U(PerlIO_printf(Perl_error_log, "PRNG U2D: %lu => %0.15f\n", num, ret));*/

	return ret;
}

//////////////////////////////////////////////////////////////
// PCG32 functions
//////////////////////////////////////////////////////////////

// Perl can only send one seed, so we have to deterministically
// create the other seeds needed for our PRNG
void
Perl_pcg32_seed(U64 seed)
{
	U64 seed1 = hash_msh(seed);
	U64 seed2 = hash_msh(seed1);

	prng.state = seed1;
	prng.inc   = seed2;

	DEBUG_U(PerlIO_printf(Perl_error_log, "PCG32 INIT: %lu => %lu / %lu\n", seed, prng.state, prng.inc));
}

U32
pcg32_rand32()
{
	uint64_t oldstate = prng.state;
	// Advance internal state
	prng.state = oldstate * 6364136223846793005ULL + (prng.inc | 1);
	// Calculate output function (XSH RR), uses old state for max ILP
	uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
	uint32_t rot = oldstate >> 59u;
	return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

U64
pcg32_rand64()
{
	U32 high = pcg32_rand32();
	U32 low  = pcg32_rand32();
	U64 ret  = ((U64)high << 32) | low;

	/*DEBUG_U(PerlIO_printf(Perl_error_log, "PCG64: %lu\n", ret));*/

	return ret;
}

double
Perl_pcg32_random_double()
{
	U64 num    = pcg32_rand64();
	double ret = uint64_to_double(num);

	/*DEBUG_U(PerlIO_printf(Perl_error_log, "PCG Double: %0.15f\n", ret));*/

	return ret;
}
