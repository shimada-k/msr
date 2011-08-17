#include <stdio.h>
#include <unistd.h>	/* sleep(3) */
#include "msr.h"

#include "msr_address.h"

static FILE *tmp_fp[2];

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

#define USE_NR_MSR	3	/* いくつのMSRを使ってイベントを計測するか */

int main(int argc, char *argv[])
{
	int i, nr_ia32_pmcs;
	MHANDLE *handles[USE_NR_MSR];
	enum msr_scope scope = thread;

	union IA32_PERFEVTSELx reg;
	reg.full = 0;

	/* tempファイルをオープン */
	for(i = 0; i < USE_NR_MSR; i++){
		tmp_fp[i] = tmpfile();
	}

	/* PerfGlobalCtrlレジスタを設定 */
	nr_ia32_pmcs = setup_PERF_GLOBAL_CTRL();

	printf("%d nr_ia32_pmcs registered.\n", nr_ia32_pmcs);

	init_handle_controller(NULL, 100, USE_NR_MSR);	/* CSVファイルはライブラリ側でオープン、100回、USE_NR_MSR個のMSRを使って計測する。という指定 */


	/* PERFEVENTSELの設定 */

	reg.split.EvtSel = EVENT_LONGEST_CACHE_LAT;
	reg.split.UMASK = UMASK_LONGEST_CACHE_LAT_MISS;

	reg.split.USER = 1;
	reg.split.EN = 1;
	setup_IA32_PERFEVTSEL(IA32_PERFEVENTSEL0, &reg);

	reg.split.USER = 0;
	reg.split.OS = 1;
	setup_IA32_PERFEVTSEL(IA32_PERFEVENTSEL1, &reg);

	reg.split.USER = 1;
	reg.split.OS = 1;
	setup_IA32_PERFEVTSEL(IA32_PERFEVENTSEL2, &reg);

	//setup_IA32_PERFEVTSEL_quickly(IA32_PERFEVENTSEL0, UMASK_LONGEST_CACHE_LAT_MISS, EVENT_LONGEST_CACHE_LAT);
	//setup_IA32_PERFEVTSEL_quickly(IA32_PERFEVENTSEL1, UMASK_LONGEST_CACHE_LAT_REFERENCE, EVENT_LONGEST_CACHE_LAT);

	for(i = 0; i < USE_NR_MSR; i++){
		handles[i] = alloc_handle();
	}

	activate_handle(handles[0], "LONGEST_LAT_CACHE.MISS USER only", scope, IA32_PMC0, sub_record_multi);
	activate_handle(handles[1], "LONGEST_LAT_CACHE.MISS OS only", scope, IA32_PMC1, sub_record_multi);
	activate_handle(handles[2], "LONGEST_LAT_CACHE.MISS both ring", scope, IA32_PMC2, sub_record_multi);

	while(1){
		sleep(1);

		if(read_msr() == false){	/* MAX_RECORDS以上計測した */
			puts("time over");
			break;
		}
	}

	/* 後始末 */
	term_handle_controller(NULL);

	for(i = 0; i < USE_NR_MSR; i++){
		fclose(tmp_fp[i]);
	}

	return 0;
}

