#define _XOPEN_SOURCE 500

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>

#include "msr.h"
#include "msr_address.h"
#include "cpuid.h"

/*
	msr.c:MSRを扱うときのライブラリ
	written by shimada-k
	last modify 2011.9.12
*/

/* list管理用構造体 */
struct ListController{
	int length;
	MHANDLE *ulist_top;
	MHANDLE *ulist_current;
};

/* 管理用構造体 */
struct HandleController{
	int nr_cpus;
	int max_records;		/* 何回計測するか */
	FILE *csv;			/* レポートのCSVファイル */
	int nr_handles;		/* 使用するmsr_handleの数 */

	struct ListController list_ctl;	/* 統合形式で出力するためのリスト管理用データ */
	MHANDLE *handles;
};

static unsigned int counter;	/* 秒数カウンタ */
static struct HandleController handle_ctl;


/***
	ここからライブラリ内部関数
					***/

/*
	msr.koで作られるデバイスファイルにアクセスしてMSRから値を読み込む
	@cpu CPU番号
	@offset レジスタのアドレス
*/
static u64 getMsrValue(int cpu, off_t offset)
{
	ssize_t retval;
	int fd;
	u64 msr;
	char pathname[32];

	sprintf(pathname, "/dev/cpu/%d/msr", cpu);
	fd = open(pathname, O_RDONLY);	/* 読み込みモードでオープン */

	if (fd < 0) {
		perror(pathname);
		return 0;
	}

	retval = pread(fd, &msr, sizeof msr, offset);

	if (retval != sizeof msr) {
		fprintf(stderr, "cpu%d pread(..., 0x%zx) = %jd\n",
			cpu, offset, retval);
		exit(-2);
	}

	close(fd);
	return msr;
}

/*
	msr.koで作られるデバイスファイルにアクセスして間接的にMSRに値を書き込む
	@cpu CPU番号
	@offset レジスタのアドレス
	@val 書き込む値
*/
static void putMsrValue(int cpu, off_t offset, u64 val)
{
	ssize_t retval;
	int fd;
	char pathname[32];

	sprintf(pathname, "/dev/cpu/%d/msr", cpu);
	fd = open(pathname, O_WRONLY);	/* 書き込みモードでオープン */

	if (fd < 0) {
		perror(pathname);
	}

	retval = pwrite(fd, &val, sizeof(u64), offset);

	if (retval != sizeof(u64)) {
		fprintf(stderr, "cpu%d pread(..., 0x%zx) = %jd\n",
			cpu, offset, retval);
		exit(-2);
	}

	close(fd);
}

/*
	ハンドルでMSRからデータを読み、バッファに格納する関数
	@handle 読み込みに使用するハンドル
	return 失敗:-1 成功:0
*/
static int getEventValue(MHANDLE *handle)
{
	int handle_id = handle - handle_ctl.handles;

	if(handle->active == false){	/* activateされてなかったら終了 */
		fprintf(stderr, "this handle is not active. skiped.\n");
		return -1;
	}

	if(handle->scope == MSR_SCOPE_THREAD || handle->scope == MSR_SCOPE_CORE){

		int i;
		u64 val[handle_ctl.nr_cpus];
		u64 (*nested_records)[handle_ctl.nr_cpus] = (u64 (*)[handle_ctl.nr_cpus])handle->flat_records;

		for(i = 0; i < handle_ctl.nr_cpus; i++){
			val[i] = getMsrValue(i, (off_t)handle->addr);
		}

		if(handle->pre_closure){	/* クロージャを実行 */
			if(handle->pre_closure(handle_id, val) == -1){
				return -1;
			}
		}

		/* ハンドル内バッファに格納 */
		for(i = 0; i < handle_ctl.nr_cpus; i++){
			nested_records[counter][i] = val[i];
		}

	}
	else{
		u64 val;

		val = getMsrValue(0, (off_t)handle->addr);	/* 0番のCPUで実行 */

		//printf("%llu\n", val);

		if(handle->pre_closure){	/* 前回のデータとの差分をとって、今回のデータをクロージャ内に置いてくる */
			if(handle->pre_closure(handle_id, &val) == -1){
				return -1;
			}
		}

		printf("%llu\n", val);

		/* ハンドル内バッファに格納 */
		handle->flat_records[counter] = val;
	}

	return 0;
}

