#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include "src/arm/uarch.h"

extern struct features* get_features_info(void);
extern int64_t get_peak_performance(struct cpuInfo*);
static unsigned long caps[3];
static int streaming_vl = 64;

extern unsigned long __real_getauxval(unsigned long);
unsigned long __wrap_getauxval(unsigned long type) {
  if(type == AT_HWCAP) return caps[0];
  if(type == AT_HWCAP2) return caps[1];
  if(type == 29) return caps[2];
  return __real_getauxval(type);
}
int __wrap_prctl(int option, ...) {
  assert(option == 64);
  return streaming_vl;
}
uint64_t sve_cntb(void) { return 16; }

static void test_capabilities(void) {
  caps[0] = UINT64_C(1) << 1; // ASIMD
  for(unsigned mask = 0; mask < 4; mask++) {
    caps[1] = (mask & 1 ? UINT64_C(1) << 23 : 0) |
              (mask & 2 ? UINT64_C(1) << 37 : 0);
    struct features* f = get_features_info();
    assert(f->SME == !!(mask & 1));
    assert(f->SME2 == !!(mask & 2));
    assert(f->sme_vl == (mask & 1 ? 64 : 0));
    struct cpuInfo cpu = {.feat = f};
    char* text = get_str_features_full(&cpu);
    const char* expected[] = {"ASIMD", "ASIMD,SME", "ASIMD,SME2", "ASIMD,SME,SME2"};
    assert(strcmp(text, expected[mask]) == 0);
    free(text);
    free(f);
  }
  // A denied streaming-length query must not manufacture a width.
  streaming_vl = -1;
  struct features* f = get_features_info();
  assert(f->SME && f->sme_vl == 0);
  free(f);

  // New SDK-independent flags, the highest bit, and future unknown bits survive.
  struct features all = {.hwcaps_valid = true,
    .hwcaps = {UINT64_MAX, UINT64_MAX, UINT64_MAX}};
  struct cpuInfo cpu = {.feat = &all};
  char* text = get_str_features_full(&cpu);
  assert(strstr(text, "SVE_F16MM"));
  assert(strstr(text, "SME2P3"));
  assert(strstr(text, "POE"));
  assert(strstr(text, "HWCAP3_BIT63"));
  unsigned count = 1;
  for(char* p = text; *p; p++) count += *p == ',';
  assert(count == 192);
  free(text);
  memset(all.hwcaps, 0, sizeof(all.hwcaps));
  assert(get_str_features_full(&cpu) == NULL);
}

static void test_fp32_peak(void) {
  struct features features = {.NEON = true, .SVE = true, .cntb = 16,
    .SME = true, .SME2 = true, .sme_vl = 64};
  struct topology topo[3] = {{.total_cores = 2}, {.total_cores = 4}, {.total_cores = 1}};
  struct frequency freq[3] = {{.max = 2649}, {.max = 3379}, {.max = 4109}};
  struct cpuInfo cpu[3] = {0};
  uint32_t midrs[] = {0x411fd8b1, 0x411fd8b1, 0x411fd8c0};
  for(unsigned i = 0; i < 3; i++) {
    cpu[i].arch = get_uarch_from_midr(midrs[i], &cpu[i]);
    cpu[i].feat = &features;
    cpu[i].topo = &topo[i];
    cpu[i].freq = &freq[i];
    cpu[i].num_cpus = 1;
    cpu[i].next_cpu = i < 2 ? &cpu[i+1] : NULL;
  }
  assert(get_peak_performance(&cpu[0]) == INT64_C(84768000000));
  assert(get_peak_performance(&cpu[1]) == INT64_C(216256000000));
  assert(get_peak_performance(&cpu[2]) == INT64_C(197232000000));
  cpu[0].num_cpus = 3;
  assert(get_peak_performance(cpu) == INT64_C(498256000000));
  // Streaming width and SME support cannot multiply the non-streaming estimate.
  features.sme_vl = 256;
  assert(get_peak_performance(cpu) == INT64_C(498256000000));
  features.SME = features.SME2 = false;
  assert(get_peak_performance(cpu) == INT64_C(498256000000));
  freq[1].max = -1;
  assert(get_peak_performance(cpu) == -1);
  for(unsigned i = 0; i < 3; i++) free_uarch_struct(cpu[i].arch);
}

int main(void) {
  test_capabilities();
  test_fp32_peak();
  puts("PASS: ARM capability decoding and FP32 vector estimates");
  return 0;
}
