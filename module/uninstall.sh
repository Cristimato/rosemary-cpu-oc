#!/system/bin/sh
# Remove estado e kill switch. A tabela volta ao stock no próximo boot (CSRAM é volátil).
rm -f /data/adb/rmoc_state /data/adb/rmoc_disable
