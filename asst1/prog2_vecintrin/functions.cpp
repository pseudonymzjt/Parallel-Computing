#include <stdio.h>
#include <algorithm>
#include <math.h>
#include "CMU418intrin.h"
#include "logger.h"
using namespace std;


void absSerial(float* values, float* output, int N) {
    for (int i=0; i<N; i++) {
	float x = values[i];
	if (x < 0) {
	    output[i] = -x;
	} else {
	    output[i] = x;
	}
    }
}

// implementation of absolute value using 15418 instrinsics
void absVector(float* values, float* output, int N) {
    __cmu418_vec_float x;
    __cmu418_vec_float result;
    __cmu418_vec_float zero = _cmu418_vset_float(0.f);
    __cmu418_mask maskTail, maskIsNegative, maskIsNotNegative;

    //  Note: Take a careful look at this loop indexing.  This example
    //  code is not guaranteed to work when (N % VECTOR_WIDTH) != 0.
    //  Why is that the case?
    for (int i=0; i<N; i+=VECTOR_WIDTH) {

	int tail = N - i < VECTOR_WIDTH ? N - i : VECTOR_WIDTH;

	// All ones
	// __cmu418_mask maskAll = _cmu418_init_ones();
	maskTail = _cmu418_init_ones(tail);

	// All zeros
	maskIsNegative = _cmu418_init_ones(0);

	// Load vector of values from contiguous memory addresses
	_cmu418_vload_float(x, values+i, maskTail);               // x = values[i];

	// Set mask according to predicate
	_cmu418_vlt_float(maskIsNegative, x, zero, maskTail);     // if (x < 0) {

	// Execute instruction using mask ("if" clause)
	_cmu418_vsub_float(result, zero, x, maskIsNegative);      //   output[i] = -x;

	// Inverse maskIsNegative to generate "else" mask
	maskIsNotNegative = _cmu418_mask_not(maskIsNegative);     // } else {

	// Execute instruction ("else" clause)
	_cmu418_vload_float(result, values+i, maskIsNotNegative); //   output[i] = x; }

	// Write results back to memory
	_cmu418_vstore_float(output+i, result, maskTail);
    }
}

// Accepts an array of values and an array of exponents
// For each element, compute values[i]^exponents[i] and clamp value to
// 4.18.  Store result in outputs.
// Uses iterative squaring, so that total iterations is proportional
// to the log_2 of the exponent
void clampedExpSerial(float* values, int* exponents, float* output, int N) {
    for (int i=0; i<N; i++) {
	float x = values[i];
	float result = 1.f;
	int y = exponents[i];
	float xpower = x;
	while (y > 0) {
	    if (y & 0x1) {
			result *= xpower;
		}
	    xpower = xpower * xpower;
	    y >>= 1;
	}
	if (result > 4.18f) {
	    result = 4.18f;
	}
	output[i] = result;
    }
}

void clampedExpVector(float* values, int* exponents, float* output, int N) {
	__cmu418_vec_float x, xpower;
	__cmu418_vec_int y, ypower;
	__cmu418_vec_float result;
	__cmu418_vec_int izero = _cmu418_vset_int(0);
	__cmu418_vec_int ione = _cmu418_vset_int(1);
	__cmu418_mask maskTail, maskPower, maskAlive;
    // Implement your vectorized version of clampedExpSerial here
    for (int i = 0; i < N; i += VECTOR_WIDTH) {
		int tail = N - i < VECTOR_WIDTH ? N - i : VECTOR_WIDTH; 

		maskTail = _cmu418_init_ones(tail);
		_cmu418_vset_float(result, 1.f, maskTail); // result = 1.f

		_cmu418_vload_float(x, values + i, maskTail);  // x = values[i];
		_cmu418_vload_int(y, exponents + i, maskTail);   // y = exponents[i];
		_cmu418_vmove_float(xpower, x, maskTail); // xpower = x;
		while(true) {
			_cmu418_vset_int(ypower, 1, maskTail);
			_cmu418_vbitand_int(ypower, ypower, y, maskTail);
			_cmu418_vgt_int(maskPower, ypower, izero, maskTail); 
			// notmaskPower = _cmu418_mask_not(maskPower);
			// fix: correct exit logic
			_cmu418_vgt_int(maskAlive, y, izero, maskTail);
			if (_cmu418_cntbits(maskAlive) == 0) break; // if(y == 0) break;
			// if(_cmu418_cntbits(maskPower) == 0) break; // if(y == 0) break;

			// if (y & 0x1) {
			// 	result *= xpower;
			// }
			// __cmu418_vec_float ypowerfloat;
			// _cmu418_vmove_float(ypowerfloat, xpower, maskPower);
			// _cmu418_vset_float(ypowerfloat, 1.f, notmaskPower);
			// _cmu418_vmult_float(result, result, ypowerfloat, maskTail);
			// optimize 1: avoid redundant move
			_cmu418_vmult_float(result, result, xpower, maskPower);

			_cmu418_vmult_float(xpower, xpower, xpower, maskTail); // xpower = xpower * xpower;
			_cmu418_vshiftright_int(y, y, ione, maskTail); // y >>= 1;
		}

		//	if (result > 4.18f) {
		//     result = 4.18f;
		// }
		__cmu418_mask maskUpper;
		__cmu418_vec_float upperLimit = _cmu418_vset_float(4.18f);
		_cmu418_vgt_float(maskUpper, result, upperLimit, maskTail);
		_cmu418_vmove_float(result, upperLimit, maskUpper);

		_cmu418_vstore_float(output + i, result, maskTail); // output[i] = result;
	}
}


float arraySumSerial(float* values, int N) {
    float sum = 0;
    for (int i=0; i<N; i++) {
	sum += values[i];
    }

    return sum;
}

// Assume N % VECTOR_WIDTH == 0
// Assume VECTOR_WIDTH is a power of 2
// float arraySumVector(float* values, int N) {
// 	__cmu418_vec_float x;
// 	__cmu418_mask maskAll;
// 	float partial[VECTOR_WIDTH];
// 	float result = 0.f;
//     // Implement your vectorized version here
//     for(int i = 0; i < N; i += VECTOR_WIDTH) {
// 		maskAll = _cmu418_init_ones();
// 		_cmu418_vload_float(x, values + i, maskAll); 
// 		for(int length = VECTOR_WIDTH; length > 1; length /= 2) {
// 			_cmu418_hadd_float(x, x);
// 			_cmu418_interleave_float(x, x);
// 		}
// 		_cmu418_vstore_float(partial, x, maskAll);
// 		result += partial[0];
// 	}
// 	return result;
// }
// optimize 2: better reduction logic
float arraySumVector(float* values, int N) {
	__cmu418_vec_float x, acc;
	__cmu418_mask maskAll = _cmu418_init_ones();
	float partial[VECTOR_WIDTH];
	acc = _cmu418_vset_float(0.f);

	for (int i = 0; i < N; i += VECTOR_WIDTH) {
		_cmu418_vload_float(x, values + i, maskAll);
		_cmu418_vadd_float(acc, acc, x, maskAll);
	}

	_cmu418_hadd_float(acc, acc);
	_cmu418_interleave_float(acc, acc);
	_cmu418_hadd_float(acc, acc);    
	_cmu418_interleave_float(acc, acc);
	_cmu418_hadd_float(acc, acc);

	_cmu418_vstore_float(partial, acc, maskAll);
	return partial[0];
}