#include <algorithm>

// Generate random data
void initRandom(float *values, int N) {
    for (int i=0; i<N; i++)
    {
        // random input values
        values[i] = .001f + 2.998f * static_cast<float>(rand()) / RAND_MAX;
    }
}

// $ ./sqrt --data g
// [sqrt serial]:          [2565.816] ms
// [sqrt ispc]:            [631.641] ms
// [sqrt task ispc]:       [158.728] ms
//                                 (4.06x speedup from ISPC)
//                                 (16.16x speedup from task ISPC)

// Generate data that gives high relative speedup
void initGood(float *values, int N) {
    for (int i=0; i<N; i++)
    {
        // Todo: Choose values
        values[i] = 2.999999f;
    }
}

// $ ./sqrt --data b
// [sqrt serial]:          [478.191] ms
// [sqrt ispc]:            [629.972] ms
// [sqrt task ispc]:       [129.565] ms
//                                 (0.76x speedup from ISPC)
//                                 (3.69x speedup from task ISPC)

// Generate data that gives low relative speedup
void initBad(float *values, int N) {
    for (int i=0; i<N; i++)
    {
        // Todo: Choose values
        values[i] = (i % 8 == 0) ? 2.999999f : 1.000f;
    }
}

