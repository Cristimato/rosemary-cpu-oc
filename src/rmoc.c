// SPDX-License-Identifier: GPL-2.0
/*
 * rosemary_oc - MT6785 (Helio G95) CPU OPP0 frequency override via CPUHVFS CSRAM.
 *
 * The SSPM DVFS firmware reads the per-cluster OPP table from CSRAM
 * (CSRAM_BASE + OFFS_TBL_S). Each cluster has 36 u32 words:
 *   [0..15]  = (Vproc_pmic << 16) | freq_MHz
 *   [16..31] = (clk_div << 16) | post_div
 *   [32]     = 0xffffffff terminator
 * Linux only sends an OPP index; the firmware looks the rest up here.
 *
 * Default: read-only probe. Pass big_mhz=/little_mhz= to change OPP0 freq.
 * Voltage is NOT touched here (EEM owns it; use /proc/eem/.../eem_setmargin).
 * Original values are restored on rmmod.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/cpufreq.h>
#include <linux/string.h>
#include <linux/stddef.h>
#include <linux/uaccess.h>
/* device kernel exports the 4.19 name (tree has the newer copy_from_kernel_nofault) */
extern long probe_kernel_read(void *dst, const void *src, size_t size);

#define CSRAM_BASE	0x0011bc00
#define CSRAM_SIZE	0x1400
#define OFFS_TBL_S	0x0010
#define WORDS_PER_CL	36
#define NR_FREQ		16
#define POL_SCAN	0x400
#define CL_LL		0
#define CL_L		1

/*
 * Hard limits (MHz), measured on rosemary:
 *  - big: the SSPM firmware accepts OPP0 up to 2112 exactly; 2113+ halves
 *    the PLL (~1060 MHz) or falls back to OPP1.
 *  - little: anything above 2000 halves the PLL; raw workarounds panicked.
 *    LL_MAX == LL_STOCK keeps little_mhz probe-only.
 */
#define LL_STOCK	2000
#define L_STOCK		2050
#define LL_MAX		2000
#define L_MAX		2112

static uint big_mhz;
module_param(big_mhz, uint, 0444);
MODULE_PARM_DESC(big_mhz, "OPP0 freq for big (A76) cluster in MHz, 0 = keep");
static uint little_mhz;
module_param(little_mhz, uint, 0444);
MODULE_PARM_DESC(little_mhz, "OPP0 freq for little (A55) cluster in MHz, 0 = keep");

static ulong cpu_dvfs_addr;
module_param(cpu_dvfs_addr, ulong, 0444);
MODULE_PARM_DESC(cpu_dvfs_addr, "address of cpu_dvfs[] (from /proc/kallsyms) to fix the kernel-visible freq table");

/* Mirror of MTK struct mt_cpu_dvfs (cpufreq_v1/inc/mtk_cpufreq_struct.h) */
struct mt_cpu_freq_info_m { unsigned int cpufreq_khz, cpufreq_volt; };
struct mt_cpu_dvfs_m {
	const char *name;
	int id;
	unsigned int cpu_id;
	unsigned int cpu_level;
	struct cpufreq_policy *mt_policy;
	int Vproc_buck_id, Vsram_buck_id, Pll_id;
	void *freq_tbl;
	struct mt_cpu_freq_info_m *opp_tbl;
	int nr_opp_tbl, idx_opp_tbl, idx_opp_ppm_base, idx_opp_ppm_limit;
	int armpll_is_available, idx_normal_max_opp;
	struct cpufreq_frequency_table *freq_tbl_for_cpufreq;
	bool dvfs_disable_by_suspend, dvfs_disable_by_procfs;
	unsigned int turbo_mode;
};
static struct mt_cpu_dvfs_m *kp;	/* big cluster entry, NULL if not patched */
static unsigned int *kp_cpuinfo_max;
static unsigned int kp_orig_khz;
static unsigned int *kp_stats_ft;	/* cpufreq_stats freq_table[0] (time_in_state), NULL if not patched */

