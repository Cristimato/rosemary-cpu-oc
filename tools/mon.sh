#!/system/bin/sh
# mon.sh <segundos>: imprime frequência (PLL), tensão do cluster grande e temperatura da CPU a cada segundo
Z=$(for z in /sys/class/thermal/thermal_zone*; do [ "$(cat $z/type)" = mtktscpu ] && echo $z; done | head -1)
for i in $(seq 1 ${1:-10}); do
  f=$(cat /proc/cpufreq/MT_CPU_DVFS_L/cpufreq_freq)
  v=$(head -1 /proc/cpufreq/MT_CPU_DVFS_L/cpufreq_volt)
  echo "$(date +%T) $f | $v | cpu $(cat $Z/temp)"
  sleep 1
done
