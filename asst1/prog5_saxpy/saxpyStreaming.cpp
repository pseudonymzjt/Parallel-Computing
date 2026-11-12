#include <smmintrin.h> // For _mm_stream_load_si128
#include <emmintrin.h> // For _mm_mul_ps
#include <assert.h>
#include <stdint.h>

extern void saxpySerial(int N,
			float scale,
			float X[],
			float Y[],
			float result[]);


void saxpyStreaming(int N, float scale, float X[], float Y[], float result[]) {
    const __m128 s = _mm_set1_ps(scale);
    int i = 0;
    for (; i + 16 <= N; i += 16) {
        _mm_stream_ps(result+i+ 0, _mm_add_ps(_mm_mul_ps(s, _mm_load_ps(X+i+ 0)), _mm_load_ps(Y+i+ 0)));
        _mm_stream_ps(result+i+ 4, _mm_add_ps(_mm_mul_ps(s, _mm_load_ps(X+i+ 4)), _mm_load_ps(Y+i+ 4)));
        _mm_stream_ps(result+i+ 8, _mm_add_ps(_mm_mul_ps(s, _mm_load_ps(X+i+ 8)), _mm_load_ps(Y+i+ 8)));
        _mm_stream_ps(result+i+12, _mm_add_ps(_mm_mul_ps(s, _mm_load_ps(X+i+12)), _mm_load_ps(Y+i+12)));
    }
    for (; i < N; ++i) result[i] = scale*X[i] + Y[i];
    _mm_sfence();
}
