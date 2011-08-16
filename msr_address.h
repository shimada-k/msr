/* 
*
*	Nehalem register address
*
*/
#if 0
#define	IA32_PERF_GLOBAL_CTRL			0x38F

#define 	IA32_FIXED_CTR1				0x30A	/* CPU_Unhalted.Core */
#define 	IA32_FIXED_CNTR_CTRL				0x38D	/* P.1138 */


/* p.1616 */
#define	IA32_PMC0					0xC1	/* MEM_LOAD_RETIRED.LLC_MISS */
#define	IA32_PMC1					0xC2	/* MEM_LOAD_RETIRED.LLC_UNSHARED_HIT */
#define	IA32_PMC2					0xC3	/* MEM_LOAD_RETIRED.OTHER_CORE_L2_HIT_HITM */
#define	IA32_PMC3					0xC4	/* CPU_CLK_UNHALTED.THREAD_P */

/* P.1137 P.1621 */
#define	IA32_PERFEVENTSEL0				0x186
#define	IA32_PERFEVENTSEL1				0x187
#define	IA32_PERFEVENTSEL2				0x188
#define	IA32_PERFEVENTSEL3				0x189

#define	EVENT_CACHE_MISS				0xCB	/* 0xCB 0x2E */
#define	UMASK_LLC_MISS				0x10	/* 0x10 0x41*/
#define	UMASK_LLC_UNSHARED_HIT			0x04
#define	UMASK_LLC_OTHER_CORE_L2_HIT_HITM		0x08

#define	EVENT_CPU_CLK					0x3C
#define	UMASK_CPU_CLK_UNHALTED_THREAD_P		0x00

/* UNCOREイベント(p.1296) */
#define	MSR_UNCORE_PERF_GLOBAL_CTRL			0x391
#define	MSR_UNCORE_PERFEVTSEL0			0x3C0	/* READ */
#define	MSR_UNCORE_PERFEVTSEL1			0x3C1	/* WRITE */
#define	MSR_UNCORE_PERFEVTSEL2			0x3C2	/* ANY */
#define	MSR_UNCORE_PERFEVTSEL3			0x3C3	/* PROBE */

#define	MSR_UNCORE_PMC0				0x3B0	/* READ */
#define	MSR_UNCORE_PMC1				0x3B1	/* WRITE */
#define	MSR_UNCORE_PMC2				0x3B2	/* ANY */
#define	MSR_UNCORE_PMC3				0x3B3	/* PROBE */

#define	UNC_L3_MISS_EVTNUM				0x09
#define	UNC_L3_MISS_READ_UMASK			0x01
#define	UNC_L3_MISS_WRITE_UMASK			0x02
#define	UNC_L3_MISS_ANY_UMASK			0x03
#define	UNC_L3_MISS_PROBE_UMASK			0x04
#endif

/* 
*
*	SandyBridge register address
*
*/
#if 1
#define	IA32_PERF_GLOBAL_CTRL			0x38F

#define 	IA32_FIXED_CTR1				0x30A	/* CPU_Unhalted.Core */
#define 	IA32_FIXED_CNTR_CTRL				0x38D	/* P.1138 */


/* IA32_PMCx */
/* scope:thread */
#define	IA32_PMC0					0xC1
#define	IA32_PMC1					0xC2
#define	IA32_PMC2					0xC3
#define	IA32_PMC3					0xC4
/* scope:core */
#define	IA32_PMC4					0xC5
#define	IA32_PMC5					0xC6
#define	IA32_PMC6					0xC7
#define	IA32_PMC7					0xC8

/* IA32_PERFEVENTSELx */
/* scope:thread */
#define	IA32_PERFEVENTSEL0				0x186
#define	IA32_PERFEVENTSEL1				0x187
#define	IA32_PERFEVENTSEL2				0x188
#define	IA32_PERFEVENTSEL3				0x189
/* scope:core */
#define	IA32_PERFEVENTSEL4				0x186
#define	IA32_PERFEVENTSEL5				0x187
#define	IA32_PERFEVENTSEL6				0x188
#define	IA32_PERFEVENTSEL7				0x189

#define	EVENT_LONGEST_CACHE_LAT			0x2E
#define	UMASK_LONGEST_CACHE_LAT_MISS		0x41
#define	UMASK_LONGEST_CACHE_LAT_REFERENCE		0x4F

/* UNCOREイベント(p.1296) */
#define	MSR_UNCORE_PERF_GLOBAL_CTRL			0x391
#define	MSR_UNCORE_PERFEVTSEL0			0x3C0	/* READ */
#define	MSR_UNCORE_PERFEVTSEL1			0x3C1	/* WRITE */
#define	MSR_UNCORE_PERFEVTSEL2			0x3C2	/* ANY */
#define	MSR_UNCORE_PERFEVTSEL3			0x3C3	/* PROBE */

#define	MSR_UNCORE_PMC0				0x3B0	/* READ */
#define	MSR_UNCORE_PMC1				0x3B1	/* WRITE */
#define	MSR_UNCORE_PMC2				0x3B2	/* ANY */
#define	MSR_UNCORE_PMC3				0x3B3	/* PROBE */

#define	UNC_L3_MISS_EVTNUM				0x09
#define	UNC_L3_MISS_READ_UMASK			0x01
#define	UNC_L3_MISS_WRITE_UMASK			0x02
#define	UNC_L3_MISS_ANY_UMASK			0x03
#define	UNC_L3_MISS_PROBE_UMASK			0x04
#endif

