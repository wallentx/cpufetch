# ARM capabilities and floating-point throughput

On Linux AArch64, cpufetch decodes all three auxiliary-vector hardware capability
banks. These are kernel-enabled userspace capabilities, not guesses from the SoC
name. The ABI table lives in `src/arm/hwcaps.h`; unknown future bits remain visible
as `HWCAPn_BITm`. Names follow Linux's HWCAP constants (ASIMD is NEON).
Normal output retains the abbreviated, colorized feature list under each CPU
cluster, including SME and SME2. The complete capability list, vector widths,
and SME peak limitations are available with `--debug`. Other platforms retain
their existing feature probes.

Source: [Linux ARM64 ELF hwcaps](https://docs.kernel.org/arch/arm64/elf_hwcaps.html)
and [ABI definitions](https://github.com/torvalds/linux/blob/master/arch/arm64/include/uapi/asm/hwcap.h).

## FP32 vector peak

The displayed value is a theoretical non-streaming FP32 vector estimate:

```
cores * max frequency in Hz * vector operations per cycle * (vector bits / 32)
      * 2 for fused multiply-add
```

For the observed Tensor G6 configuration, the non-streaming vector length is
128 bits. C1-Pro issues two vector FP32 FMAs per cycle; C1-Ultra issues six.

| Cores | Core | GHz | FP32 FLOPs/cycle/core | GFLOP/s |
| --- | --- | ---: | ---: | ---: |
| 2 | C1-Pro | 2.649 | 16 | 84.768 |
| 4 | C1-Pro | 3.379 | 16 | 216.256 |
| 1 | C1-Ultra | 4.109 | 48 | 197.232 |
| Total | | | | 498.256 |

This is not a benchmark or a guarantee that all clusters sustain maximum clocks.
An unrecognized microarchitecture does not get an invented one-unit estimate.

Sources: [C1-Pro optimization guide, non-streaming instruction tables](https://documentation-service.arm.com/static/68c2ee168a337a2bc6645d3e)
and [LLVM C1-Ultra scheduling model](https://github.com/llvm/llvm-project/blob/main/llvm/lib/Target/AArch64/AArch64SchedC1Ultra.td).

## SME matrix throughput

The SME streaming vector length is read with `PR_SME_GET_VL`, without changing
thread state. It is distinct from the non-streaming SVE length. A denied or
unsupported query stays unknown; it is not replaced with an assumed 512 bits.

For unpredicated full-width FP32 FMOPA, FLOPs per instruction are:

```
2 * (streaming vector bits / 32)^2
```

At 512 bits that is 512 FLOPs per instruction. A theoretical matrix rate still
requires the engine's instruction throughput, actual engine count and clock.
C1-SME2 engines can be shared between CPUs and have independent DVFS. Neither
CPU core count nor CPU maximum frequency supplies those missing quantities.
Consequently cpufetch reports SME matrix peak as unknown until those inputs can
be established. It does not add SME to the vector estimate.

Source: [C1-SME2 technical reference manual, shared configuration and DVFS](https://documentation-service.arm.com/static/68c0d4c3cccf2a5517016403).

## Validation

On Linux AArch64 with a native C compiler:

```
sh tests/run-arm-tests.sh
./cpufetch --debug
```

The focused tests mock the kernel capability banks and SME vector-length query,
exercise high and unknown capability bits, and verify the Tensor G6 per-cluster
and aggregate arithmetic under UBSan. Debug output includes the raw capability
masks and vector widths for comparison with the kernel.