/*
 * cpufreq_stats keeps its own copy of the freq table, built at policy creation,
 * so stats/time_in_state (what kernel managers like FKM read) keeps showing the
 * old OPP0. Find it by content: some pointer in policy -> stats struct, which
 * holds a pointer to a u32 array starting with {opp0, opp1}.
 */
static void fix_stats_table(const u32 *pbuf, unsigned int f0, unsigned int f1, unsigned int newk)
{
	int i, j, n = 0;
	unsigned int *hit = NULL;

	for (i = 0; i < POL_SCAN / 4; i += 2) {
		unsigned long p, sb[16];

		memcpy(&p, &pbuf[i], sizeof(p));
		if (!p || !virt_addr_valid((void *)p) || probe_kernel_read(sb, (void *)p, sizeof(sb)))
			continue;
		for (j = 0; j < 16; j++) {
			u32 v[2];

			if (!sb[j] || !virt_addr_valid((void *)sb[j]) ||
			    probe_kernel_read(v, (void *)sb[j], sizeof(v)))
				continue;
			if (v[0] == f0 && v[1] == f1 && hit != (unsigned int *)sb[j]) {
				hit = (unsigned int *)sb[j];
				n++;
			}
		}
	}
	if (n != 1) {
		pr_err("rmoc: stats freq_table found x%d, time_in_state NOT fixed\n", n);
		return;
	}
	WRITE_ONCE(hit[0], newk);
	mb();
	kp_stats_ft = hit;
	pr_info("rmoc: stats time_in_state OPP0 %u -> %u kHz\n", f0, newk);
}

