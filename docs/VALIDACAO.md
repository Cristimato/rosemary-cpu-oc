# Validação

Aparelho: Redmi Note 10S (`rosemary`), kernel `4.19.325-cip134-st18-YuuriLegacy:3`.

## Estabilidade

| Data | Teste | Resultado |
|---|---|---|
| 2026-11-21 | `stab 90 6 7` @ 2111 MHz / 1,025 V | 0 erros, ~64 °C |
| 2026-11-23 | carga sustentada, termal Xiaomi liberado | 2112 MHz @ 1,025 V, 64 °C |
| 2026-11-25 | `stab 30 6 7` + `mon.sh` | cpu6 778 / cpu7 761 rodadas, **0 erros**, 2111000 kHz @ 1025000 µV, 60 °C |
| 2026-11-21 → 25 | uso diário, vários boots | anti-bootloop sempre confirmou "5 min estável" |

Na versão atual, `cpuinfo_max_freq`, `scaling_available_frequencies` e
`time_in_state` mostram 2112000.

## Incidentes durante o desenvolvimento

Ficam registrados aqui para ninguém repetir:

| Data | O que aconteceu | Causa | Correção |
|---|---|---|---|
| 2026-11-22 | Travamento | Script de undervolt (`eem_offset` B = −16) somado ao OC: OPP0 rodando a 0,925 V | Offset do cluster B = 0. Não empilhar undervolt. |
| 2026-11-23 | Kernel panic no `insmod` | `.config` da árvore com SCS ligado, prólogos usando x18 | `.config` = `/proc/config.gz`; `build.sh` checa x18 |
| 2026-11-23 | Travamento | `post_div` forçado com OPP0 = 2150 | Abandonado; teto em 2112 |
| 2026-11-23 | Kernel panic | Teste do cluster A55 com valor cru 4120 | Cluster A55 fica stock |
| 2026-11-23 | Travamento | IPI de modo turbo para o SSPM | Abandonado |

Nenhum desses incidentes afeta a configuração publicada (A76 @ 2112, +4
passos EEM, sem undervolt).
