#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_clkmgr.h>
#include "mt_cpufreq.h"

/* =========================================================================
 * MAXIMUM HARDWARE OVERCLOCK TO 1.56 GHz @ 1.4V
 * ========================================================================= */
#define CPU_DVFS_FREQ0   (1560000) /* Max Stable Frequency step for MT6580 (in KHz) */
#define CPU_DVFS_FREQ1   (1404000) /* Intermediate Overclock Step */
#define CPU_DVFS_FREQ2   (1300000) /* Factory Baseline Peak */
#define CPU_DVFS_FREQ3   (1196000) 
#define CPU_DVFS_FREQ4   (1001000) 
#define CPU_DVFS_FREQ5   (754000)  
#define CPU_DVFS_FREQ6   (600000)  

#define CPU_DVFS_VOLT0   (140000)  /* Capped at exactly 1.40V via PMIC tracking rules */
#define CPU_DVFS_VOLT1   (132500)  /* 1.325V for 1.4 GHz safety fallback */
#define CPU_DVFS_VOLT2   (125000)  /* 1.25V Factory Stock Limit */
#define CPU_DVFS_VOLT3   (120000)  
#define CPU_DVFS_VOLT4   (110000)  
#define CPU_DVFS_VOLT5   (100000)   
#define CPU_DVFS_VOLT6   (95000)   

#define OP(freq, volt)    { .cpufreq_khz = freq, .cpufreq_volt = volt }

/* Main Dynamic Voltage and Frequency Lookup Table */
static struct mt_cpu_freq_info mt6580_opp_table[] = {
    OP(CPU_DVFS_FREQ0, CPU_DVFS_VOLT0),
    OP(CPU_DVFS_FREQ1, CPU_DVFS_VOLT1),
    OP(CPU_DVFS_FREQ2, CPU_DVFS_VOLT2),
    OP(CPU_DVFS_FREQ3, CPU_DVFS_VOLT3),
    OP(CPU_DVFS_FREQ4, CPU_DVFS_VOLT4),
    OP(CPU_DVFS_FREQ5, CPU_DVFS_VOLT5),
    OP(CPU_DVFS_FREQ6, CPU_DVFS_VOLT6),
};

static struct cpufreq_frequency_table mt6580_freq_table[ARRAY_SIZE(mt6580_opp_table) + 1];

/* =========================================================================
 * SUB-SYSTEM CORES AND EXPORTS
 * ========================================================================= */
unsigned int mt_cpufreq_get_max_freq(void)
{
    return CPU_DVFS_FREQ0; /* Forces internal governor metrics to see 1.56GHz */
}
EXPORT_SYMBOL(mt_cpufreq_get_max_freq);

unsigned int mt_cpufreq_get_cur_volt(void)
{
    unsigned int index = cpufreq_frequency_table_get_index(mt6580_freq_table, cpufreq_quick_get(0));
    if (index < ARRAY_SIZE(mt6580_opp_table))
        return mt6580_opp_table[index].cpufreq_volt * 10;
    return CPU_DVFS_VOLT2 * 10;
}
EXPORT_SYMBOL(mt_cpufreq_get_cur_volt);

static int mt_cpufreq_target(struct cpufreq_policy *policy, unsigned int target_freq, unsigned int relation)
{
    unsigned int index = 0;
    unsigned int target_volt = 0;
    
    if (cpufreq_frequency_table_target(policy, mt6580_freq_table, target_freq, relation, &index)) {
        return -EINVAL;
    }

    target_freq = mt6580_opp_table[index].cpufreq_khz;
    target_volt = mt6580_opp_table[index].cpufreq_volt * 10; 

    /* Route directly to lower hardware platform clock controls */
    mt_clkmgr_set_cpu_freq(target_freq);
    
    return 0;
}

static int mt_cpufreq_init(struct cpufreq_policy *policy)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(mt6580_opp_table); i++) {
        mt6580_freq_table[i].driver_data = i;
        mt6580_freq_table[i].frequency = mt6580_opp_table[i].cpufreq_khz;
    }
    mt6580_freq_table[ARRAY_SIZE(mt6580_opp_table)].frequency = CPUFREQ_TABLE_END;

    policy->clk = NULL;
    policy->cpuinfo.max_freq = CPU_DVFS_FREQ0;
    policy->cpuinfo.min_freq = CPU_DVFS_FREQ6;
    policy->cur = CPU_DVFS_FREQ2;
    
    return cpufreq_table_validate_and_show(policy, mt6580_freq_table);
}

static struct cpufreq_driver mt_cpufreq_driver = {
    .flags = CPUFREQ_STICKY | CPUFREQ_NEED_INITIAL_FREQ_CHECK,
    .verify = cpufreq_generic_frequency_table_verify,
    .target = mt_cpufreq_target,
    .init = mt_cpufreq_init,
    .name = "mt-cpufreq",
};

static int __init mt_cpufreq_driver_init(void)
{
    return cpufreq_register_driver(&mt_cpufreq_driver);
}

static void __exit mt_cpufreq_driver_exit(void)
{
    cpufreq_unregister_driver(&mt_cpufreq_driver);
}

module_init(mt_cpufreq_driver_init);
module_exit(mt_cpufreq_driver_exit);

MODULE_DESCRIPTION("MediaTek MT6580 CPUfreq Driver - Maximum 1.56GHz 1.4V Limit Edition");
MODULE_LICENSE("GPL");