/* Make cpufreq sysfs (scaling_available_frequencies, cpuinfo_max_freq) report the real OPP0 */
static void fix_kernel_table(u32 mhz)
{
	struct mt_cpu_dvfs_m *p;
	struct cpufreq_policy *pol;
	unsigned int *ci;
	unsigned int newk = mhz * 1000;

	if (!cpu_dvfs_addr)
		return;
	p = (struct mt_cpu_dvfs_m *)cpu_dvfs_addr + 1;	/* [MT_CPU_DVFS_L] */
	{
		struct mt_cpu_dvfs_m c;
		struct mt_cpu_freq_info_m o0, ol;
		struct cpufreq_frequency_table f0;
		struct cpufreq_frequency_table *pft;
		unsigned int cimax, cimin;
		char nm[16] = {0};

		/* cpu_dvfs[] lives in the kernel image (not linear map): validate with fault-safe reads */
		if (probe_kernel_read(&c, p, sizeof(c)) || !c.name ||
		    probe_kernel_read(nm, c.name, 14) || strncmp(nm, "MT_CPU_DVFS_L", 14)) {
			pr_err("rmoc: cpu_dvfs[1] name mismatch, kernel table NOT fixed\n");
			return;
		}
		if (c.cpu_id != 6 || c.nr_opp_tbl != NR_FREQ || !c.opp_tbl ||
		    !c.freq_tbl_for_cpufreq || !c.mt_policy) {
			pr_err("rmoc: cpu_dvfs fields mismatch cpu=%u nr=%d, NOT fixed\n", c.cpu_id, c.nr_opp_tbl);
			return;
		}
		pol = c.mt_policy;
		if (probe_kernel_read(&o0, &c.opp_tbl[0], sizeof(o0)) ||
		    probe_kernel_read(&ol, &c.opp_tbl[NR_FREQ - 1], sizeof(ol)) ||
		    probe_kernel_read(&f0, &c.freq_tbl_for_cpufreq[0], sizeof(f0))) {
			pr_err("rmoc: table read fault, NOT fixed\n");
			return;
		}
		/*
		 * Device struct cpufreq_policy layout differs from our source tree, so locate
		 * fields by content: freq_table pointer and cpuinfo {max,min} must each
		 * appear exactly once, with cpuinfo before freq_table.
		 */
		{
			static u32 buf[POL_SCAN / 4];
			int i, nft = 0, nci = 0, oft = -1, oci = -1;

			if (probe_kernel_read(buf, pol, sizeof(buf))) {
				pr_err("rmoc: policy read fault, NOT fixed\n");
				return;
			}
			for (i = 0; i + 1 < POL_SCAN / 4; i++) {
				if (!(i & 1)) {
					memcpy(&pft, &buf[i], sizeof(pft));
					if (pft == c.freq_tbl_for_cpufreq) { nft++; oft = i * 4; }
				}
				if (buf[i] == o0.cpufreq_khz && buf[i + 1] == ol.cpufreq_khz) { nci++; oci = i * 4; }
			}
			pr_info("rmoc: policy scan freq_table x%d @0x%x, cpuinfo x%d @0x%x\n", nft, oft, nci, oci);
			if (nft != 1 || nci != 1 || oci >= oft || oci < 0x10 || oci > 0x80) {
				pr_err("rmoc: policy fields not uniquely found, NOT fixed\n");
				return;
			}
			ci = (unsigned int *)((char *)pol + oci);
			cimax = buf[oci / 4];
			cimin = buf[oci / 4 + 1];
			if (o0.cpufreq_khz != L_STOCK * 1000 || f0.frequency != L_STOCK * 1000 ||
			    cimax != L_STOCK * 1000 || cimin != ol.cpufreq_khz) {
				pr_err("rmoc: unexpected values opp=%u tbl=%u cimax=%u cimin=%u last=%u, NOT fixed\n",
				       o0.cpufreq_khz, f0.frequency, cimax, cimin, ol.cpufreq_khz);
				return;
			}
			/* all checks passed: only now touch time_in_state */
			{
				struct mt_cpu_freq_info_m o1;

				if (!probe_kernel_read(&o1, &c.opp_tbl[1], sizeof(o1)))
					fix_stats_table(buf, o0.cpufreq_khz, o1.cpufreq_khz, newk);
			}
		}
	}
	kp_orig_khz = p->opp_tbl[0].cpufreq_khz;
	p->opp_tbl[0].cpufreq_khz = newk;
	p->freq_tbl_for_cpufreq[0].frequency = newk;
	WRITE_ONCE(*ci, newk);
	mb();
	kp = p;
	kp_cpuinfo_max = ci;
	pr_info("rmoc: kernel cpufreq table OPP0 %u -> %u kHz\n", kp_orig_khz, newk);
}

static void restore_kernel_table(void)
{
	if (!kp)
		return;
	kp->opp_tbl[0].cpufreq_khz = kp_orig_khz;
	kp->freq_tbl_for_cpufreq[0].frequency = kp_orig_khz;
	WRITE_ONCE(*kp_cpuinfo_max, kp_orig_khz);
	if (kp_stats_ft)
		WRITE_ONCE(kp_stats_ft[0], kp_orig_khz);
	mb();
}

static void __iomem *csram;
static u32 orig_l0, orig_ll0;
static bool changed_l, changed_ll;

static inline void __iomem *tbl_word(int cl, int idx)
{
	return csram + OFFS_TBL_S + (cl * WORDS_PER_CL + idx) * 4;
}

static int validate_cluster(int cl, u32 stock_mhz)
{
	u32 w0 = readl(tbl_word(cl, 0));
	u32 term = readl(tbl_word(cl, 2 * NR_FREQ));
	u32 div0 = readl(tbl_word(cl, NR_FREQ));
	u32 prev = 0xffff;
	int i;

	if (term != 0xffffffff) {
		pr_err("rmoc: cl%d terminator 0x%08x != ffffffff\n", cl, term);
		return -ENODEV;
	}
	/* frequencies must be strictly descending */
	for (i = 0; i < NR_FREQ; i++) {
		u32 f = readl(tbl_word(cl, i)) & 0xffff;

		if (f == 0 || f >= prev) {
			pr_err("rmoc: cl%d idx%d freq %u not descending\n", cl, i, f);
			return -ENODEV;
		}
		prev = f;
	}
	if ((w0 & 0xffff) != stock_mhz) {
		pr_err("rmoc: cl%d OPP0 %u MHz != stock %u (already modified?)\n",
		       cl, w0 & 0xffff, stock_mhz);
		return -EBUSY;
	}
	pr_info("rmoc: cl%d OPP0 %u MHz vproc_pmic=%u div=0x%08x OK\n",
		cl, w0 & 0xffff, w0 >> 16, div0);
	return 0;
}

