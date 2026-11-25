# Changelog

## v5.1 — 2026-11-25

- **Limites do módulo travados no que foi medido:** `big_mhz` ≤ 2112 (teto do SSPM) e `little_mhz` só 2000 (o A55 não aceita OC). Antes aceitava até 2400/2200.
- **Correção:** o `time_in_state` só é alterado depois de todas as validações. Antes, se a última checagem falhasse, ele ficava em 2112 mesmo após o `rmmod`.
- `tools/fixko.py` exige offsets explícitos e confere que a relocação é a de `cleanup_module`. O `build.sh` só aplica com `EXIT_FIX`.
- README: aviso sobre falso positivo do anti-bootloop, como desligar sem reiniciar e explicação do `vermagic`.
- `customize.sh`: removidos codinomes de aparelhos não testados.

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
