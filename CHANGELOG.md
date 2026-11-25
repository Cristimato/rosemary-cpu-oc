# Changelog

## v5.0 — 2026-11-25 (primeira versão pública)

Empacota o `rmoc.ko` v5, o mesmo binário que roda desde 2026-11-23.

- `stats/time_in_state` agora mostra 2112 MHz (FKM e similares exibem o OC).
- Zip instalável com `customize.sh`: confere a plataforma MT6785 e avisa se o kernel não for o validado.
- `service.sh` documentado; o kernel validado fica em `KERNEL_OK`.
- `tools/build.sh` recusa módulos com shadow call stack (x18).

## v4 — 2026-11-23

- Parâmetro `cpu_dvfs_addr`: corrige a tabela vista pelo kernel (`cpuinfo_max_freq`, `scaling_available_frequencies`).
- Campos de `cpufreq_policy` localizados pelo conteúdo, porque o layout do aparelho difere do fonte.

## v1 — 2026-11-21

- Reescrita do OPP0 do cluster A76 na CSRAM do CPUHVFS (2050 → 2112 MHz).
- +4 passos de tensão via EEM, anti-bootloop, kill switch e restauração no `rmmod`.