static void dump_cluster(int cl)
{
	int i;

	for (i = 0; i < NR_FREQ; i++) {
		u32 w = readl(tbl_word(cl, i));
		u32 d = readl(tbl_word(cl, NR_FREQ + i));

		pr_info("rmoc: cl%d[%2d] %4u MHz pmic=%3u (%u uV) post=%u clk=%u\n",
			cl, i, w & 0xffff, w >> 16, 400000 + (w >> 16) * 6250,
			d & 0xffff, d >> 16);
	}
}

static void set_freq(int cl, u32 orig, u32 mhz)
{
	u32 nw = (orig & 0xffff0000) | (mhz & 0xffff);

	writel(nw, tbl_word(cl, 0));
	mb();
	pr_info("rmoc: cl%d OPP0 0x%08x -> 0x%08x (%u MHz)\n", cl, orig, nw, mhz);
}

static int __init rosemary_oc_init(void)
{
	int ret;

	if (big_mhz && (big_mhz < L_STOCK || big_mhz > L_MAX)) {
		pr_err("rmoc: big_mhz %u out of [%u,%u]\n", big_mhz, L_STOCK, L_MAX);
		return -EINVAL;
	}
	if (little_mhz && (little_mhz < LL_STOCK || little_mhz > LL_MAX)) {
		pr_err("rmoc: little_mhz %u out of [%u,%u]\n", little_mhz, LL_STOCK, LL_MAX);
		return -EINVAL;
	}

	csram = ioremap_nocache(CSRAM_BASE, CSRAM_SIZE);
	if (!csram)
		return -ENOMEM;

	ret = validate_cluster(CL_LL, LL_STOCK);
	if (!ret)
		ret = validate_cluster(CL_L, L_STOCK);
	if (ret) {
		iounmap(csram);
		return ret;
	}

	dump_cluster(CL_LL);
	dump_cluster(CL_L);

	orig_ll0 = readl(tbl_word(CL_LL, 0));
	orig_l0 = readl(tbl_word(CL_L, 0));

	if (big_mhz && big_mhz != L_STOCK) {
		set_freq(CL_L, orig_l0, big_mhz);
		changed_l = true;
		fix_kernel_table(big_mhz);
	}
	if (little_mhz && little_mhz != LL_STOCK) {
		set_freq(CL_LL, orig_ll0, little_mhz);
		changed_ll = true;
	}
	if (!changed_l && !changed_ll)
		pr_info("rmoc: probe-only mode, nothing changed\n");
	return 0;
}

static void __exit rosemary_oc_exit(void)
{
	restore_kernel_table();
	/* restore freq field only; keep whatever voltage EEM set meanwhile */
	if (changed_l) {
		u32 cur = readl(tbl_word(CL_L, 0));

		writel((cur & 0xffff0000) | (orig_l0 & 0xffff), tbl_word(CL_L, 0));
	}
	if (changed_ll) {
		u32 cur = readl(tbl_word(CL_LL, 0));

		writel((cur & 0xffff0000) | (orig_ll0 & 0xffff), tbl_word(CL_LL, 0));
	}
	mb();
	iounmap(csram);
	pr_info("rmoc: restored stock OPP0, unloaded\n");
}

module_init(rosemary_oc_init);
module_exit(rosemary_oc_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MT6785 CPU OPP0 override via CPUHVFS CSRAM");
