#!/system/bin/sh
# rmoc_oc — aplica o OC dos núcleos grandes (A76) no MT6785 depois do boot.
#
# Segurança:
#   - espera 90 s após o boot (dá tempo de abrir o gerenciador de root e criar o kill switch)
#   - kill switch: touch /data/adb/rmoc_disable  -> OC não é aplicado
#   - anti-bootloop: grava "pending" antes do insmod e "ok" após 5 min estáveis;
#     se o boot seguinte encontrar "pending", o OC se desliga sozinho
#   - só carrega no kernel validado (layout de structs/ABI do módulo dependem dele)
MODDIR=${0%/*}
ST=/data/adb/rmoc_state
LOG=/data/adb/rmoc.log
BIG_MHZ=2112          # teto do firmware SSPM; acima disso ele cai para OPP1 ou divide o PLL
BIG_MARGIN=4          # passos EEM (6,25 mV cada) somados à tensão do OPP0 => +25 mV
KERNEL_OK="4.19.325-cip134-st18-YuuriLegacy:3"
log() { echo "$(date '+%F %T') $*" >> $LOG; }

until [ "$(getprop sys.boot_completed)" = "1" ]; do sleep 2; done
sleep 90

if [ -f /data/adb/rmoc_disable ]; then log "kill switch presente, OC nao aplicado"; exit 0; fi
if [ "$(cat $ST 2>/dev/null)" = "pending" ]; then
  log "boot anterior nao confirmou estabilidade -> OC DESATIVADO automaticamente"
  touch /data/adb/rmoc_disable; echo disabled > $ST; exit 0
fi
[ "$(uname -r)" = "$KERNEL_OK" ] || { log "kernel diferente ($(uname -r)), abortando"; exit 0; }

echo pending > $ST; sync
echo "setopp 0 $BIG_MARGIN" > /proc/eem/EEM_DET_B/eem_setmargin
sleep 1

# endereço de cpu_dvfs[] para o módulo corrigir a tabela que o kernel mostra (cpuinfo_max_freq etc.)
KR=$(cat /proc/sys/kernel/kptr_restrict); echo 0 > /proc/sys/kernel/kptr_restrict
DVFS=$(awk '$3=="cpu_dvfs"{print $1}' /proc/kallsyms); echo $KR > /proc/sys/kernel/kptr_restrict
case "$DVFS" in ""|0000000000000000) EXTRA="";; *) EXTRA="cpu_dvfs_addr=0x$DVFS";; esac

if ! insmod $MODDIR/rmoc.ko big_mhz=$BIG_MHZ $EXTRA; then
  log "insmod falhou: $(dmesg | grep rmoc | tail -2)"
  echo "setopp 0 0" > /proc/eem/EEM_DET_B/eem_setmargin; echo failed > $ST; exit 0
fi

# força o cpufreq a pedir o OPP0 de novo para o SSPM ler a tabela nova
P=/sys/devices/system/cpu/cpufreq/policy6
m=$(cat $P/scaling_max_freq); echo 1986000 > $P/scaling_max_freq; sleep 0.3; echo $m > $P/scaling_max_freq

log "tabela kernel: $(dmesg | grep -E "rmoc: (kernel cpufreq|.*NOT fixed)" | tail -1 | sed "s/.*rmoc: //")"
log "OC aplicado: L $(cat /proc/cpufreq/MT_CPU_DVFS_L/cpufreq_freq 2>/dev/null) $(head -1 /proc/cpufreq/MT_CPU_DVFS_L/cpufreq_volt 2>/dev/null)"
sleep 300
echo ok > $ST; sync; log "5 min estavel, confirmado"