/*
	ハンドル内バッファに溜まったデータを書き出す関数
	@handle 書き出す対象のハンドル
*/
static void flushRecordsByHandle(MHANDLE *handle)
{
	int i, j;

	fprintf(handle_ctl.csv, "%s\n", handle->tag);

	if(handle->scope == MSR_SCOPE_THREAD || handle->scope == MSR_SCOPE_CORE){
		for(i = 0; i < handle_ctl.nr_cpus; i++){
			fprintf(handle_ctl.csv, ",CPU%d", i);
		}
	}
	else{
		fprintf(handle_ctl.csv, ",value");
	}

	fprintf(handle_ctl.csv, "\n");

	if(handle->scope == MSR_SCOPE_THREAD || handle->scope == MSR_SCOPE_CORE){
		/* 1次元配列を2次元配列にキャスト */
		u64 (*nested_records)[handle_ctl.nr_cpus] = (u64 (*)[handle_ctl.nr_cpus])handle->flat_records;

		printf("%llu,%llu\n", nested_records[0][0], nested_records[0][1]);

		for(i = 0; i < counter; i++){
			fprintf(handle_ctl.csv, "%d", i);	/* 時間軸を書き込む */

			for(j = 0; j < handle_ctl.nr_cpus; j++){
				fprintf(handle_ctl.csv, ",%llu", nested_records[i][j]);	/* 値を書き込む */
			}
			fprintf(handle_ctl.csv, "\n");
		}
	}
	else{
		for(i = 0; i < counter; i++){
			fprintf(handle_ctl.csv, "%d", i);	/* 時間軸を書き込む */
			fprintf(handle_ctl.csv, ",%llu\n", handle->flat_records[i]);
		}
	}

	fprintf(handle_ctl.csv, "\n\n");
}



/*
	unified形式で結果を出力する関数
*/
static void flushRecordsByList(void)
{
	int i = 0, j;
	MHANDLE *curr = NULL;
	struct ListController *lctl = &handle_ctl.list_ctl;
	u64 *unified_records[lctl->length];

	/* unified_recordsにアドレスを代入する */
	for(curr = lctl->ulist_top; curr; curr = curr->next){
		unified_records[i] = curr->flat_records;
		i++;
	}

	fprintf(handle_ctl.csv, "Listed event[Unified]\n");

	for(curr = lctl->ulist_top; curr; curr = curr->next){
		fprintf(handle_ctl.csv, ",%s", curr->tag);
	}

	fprintf(handle_ctl.csv, "\n");

	for(i = 0; i < counter; i++){
		fprintf(handle_ctl.csv, "%d", i);	/* 時間軸を書き込む */

		for(j = 0; j < lctl->length; j++){	/* 値を書き込む */
			fprintf(handle_ctl.csv, ",%llu", unified_records[j][i]);
		}

		fprintf(handle_ctl.csv, "\n");
	}

	fprintf(handle_ctl.csv, "\n\n");
}


/***
	ここから公開関数
				***/

/*
	引数のハンドルをリストの末尾につなぐ関数
	@handle つなぐハンドルのアドレス
*/
void addUnifiedList(MHANDLE *handle)
{
	if(handle->scope == MSR_SCOPE_PACKAGE && handle->active == true){
		;
	}
	else{
		return;
	}

	/* リストをつなぐ */
	if(handle_ctl.list_ctl.ulist_top == NULL){
		handle_ctl.list_ctl.ulist_top = handle;		/* 1番始目の要素を記録 */
		handle_ctl.list_ctl.ulist_current = handle;
	}
	else{
		handle_ctl.list_ctl.ulist_current->next = handle;	/* 2番目以降の要素 */
		handle_ctl.list_ctl.ulist_current = handle_ctl.list_ctl.ulist_current->next;
	}

	handle_ctl.list_ctl.length++;
}

/*
	設定してあるハンドルすべてからread_msr()を発行する関数
	return 失敗:-1 成功:0
*/
int getEventValues(void)
{
	int i, skip = 0;

	if(counter >= handle_ctl.max_records){
		return -1;
	}

	for(i = 0; i < handle_ctl.nr_handles; i++){
		if(getEventValue(&handle_ctl.handles[i]) == -1){
			skip = 1;
		}
	}

	if(skip){
		;
	}
	else{
		counter++;
	}

	return 0;
}


