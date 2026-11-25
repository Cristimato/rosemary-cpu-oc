// mhz <cpu>: mede o clock real do núcleo (cadeia de adds dependentes, 1 ciclo cada no A76/A55)
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(int c,char**v){
  cpu_set_t s;CPU_ZERO(&s);CPU_SET(c>1?atoi(v[1]):7,&s);sched_setaffinity(0,sizeof s,&s);
  long n=(c>2?atol(v[2]):2000000000L);unsigned long x=0;
  for(int w=0;w<2;w++){double t0=now();
    __asm__ volatile("mov x9,%1\n1:\n"
      ".rept 100\nadd %0,%0,#1\n.endr\n"
      "subs x9,x9,#1\nb.ne 1b\n":"+r"(x):"r"(n/100):"x9","cc");
    double dt=now()-t0; if(w) printf("%d MHz\n",(int)(n/dt/1e6+0.5));}
  return 0;}
