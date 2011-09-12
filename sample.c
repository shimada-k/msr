#include <stdio.h>
#include <unistd.h>	/* sleep(3) */
#include "msr.h"

#include "msr_address.h"

#define USE_NR_MSR	7	/* いくつのMSRを使ってイベントを計測するか */

static FILE *tmp_fp[USE_NR_MSR];

/*
	前回のデータとの差分を取る関数 ライブラリ側で呼び出される
	msr_handleに格納される関数（scope==package用）
	@handle_id mh_ctl.handles[]の添字
	@val MSRを計測した生データ
	return true/false
*/
bool sub_record_single(int handle_id, u64 *val)
{
	int skip = 0;

	u64 val_last;
	int num;

	/*-- tmp_fpはcloseすると削除されるので、openとcloseはalloc,freeで行うこととする --*/

	fseek(tmp_fp[handle_id], 0, SEEK_SET);

	/* 過去のvalをtmp_fpから読み込む */
	if((num = fread(&val_last, sizeof(u64), 1, tmp_fp[handle_id])) != 1){
		skip = 1;
	}

	fseek(tmp_fp[handle_id], 0, SEEK_SET);

	/* 現在のcpu_valを書き込む */
	fwrite(val, sizeof(u64), 1, tmp_fp[handle_id]);

	fseek(tmp_fp[handle_id], 0, SEEK_SET);

	/* MSRの回数は増えることはあっても減ることはないのでここは絶対0以上 */
	*val -= val_last;

	if(skip){
		return false;
	}
	else{
		return true;
	}
}

/*
	前回のデータとの差分を取る関数 ライブラリ側で呼び出される
	msr_handleに格納される関数（scope==thread or core用）
	@handle_id mh_ctl.handles[]の添字
	@val MSRを計測した生データ
	return true/false
*/
bool sub_record_multi(int handle_id, u64 *val)
{
	int nr_cpus = sysconf(_SC_NPROCESSORS_CONF);
	int skip = 0;

	u64 val_last[nr_cpus];
	int i;
	int num;

	/*-- tmp_fpはcloseすると削除されるので、openとcloseはalloc,freeで行うこととする --*/

	fseek(tmp_fp[handle_id], 0, SEEK_SET);

	/* 過去のvalをtmp_fpから読み込む */
	if((num = fread(val_last, sizeof(u64), nr_cpus, tmp_fp[handle_id])) != nr_cpus){
		skip = 1;
	}

	fseek(tmp_fp[handle_id], 0, SEEK_SET);

	/* 現在のcpu_valを書き込む */
	fwrite(val, sizeof(u64), nr_cpus, tmp_fp[handle_id]);

	fseek(tmp_fp[handle_id], 0, SEEK_SET);

	for(i = 0; i < nr_cpus; i ++){
		/* MSRの回数は増えることはあっても減ることはないのでここは絶対0以上 */
		val[i] -= val_last[i];
	}

	if(skip){
		return false;
	}
	else{
		return true;
	}
}

int main(int argc, char *argv[])
{
	int i, nr_pmcs;
	MHANDLE *handles = NULL;

	union IA32_PERFEVTSELx reg;
	reg.full = 0;

	/* tempファイルをオープン */
	for(i = 0; i < USE_NR_MSR; i++){
		tmp_fp[i] = tmpfile();
	}

	/* PerfGlobalCtrlレジスタを設定 */
	nr_pmcs = setup_PERF_GLOBAL_CTRL();
	nr_pmcs = setup_UNCORE_PERF_GLOBAL_CTRL();
	printf("%d nr_pmcs registered.\n", nr_pmcs);

	if((handles = init_handle_controller(NULL, 100, USE_NR_MSR)) == NULL){	/* CSVファイルはライブラリ側でオープン、100回、USE_NR_MSR個のMSRを使って計測する。という指定 */
		puts("error");
		return 0;
	}


	/* PERFEVENTSELの設定 */

#if 1
	reg.split.EvtSel = EVENT_L3_LAT_CACHE;
	reg.split.UMASK = UMASK_L3_LAT_CACHE_MISS;

	reg.split.USER = 1;
	reg.split.EN = 1;
	setup_IA32_PERFEVTSEL(IA32_PERFEVENTSEL0, &reg);

	reg.split.USER = 0;
	reg.split.OS = 1;
	setup_IA32_PERFEVTSEL(IA32_PERFEVENTSEL1, &reg);

	reg.split.USER = 1;
	reg.split.OS = 1;
	setup_IA32_PERFEVTSEL(IA32_PERFEVENTSEL2, &reg);
#endif

#if 1
	//setup_IA32_PERFEVTSEL_quickly(IA32_PERFEVENTSEL0, UMASK_LONGEST_CACHE_LAT_MISS, EVENT_LONGEST_CACHE_LAT);
	//setup_IA32_PERFEVTSEL_quickly(IA32_PERFEVENTSEL1, UMASK_LONGEST_CACHE_LAT_REFERENCE, EVENT_LONGEST_CACHE_LAT);

	setup_UNCORE_PERFEVTSEL_quickly(MSR_UNCORE_PERFEVTSEL0, UNC_L3_MISS_READ_UMASK, UNC_L3_MISS_EVTNUM);
	setup_UNCORE_PERFEVTSEL_quickly(MSR_UNCORE_PERFEVTSEL1, UNC_L3_MISS_WRITE_UMASK, UNC_L3_MISS_EVTNUM);
	setup_UNCORE_PERFEVTSEL_quickly(MSR_UNCORE_PERFEVTSEL2, UNC_L3_MISS_ANY_UMASK, UNC_L3_MISS_EVTNUM);
	setup_UNCORE_PERFEVTSEL_quickly(MSR_UNCORE_PERFEVTSEL3, UNC_L3_MISS_PROBE_UMASK, UNC_L3_MISS_EVTNUM);

	activate_handle(&handles[0], "UNC_L3_MISS.READ", MSR_SCOPE_PACKAGE, MSR_UNCORE_PMC0, sub_record_single);
	activate_handle(&handles[1], "UNC_L3_MISS.WRITE", MSR_SCOPE_PACKAGE, MSR_UNCORE_PMC1, sub_record_single);
	activate_handle(&handles[2], "UNC_L3_MISS.ANY", MSR_SCOPE_PACKAGE, MSR_UNCORE_PMC2, sub_record_single);
	activate_handle(&handles[3], "UNC_L3_MISS.PROBE", MSR_SCOPE_PACKAGE, MSR_UNCORE_PMC3, sub_record_single);
#endif

	for(i = 0; i < 4; i++){
		add_unified_list(&handles[i]);
	}

#if 1
	activate_handle(&handles[4], "LONGEST_LAT_CACHE.MISS USER only", MSR_SCOPE_THREAD, IA32_PMC0, sub_record_multi);
	activate_handle(&handles[5], "LONGEST_LAT_CACHE.MISS OS only", MSR_SCOPE_THREAD, IA32_PMC1, sub_record_multi);
	activate_handle(&handles[6], "LONGEST_LAT_CACHE.MISS both ring", MSR_SCOPE_THREAD, IA32_PMC2, sub_record_multi);
#endif

	while(1){
		sleep(1);

		if(read_msrs() == false){	/* MAX_RECORDS以上計測した */
			puts("time over");
			break;
		}
	}

	flush_handle_records();

	/* handleの無効化 */
	for(i = 0; i < USE_NR_MSR; i++){
		deactivate_handle(&handles[i]);
	}

	/* 後始末 */
	term_handle_controller();

	for(i = 0; i < USE_NR_MSR; i++){
		fclose(tmp_fp[i]);
	}

	return 0;
}

