// stab: per-core deterministic stress + self-verification
// usage: stab <seconds> <cpu> [cpu...]
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define MEM (4u << 20) /* 4 MiB: spills L2, hits L3/DRAM */

static int secs;
struct targ { int cpu; long rounds; long errors; };

static uint64_t round_work(uint64_t *buf, double *fbuf)
{
	uint64_t x = 0x9E3779B97F4A7C15ull, acc = 0;
	double f = 1.0000001, fa = 0.0;
	size_t n = MEM / 8, i;

	for (i = 0; i < n; i++) {
		x ^= x << 13; x ^= x >> 7; x ^= x << 17;
		buf[i] = x * 0xff51afd7ed558ccdull;
	}
	for (i = 0; i < n / 2; i++) {
		fbuf[i] = fma(f, (double)(i & 1023), fa);
		fa = fbuf[i] * 0.999999 + sqrt((double)(i & 4095));
	}
	for (int pass = 0; pass < 3; pass++)
		for (i = 0; i < n; i++) {
			size_t j = (buf[i] >> 7) % n;
			acc = (acc ^ buf[j]) * 0xc4ceb9fe1a85ec53ull + i;
			buf[i] += acc;
		}
	uint64_t fbits;
	memcpy(&fbits, &fa, 8);
	return acc ^ fbits;
}

static void *worker(void *p)
{
	struct targ *t = p;
	cpu_set_t s;
	CPU_ZERO(&s);
	CPU_SET(t->cpu, &s);
	if (sched_setaffinity(0, sizeof(s), &s)) { perror("affinity"); return NULL; }
	uint64_t *buf = malloc(MEM);
	double *fbuf = malloc(MEM / 2 * sizeof(double) / 8 * 8);
	uint64_t ref = round_work(buf, fbuf);
	time_t end = time(NULL) + secs;
	while (time(NULL) < end) {
		uint64_t r = round_work(buf, fbuf);
		t->rounds++;
		if (r != ref) {
			t->errors++;
			fprintf(stderr, "cpu%d ERRO rodada %ld: %016llx != %016llx\n",
				t->cpu, t->rounds, (unsigned long long)r, (unsigned long long)ref);
		}
	}
	free(buf); free(fbuf);
	return NULL;
}

int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "uso: stab <seg> <cpu>...\n"); return 2; }
	secs = atoi(argv[1]);
	int n = argc - 2;
	pthread_t th[16];
	struct targ ta[16] = {0};
	for (int i = 0; i < n; i++) {
		ta[i].cpu = atoi(argv[i + 2]);
		pthread_create(&th[i], NULL, worker, &ta[i]);
	}
	long errs = 0;
	for (int i = 0; i < n; i++) {
		pthread_join(th[i], NULL);
		printf("cpu%d: %ld rodadas, %ld erros\n", ta[i].cpu, ta[i].rounds, ta[i].errors);
		errs += ta[i].errors;
	}
	printf(errs ? "RESULTADO: INSTAVEL\n" : "RESULTADO: OK\n");
	return errs ? 1 : 0;
}
