# Como funciona

## Quem controla o clock no MT6785

No MT6785 o Linux não programa o PLL da CPU diretamente. O driver `cpufreq`
só manda um **índice de OPP** para o **SSPM** (microcontrolador de energia),
e o firmware do SSPM busca frequência, tensão e divisores numa tabela na
**CSRAM do CPUHVFS**:

```
CSRAM_BASE = 0x0011bc00, tabela em +0x10, 36 words u32 por cluster

cluster 0 = LL (A55), cluster 1 = L (A76)
  [0..15]  (Vproc_pmic << 16) | freq_MHz     ← OPP0..OPP15
  [16..31] (clk_div   << 16) | post_div
  [32]     0xffffffff                         ← terminador
```

O `rmoc.ko` faz `ioremap` dessa região, valida a tabela e reescreve **só o
campo de frequência** da word OPP0 do cluster L. A tensão fica com o
**EEM** (o AVS da MediaTek), que reescreve os campos de tensão sozinho. Por
isso o aumento de tensão é feito do lado do EEM, antes do `insmod`:

```sh
echo "setopp 0 4" > /proc/eem/EEM_DET_B/eem_setmargin   # +4 × 6,25 mV no OPP0
```

Depois de escrever a tabela, o serviço baixa e restaura o
`scaling_max_freq` da policy6. Isso força o cpufreq a pedir o OPP0 de novo, e
o SSPM relê a tabela.

## Tabela que o kernel mostra

Reescrever a CSRAM já muda o clock real, mas o kernel continuaria mostrando
2050 MHz em `cpuinfo_max_freq`, `scaling_available_frequencies` e
`stats/time_in_state` (o que apps como Franco Kernel Manager leem). O serviço
passa o endereço de `cpu_dvfs[]` (lido de `/proc/kallsyms`) e o módulo:

1. confere por leitura tolerante a falhas (`probe_kernel_read`) que
   `cpu_dvfs[1]` é `MT_CPU_DVFS_L`, com `cpu_id == 6` e 16 OPPs;
2. corrige `opp_tbl[0]` e `freq_tbl_for_cpufreq[0]`;
3. acha `policy->cpuinfo.max_freq` e a `freq_table` do `cpufreq_stats`
   **pelo conteúdo**: o layout de `struct cpufreq_policy` do kernel do
   aparelho é diferente do fonte público, então offsets fixos não servem. Se
   alguma busca não der exatamente 1 resultado, o módulo não escreve nada e só
   registra no log.

No `rmmod`, tudo volta aos valores originais.

## O teto de 2112 MHz

Para achar o limite, a frequência do OPP0 foi alterada ao vivo e o clock real
foi medido com `tools/mhz.c`:

| OPP0 pedido | Resultado real |
|---|---|
| 2112 MHz | ✅ 2112 |
| 2113, 2118, 2125, 2150 | ❌ SSPM divide o PLL por 2 (~1060 MHz) |
| 2114 | ❌ SSPM ignora e cai para OPP1 |

O firmware do SSPM aceita o OPP0 até cerca de 103 % do valor de fábrica.
Outros caminhos testados, todos sem sucesso:

- **Forçar `post_div` na word DIV com OPP0 = 2150:** congelou o aparelho. Não repita.
- **Modo turbo (`cpuhvfs_set_turbo_mode` via IPI):** no MT6785 o lado do AP é
  stub, e o IPI congelou o aparelho. É um beco sem saída.
- **Cluster A55 acima de 2000:** o PLL é dividido por 2. Dobrar o valor cru
  para compensar causou kernel panic no idle do CPU0.

Ir além de 2112 exigiria modificar o firmware do SSPM (`tinysys-sspm`), que é
cifrado, com risco real de brick.

## Pegadinhas de build

- **`.config` diferente do aparelho = panic.** Com `CONFIG_SHADOW_CALL_STACK=y`
  na árvore (desligado no aparelho), todo prólogo grava em `[x18]`. No aparelho,
  x18 é lixo, e o `insmod` derruba o kernel. O `build.sh` checa isso.
- **`exit` em `__this_module`:** o kernel do aparelho tem o campo em 0x370, e a
  árvore em 0x378. Sem o `fixko.py` o módulo carrega, mas fica `[permanent]`.
  A correção é específica deste kernel, por isso o `build.sh` só a aplica com
  `EXIT_FIX`.
- **`probe_kernel_read`:** o aparelho exporta o nome antigo (4.19), não
  `copy_from_kernel_nofault`, por isso o `extern` manual.
- **`cpu_dvfs[]` fica na imagem do kernel:** `virt_addr_valid()` é sempre
  falso para ele, então a validação usa leituras tolerantes a falha.