/*
	flush_handle_records()のラッパ関数
*/
void flushHandleRecords(void)
{
	int i;

	for(i = 0; i < handle_ctl.nr_handles; i++){

		if(handle_ctl.handles[i].tag == NULL){
			;
		}
		else{
			flushRecordsByHandle(&handle_ctl.handles[i]);
		}
	}

	if(handle_ctl.list_ctl.length > 0){	/* リストにハンドルが登録されていれば */
		flushRecordsByList();
	}
}

/*
	IA32_PERF_GLOBAL_CTRLを設定する関数 使えるPMCはすべて有功にする
	return: 有効にしたPMCの数
*/
int set_IA32_PERF_GLOBAL_CTRL(void)
{
	int i, nr_pmcs, nr_cpus = sysconf(_SC_NPROCESSORS_CONF);
	u64 reg = 0;

	/* CPUID.EAXから必要なレポートを取得 */
	nr_pmcs = ia32_nr_pmcs();

	//printf("nr_pmc:%d\n", nr_pmcs);	/* debug */

	/* PMCのenableビットを立てる */
	for(i = 0; i < nr_pmcs; i++){
		set_nbit64(&reg, i);
	}

	/* 各CPUに対して書き込み */
	for(i = 0; i < nr_cpus; i++){
		putMsrValue(i, IA32_PERF_GLOBAL_CTRL, reg);
		printf("%llu, ", getMsrValue(i, IA32_PERF_GLOBAL_CTRL));
	}

	return nr_pmcs;
}

/*
	MSR_UNCORE_PERF_GLOBAL_CTRLを設定する関数 使えるUNCORE_PMCはすべて有功にする
	return: 有効にしたPMCの数
*/
int set_UNC_PERF_GLOBAL_CTRL(void)
{
	u64 reg = 0;
	int i;

	for(i = 0; i < 8; i++){
		set_nbit64(&reg, i);
	}

	putMsrValue(0, MSR_UNCORE_PERF_GLOBAL_CTRL, reg);	/* 0番のCPUで実行 */

	return 8; 	/* 2011.8.9時点でuncoreのPMCは8つで決め打ち */
}

/***
	PERFEVTSELを設定する関数 -簡易版-
***/


/*
	IA32_PERFEVTSELを設定する関数
	@sel PERFEVTSELのアドレス
	@umask UMASKの値
	@event Event Numの値
*/
void set_IA32_PERFEVTSEL_handy(unsigned int sel, unsigned int umask, unsigned int event)
{
	int i, nr_cpus = sysconf(_SC_NPROCESSORS_CONF);

	u64 reg = 0;

	reg |= umask;
	reg = reg << 8;
	reg |= event;

	set_nbit64(&reg, 16);	/* USER bit */
	set_nbit64(&reg, 17);	/* OS bit */

	set_nbit64(&reg, 22);	/* EN bit */

	for(i = 0; i < nr_cpus; i++){	/* IA32_PERFEVTSELに書き込み */
		putMsrValue(i, sel, reg);
	}
}

/*
	MSR_UNMSR_SCOPE_CORE_PerfEvtSelを設定する関数
	@sel MSR_UNMSR_SCOPE_CORE_PerfEvtSelのアドレス
	@umask UMASKの値
	@event Event Numの値
*/
void set_UNC_PERFEVTSEL_handy(unsigned int sel, unsigned int umask, unsigned int event)
{
	u64 reg = 0;

	reg |= umask;
	reg = reg << 8;
	reg |= event;

	set_nbit64(&reg, 22);	/* EN bit */

	putMsrValue(0, sel, reg);	/* UNMSR_SCOPE_COREイベントのMSRのscopeはMSR_SCOPE_PACKAGEなので0番のCPUで実行するだけ */
}

/***
	PERFEVTSELを設定する関数 -詳細版-
***/

/*
	IA32_PERFEVTSELを設定する関数
	@addr PERFEVTSELのアドレス
	@reg IA32_PERFEVTSELxのアドレス これは呼出側で設定されていることを想定
*/
void set_IA32_PERFEVTSEL(unsigned int addr, union IA32_PERFEVTSELx *reg)
{
	int i, ver, nr_cpus = sysconf(_SC_NPROCESSORS_CONF);

	ver = perfevtsel_version_id();

	printf("version:%d", ver);

	if(ver == 2){
		reg->split.ANY = 0;
	}

	for(i = 0; i < nr_cpus; i++){	/* IA32_PERFEVTSELに書き込み */
		putMsrValue(i, addr, reg->full);
	}
}

