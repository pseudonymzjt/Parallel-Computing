# Part 1: CUDA Warm-Up — SAXPY

## Experimental setup

All measurements in this section were taken on a single machine rather than on AWS, since no T4 instance was available:

| | |
|---|---|
| GPU | NVIDIA GeForce RTX 5060 Laptop GPU, 26 SMs, 8 GB |
| Compute capability | 12.0 |
| Peak device memory bandwidth (reported by the runtime) | 384 GB/s (12 GHz × 128-bit) |
| Host↔device link | PCIe 5.0 × 8 (theoretical ≈ 31.5 GB/s) |
| CPU | Intel Core Ultra 9 285H, 16 hardware threads |

N = 100,000,000 floats (the default in `main.cpp`), i.e. 381 MB per array and 12N = 1.12 GB of logical traffic per SAXPY. The kernel is launched with 512 threads per block and ⌈N/512⌉ = 195,313 blocks. All timings below are from a steady-state run; the first call is discarded because it includes one-time CUDA context creation, and run-to-run variation for the remaining calls was within ±7%.

---

## Question 1

> *What performance do you observe compared to the sequential CPU-based implementation of SAXPY?*

| Implementation | Time | Effective bandwidth (12N bytes) |
|---|---|---|
| Sequential CPU `saxpySerial` (best of 5) | **52.7 ms** | **21.2 GB/s** |
| CUDA, **kernel only** (with `cudaDeviceSynchronize()`) | **3.52 ms** | **317 GB/s** |
| CUDA, **whole process** (H2D + kernel + D2H) | **162.8 ms** | **6.86 GB/s** |

**The kernel alone is 15.0× faster than the sequential CPU version, but the full CUDA path is 3.1× slower.**

This result is not a bug, and it is the most important observation in this part of the assignment. SAXPY performs one multiply-add per element while touching 12 bytes (read `x`, read `y`, write `result`), i.e. an arithmetic intensity of only **1 FLOP per 12 bytes ≈ 0.083 FLOP/byte**. That is far below the FLOP-per-byte ratio of any modern processor, so SAXPY is *unconditionally memory-bound*: the only thing that matters is how fast the three arrays can be streamed.

On the CPU the three arrays already sit in DRAM, and a single core can pull roughly 21 GB/s out of it. On the GPU the arrays must first be shipped across PCIe, which delivers only ~6.9 GB/s in this configuration (see Question 2). So the GPU is fed by a pipe that is ~46× narrower than its own HBM: it finishes the arithmetic in 3.5 ms and then spends 159 ms waiting for data. Concretely, the H2D (108.5 ms) and D2H (50.8 ms) copies account for **97.8%** of the 162.8 ms total, while the kernel contributes only 2.2%.

The same pattern appears at N = 20 M, where the fixed per-transfer overheads are less well amortized: CPU 10.4 ms (21.5 GB/s), kernel only 0.85–0.94 ms (238–262 GB/s), whole process 38.6–44.3 ms (5.1–5.8 GB/s).

A useful way to state the conclusion is in terms of amortization. If the same arrays are reused for K consecutive SAXPY calls, the data is uploaded once and downloaded once:

```
t_GPU ≈ 108.5 + K · 3.52 + 50.8  ms        t_CPU ≈ K · 52.7 ms
```

These are equal at **K ≈ 3.3**, so the GPU only starts to win from the fourth iteration onwards. This is exactly the regime GPU acceleration is designed for (iterative solvers, rendering, training loops, where data is resident on the device and reused many times), and it is precisely why a single isolated memory-bound kernel like SAXPY is a poor fit for offloading.


## Question 2

> *Compare and explain the difference between the results provided by the two sets of timers. Are the bandwidth values observed roughly consistent with the reported bandwidths available to the different components of the machine?*

Two timers were added. The first brackets only the kernel launch and is closed with `cudaDeviceSynchronize()`, which is required because `<<<...>>>` is asynchronous and returns as soon as the work is enqueued; without the synchronization this timer would measure launch overhead (tens of microseconds) rather than execution. The second is the original timer around the entire sequence. No extra synchronization is needed before the end of the whole-process timer, because the blocking `cudaMemcpy` of the result only returns once the copy is complete, and the copy is issued on the same stream as the kernel, so it cannot start before the kernel finishes.

| Measurement | Bytes counted | Time | Effective bandwidth |
|---|---|---|---|
| **Kernel only** | 12N (HBM traffic) | **3.52 ms** | **317 GB/s** |
| **Whole process** | 12N (logical) | **162.8 ms** | **6.86 GB/s** |
| ├ H2D copy of `x`, `y` | 2N = 763 MB | 108.5 ms | 6.87 GB/s |
| └ D2H copy of `result` | N = 381 MB | 50.8 ms | 7.34 GB/s |

**The two timers differ by 46×.** The explanation is purely the memory hierarchy, and both numbers are individually consistent with the hardware — but they are consistent with *different* components:

- **Kernel only: 317 GB/s = 82.6% of the 384 GB/s device peak.** This is the expected result. SAXPY has no data reuse, so it should stream at close to peak HBM bandwidth; the remaining ~17% is DRAM row activation, read/write turnaround and scheduling overhead. Reaching ~83% of peak confirms the kernel is written correctly (one element per thread, coalesced accesses, enough blocks to fill all 26 SMs). Had it come in at, say, 50 GB/s, the kernel would be the problem.
- **Whole process: 6.86 GB/s, only 22% of the 31.5 GB/s theoretical PCIe 5.0 ×8 link.** This is the discrepancy the handout warns about, and the reason is the host memory, not the link.

To confirm that, a targeted micro-benchmark was run: copying a 190 MB buffer to the device from ordinary pageable `malloc` memory versus from page-locked (`cudaHostAlloc`) memory.

| Host buffer type | Time | Bandwidth |
|---|---|---|
| `malloc` (pageable) | 35.2 ms | **5.30 GB/s** |
| `cudaHostAlloc` (pinned) | 17.7 ms | **10.51 GB/s** (1.98×) |

Pinning nearly doubles the achievable bandwidth, reaching 33% of the theoretical link rate. The mechanism is exactly the one described in the handout: pageable host memory cannot be handed to the GPU's DMA engine directly, because its pages may be swapped out or moved, and the physical pages behind a virtual range are not contiguous. The driver must therefore pin the pages on the fly and copy the data through an intermediate "bounce buffer" that is guaranteed to be both resident and physically contiguous. That adds a full extra pass through the CPU, which is why the pageable path is capped at roughly the bandwidth of a single core's `memcpy` from DRAM rather than at the link bandwidth.

Two further observations support this interpretation. First, the pageable figure is essentially independent of link generation: we measure **5.30 GB/s** here on a PCIe 5.0 ×8 link, which coincides almost exactly with the **5.3 GB/s** quoted in the handout for the AWS machine on a PCIe 3.0 ×16 link. If the link were the limiting factor those two numbers could not be the same. Second, even with pinned memory the transfers (1144 MB at 10.5 GB/s ≈ 109 ms) plus the kernel would take ≈ 113 ms, which is still 2.1× slower than the 52.7 ms CPU version — the single-SAXXPY comparison of Question 1 is lost regardless of pinning; pinning only narrows the gap.

The kernel and the copy bandwidths are therefore both "consistent with the reported bandwidths of the components" in the sense the question intends: the kernel figure matches HBM peak, and the copy figure matches what the PCIe path through pageable host memory can actually deliver, which is roughly a third to a fifth of the raw link rate and dominated by pageable-memory overhead rather than by the bus itself.
