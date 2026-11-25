# rmoc — overclock dos núcleos grandes do MT6785 (Helio G95)

Módulo de kernel + módulo KernelSU/Magisk que sobe o OPP0 do cluster grande
(2× Cortex-A76) do **MediaTek MT6785 / Helio G95** de **2050 → 2112 MHz**, sem
trocar o kernel e sem gravar nada em partição.

| | Stock | Com rmoc |
|---|---|---|
| A76 (cpu6–7), OPP0 | 2050 MHz @ 1,000 V | **2112 MHz @ 1,025 V** |
| A55 (cpu0–5) | 2000 MHz | inalterado |
| `cpuinfo_max_freq` / `time_in_state` | 2050000 | 2112000 |

> **+3 % parece pouco, e é o máximo possível neste chip.** 2112 MHz é um teto
> do firmware SSPM, não uma escolha conservadora. Veja
> [docs/COMO-FUNCIONA.md](docs/COMO-FUNCIONA.md#o-teto-de-2112-mhz).

## Compatibilidade

- **SoC:** MT6785 (testado no Redmi Note 10S — `rosemary`)
- **Kernel validado:** `4.19.325-cip134-st18-YuuriLegacy:3`
- **Root:** KernelSU / SukiSU (testado); Magisk e APatch devem funcionar (zip padrão)

O `rmoc.ko` pré-compilado só é carregado no kernel validado: o serviço confere
`uname -r` e aborta em qualquer outro. O `vermagic` do binário
(`4.19.325-Yuuri:[Shuumatsu]-…`) vem da árvore de build, não do kernel do
aparelho. Ele carrega porque o kernel usa `CONFIG_MODVERSIONS`, que valida
pelos CRCs dos símbolos e ignora o nome da versão. Para outro kernel 4.19 do MT6785,
recompile (ver [Compilar](#compilar)) e ajuste `KERNEL_OK` em
`module/service.sh`.

## Instalação

1. Baixe `rmoc_oc-v5.1.zip` em [Releases](../../releases).
2. Instale pelo gerenciador de root e reinicie.
3. Aguarde ~90 s após o boot e confira:

```sh
su -c cat /data/adb/rmoc.log
cat /sys/devices/system/cpu/cpufreq/policy6/cpuinfo_max_freq   # 2112000
su -c cat /proc/cpufreq/MT_CPU_DVFS_L/cpufreq_oppidx           # 0 (2112000, 102500)
```

## Segurança

| Mecanismo | Como funciona |
|---|---|
| Atraso de 90 s | O OC só entra depois do boot completo, então dá tempo de criar o kill switch. |
| Anti-bootloop | Grava `pending` antes de aplicar e `ok` após 5 min estáveis. Se o aparelho reiniciar antes disso, o próximo boot **desliga o OC sozinho**. |
| Kill switch | `su -c touch /data/adb/rmoc_disable` e reinicie. |
| Validação da tabela | O módulo só escreve se a tabela CSRAM tiver o formato esperado (terminador, frequências decrescentes, OPP0 = stock). |
| Reversível | `rmmod rmoc` devolve os valores stock na hora. A CSRAM é volátil: sem o módulo, o próximo boot é 100 % stock. |

> **Atenção ao anti-bootloop:** ele não distingue travamento de reinício
> normal. Se você reiniciar nos primeiros ~6,5 min após o boot (90 s + 5 min),
> o próximo boot desliga o OC e cria `/data/adb/rmoc_disable`. Para religar:
> `su -c rm /data/adb/rmoc_disable` e reinicie.

**Desinstalar:** remova pelo gerenciador e reinicie.

**Desligar sem reiniciar:**

```sh
su -c 'rmmod rmoc; echo "setopp 0 0" > /proc/eem/EEM_DET_B/eem_setmargin
P=/sys/devices/system/cpu/cpufreq/policy6; m=$(cat $P/scaling_max_freq)
echo 1986000 > $P/scaling_max_freq; sleep 0.3; echo $m > $P/scaling_max_freq'
```

O `rmmod` devolve as tabelas ao stock na hora. A troca do `scaling_max_freq`
força o SSPM a reler o OPP0; sem ela, o clock antigo continua até a próxima
mudança de frequência.

## Limitações conhecidas

- **Termal da Xiaomi:** com o `mi_thermald` rodando, ele pode limitar o cluster
  grande pelo PPM (`sysboost_cluster_freqidx_limit`, usuário 8 / `XM_THERM`) e o
  2112 só aparece com o aparelho frio. O módulo não mexe nisso de propósito.
- **Cluster pequeno (A55):** não suportado. Acima de 2000 MHz o firmware divide
  o PLL por 2, e contornar isso derrubou o kernel nos testes. O módulo recusa
  `little_mhz` diferente de 2000 e `big_mhz` acima de 2112.
- **Undervolt:** não empilhe undervolt negativo no cluster grande
  (`eem_offset` B < 0). Com −16 passos o OPP0 rodou a 0,925 V e travou o aparelho.

## Compilar

Precisa da árvore do kernel 4.19 do aparelho com o **mesmo `.config`** do kernel
em execução (`/proc/config.gz`) e `make modules_prepare` feito.

```sh
# kernel validado (YuuriLegacy:3): precisa da correção do campo exit
KDIR=~/kernel EXIT_FIX=0x378:0x370 tools/build.sh
tools/make_module.sh              # gera out/rmoc_oc-<versão>.zip
```

`EXIT_FIX` só serve para kernels cujo `struct module` difere da árvore de
fonte. Em outro kernel, compile **sem** ele primeiro. Use só se o módulo
aparecer como `[permanent]` em `/proc/modules`. O `fixko.py` confere que a
relocação alterada é mesmo a de `cleanup_module` e não mexe em nada se não for.

Ferramentas de teste em `tools/`:

- `stab.c`: estresse determinístico por núcleo, com verificação dos resultados (`stab 60 6 7`).
- `mhz.c`: mede o clock real por cadeia de `add` dependentes (subestima ~3–4 %).
- `mon.sh`: mostra frequência, tensão e temperatura a cada segundo.

## Documentação

- [Como funciona](docs/COMO-FUNCIONA.md): tabela CSRAM, EEM, SSPM e o teto de 2112 MHz
- [Validação](docs/VALIDACAO.md): testes de estabilidade e o que já deu errado
- [CHANGELOG](CHANGELOG.md)

## Aviso

Overclock fora de especificação. Use por sua conta e risco. O autor não se
responsabiliza por instabilidade, perda de dados ou dano ao hardware.

## Licença

GPL-2.0 ([LICENSE](LICENSE)), como exigido para módulos do kernel Linux.