/*
	UNCORE_PERFEVTSELを設定する関数
	@addr PERFEVTSELのアドレス
	@reg UNCORE_PERFEVTSELxのアドレス これは呼出側で設定されていることを想定
*/
void set_UNC_PERFEVTSEL(unsigned int addr, union UNCORE_PERFEVTSELx *reg)
{
	putMsrValue(0, addr, reg->full);	/* UNCOREイベントのMSRのscopeはPACKAGEなので0番のCPUで実行するだけ */
}

/***
	PERFEVTSELのための設定関数　ここまで
***/


/*
	ハンドルにパラメータを設定する関数
	@tag: ハンドルを識別する文字列
	@scop: MSRのスコープ
	@addr: MSRのアドレス
	@pre_closure: MSRから得たデータをバッファに格納する前に処理する関数のアドレス
	return 失敗:-1 成功:0
*/
int activateHandle(MHANDLE *handle, const char *tag, int scope,
		unsigned int addr, int (*pre_closure)(int handle_id, u64 *cpu_val))
{
	strncpy(handle->tag, tag, STR_MAX_TAG);

	handle->next = NULL;
	handle->scope = scope;
	handle->addr = addr;
	handle->pre_closure = pre_closure;

	if(handle->scope == MSR_SCOPE_THREAD || handle->scope == MSR_SCOPE_CORE){
		handle->flat_records = calloc(handle_ctl.max_records * handle_ctl.nr_cpus, sizeof(u64));
		//printf("handle_ctl.max_records:%d, handle_ctl.nr_cpus:%d\n", handle_ctl.max_records, handle_ctl.nr_cpus);

		if(handle->flat_records == NULL){
			//puts("calloc err");
			return -1;
		}
	}
	else{
		handle->flat_records = calloc(handle_ctl.max_records, sizeof(u64));

		if(handle->flat_records == NULL){
			return -1;
		}
	}

	if(handle->flat_records){
		handle->active = true;
	}
	else{
		handle->active = false;
	}

	return 0;
}

/*
	ハンドルを無効化する関数
	@handle 無効化するハンドル
*/
void deactivateHandle(MHANDLE *handle)
{
	free(handle->flat_records);
	handle->active = false;

	handle_ctl.nr_handles--;
}

/*
	handle_ctlを設定する関数
	@max_records 何回計測するか
	@nr_handles いくつハンドルを使うか（使用するPMCの数）
	return 配列のアドレス
*/
MHANDLE *initHandleController(FILE *output, int max_records, int nr_handles)
{
	if(output == NULL){
		char path[24];
		sprintf(path, "records_%d.csv", (int)time(NULL));

		if((handle_ctl.csv = fopen(path, "w")) == NULL){
			return NULL;
		}
	}
	else{
		handle_ctl.csv = output;
	}

	/* list_ctlの初期化 */
	handle_ctl.list_ctl.ulist_top = NULL;
	handle_ctl.list_ctl.ulist_current = NULL;
	handle_ctl.list_ctl.length = 0;

	/* メンバ変数の初期化 */
	handle_ctl.nr_handles = nr_handles;
	handle_ctl.nr_cpus = sysconf(_SC_NPROCESSORS_CONF);

	handle_ctl.handles = (MHANDLE *)calloc(nr_handles, sizeof(MHANDLE));

	//printf("nr_handles:%d, sizeof(MHANDLE):%d\n", nr_handles, sizeof(MHANDLE));

	if(handle_ctl.handles == NULL){
		return NULL;
	}

	handle_ctl.max_records = max_records;

	return handle_ctl.handles;
}

/*
	ライブラリの後始末のための関数
*/
void termHandleController(void)
{
	fclose(handle_ctl.csv);
	free(handle_ctl.handles);
}

/*
 pthreadのcleanup関数で使用されることを想定している
*/
void termHandleController_cleanup(void *arg)
{
	int i;

	flushHandleRecords();	/* CSVに書き出す */

	for(i = 0; i < handle_ctl.nr_handles; i++){
		deactivateHandle(&handle_ctl.handles[i]);
	}

	fclose(handle_ctl.csv);

	free(handle_ctl.handles);
}


