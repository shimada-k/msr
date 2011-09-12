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

/*
	TODO:
	1):handle同士で引き算、足し算をできるようにする
		msr.c内にsub_handle()とsum_handle()を定義
	2):1つのハンドルにつき値が一つのイベントの場合、統合形式で表示できるようにする
		いくつかのハンドルを1つのグラフにまとめて出力させる
	3):対称性を考慮し、ソースを整理する
		free_handle(MHANDLE *handle)を定義する
			alloc_handle()を削除する方向で!
		公開する関数とstaticな関数を分ける
		
*/

/* list管理用構造体 */
struct list_controller{
	int length;
	MHANDLE *ulist_top;
	MHANDLE *ulist_current;
};

/* 管理用構造体 */
struct handle_controller{
	int nr_cpus;
	int max_records;		/* 何回計測するか */
	FILE *csv;			/* レポートのCSVファイル */
	int nr_handles;		/* 使用するmsr_handleの数 */

	//MHANDLE *ulist_top, *ulist_current;	/* 統合形式で出力するハンドルの一番最初とカレントのイベント */

	struct list_controller list_ctl;	/* 統合形式で出力するためのリスト管理用データ */
	MHANDLE *handles;
};

static unsigned int counter;	/* 秒数カウンタ */
static struct handle_controller mh_ctl;


/***
	ここからライブラリ内部関数
					***/

/*
	msr.koで作られるデバイスファイルにアクセスしてMSRから値を読み込む
	@cpu CPU番号
	@offset レジスタのアドレス
*/
static u64 get_msr(int cpu, off_t offset)
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
static void put_msr(int cpu, off_t offset, u64 val)
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
*/
static bool read_msr_by_handle(MHANDLE *handle)
{
	int handle_id = handle - mh_ctl.handles;

	if(handle->active == false){	/* activateされてなかったら終了 */
		fprintf(stderr, "this handle is not active. skiped.\n");
		return false;
	}

	if(handle->scope == MSR_SCOPE_THREAD || handle->scope == MSR_SCOPE_CORE){

		int i;
		u64 val[mh_ctl.nr_cpus];
		u64 (*nested_records)[mh_ctl.nr_cpus] = (u64 (*)[mh_ctl.nr_cpus])handle->flat_records;

		for(i = 0; i < mh_ctl.nr_cpus; i++){
			val[i] = get_msr(i, (off_t)handle->addr);
		}

		if(handle->pre_closure){	/* 前回のデータとの差分をとって、今回のデータをクロージャ内に置いてくる */
			if(handle->pre_closure(handle_id, val) == false){
				return false;
			}
		}

		/* ハンドル内バッファに格納 */
		for(i = 0; i < mh_ctl.nr_cpus; i++){
			nested_records[counter][i] = val[i];
		}

	}
	else{
		u64 val;

		val = get_msr(0, (off_t)handle->addr);	/* 0番のCPUで実行 */

		//printf("%llu\n", val);

		if(handle->pre_closure){	/* 前回のデータとの差分をとって、今回のデータをクロージャ内に置いてくる */
			if(handle->pre_closure(handle_id, &val) == false){
				return false;
			}
		}

		printf("%llu\n", val);

		/* ハンドル内バッファに格納 */
		handle->flat_records[counter] = val;
	}

	return true;
}

/*
	ハンドル内バッファに溜まったデータを書き出す関数
	@handle 書き出す対象のハンドル
*/
static void flush_records_by_handle(MHANDLE *handle)
{
	int i, j;

	fprintf(mh_ctl.csv, "%s\n", handle->tag);

	if(handle->scope == MSR_SCOPE_THREAD || handle->scope == MSR_SCOPE_CORE){
		for(i = 0; i < mh_ctl.nr_cpus; i++){
			fprintf(mh_ctl.csv, ",CPU%d", i);
		}
	}
	else{
		fprintf(mh_ctl.csv, ",value");
	}

	fprintf(mh_ctl.csv, "\n");

	if(handle->scope == MSR_SCOPE_THREAD || handle->scope == MSR_SCOPE_CORE){
		/* 1次元配列を2次元配列にキャスト */
		u64 (*nested_records)[mh_ctl.nr_cpus] = (u64 (*)[mh_ctl.nr_cpus])handle->flat_records;

		printf("%llu,%llu\n", nested_records[0][0], nested_records[0][1]);

		for(i = 0; i < counter; i++){
			fprintf(mh_ctl.csv, "%d", i);	/* 時間軸を書き込む */

			for(j = 0; j < mh_ctl.nr_cpus; j++){
				fprintf(mh_ctl.csv, ",%llu", nested_records[i][j]);	/* 値を書き込む */
			}
			fprintf(mh_ctl.csv, "\n");
		}
	}
	else{
		for(i = 0; i < counter; i++){
			fprintf(mh_ctl.csv, "%d", i);	/* 時間軸を書き込む */
			fprintf(mh_ctl.csv, ",%llu\n", handle->flat_records[i]);
		}
	}

	fprintf(mh_ctl.csv, "\n\n");
}



/*
	unified形式で結果を出力する関数
*/
static void flush_records_by_list(void)
{
	int i = 0, j;
	MHANDLE *curr = NULL;
	u64 *unified_records[mh_ctl.list_ctl.length];

	/* unified_recordsにアドレスを代入する */
	for(curr = mh_ctl.list_ctl.ulist_top; curr; curr = curr->next){
		unified_records[i] = curr->flat_records;
		i++;
	}

	fprintf(mh_ctl.csv, "Listed event[Unified]\n");

	for(curr = mh_ctl.list_ctl.ulist_top; curr; curr = curr->next){
		fprintf(mh_ctl.csv, ",%s", curr->tag);
	}

	fprintf(mh_ctl.csv, "\n");

	for(i = 0; i < counter; i++){
		fprintf(mh_ctl.csv, "%d", i);	/* 時間軸を書き込む */

		for(j = 0; j < mh_ctl.list_ctl.length; j++){	/* 値を書き込む */
			fprintf(mh_ctl.csv, ",%llu", unified_records[j][i]);
		}

		fprintf(mh_ctl.csv, "\n");
	}

	fprintf(mh_ctl.csv, "\n\n");
}


/***
	ここから公開関数
				***/

/*
	引数のハンドルをリストの末尾につなぐ関数
	@handle つなぐハンドルのアドレス
*/
void add_unified_list(MHANDLE *handle)
{
	if(handle->scope == MSR_SCOPE_PACKAGE && handle->active == true){
		;
	}
	else{
		return;
	}

	/* リストをつなぐ */
	if(mh_ctl.list_ctl.ulist_top == NULL){
		mh_ctl.list_ctl.ulist_top = handle;		/* 1番始目の要素を記録 */
		mh_ctl.list_ctl.ulist_current = handle;
	}
	else{
		mh_ctl.list_ctl.ulist_current->next = handle;	/* 2番目以降の要素 */
		mh_ctl.list_ctl.ulist_current = mh_ctl.list_ctl.ulist_current->next;
	}

	mh_ctl.list_ctl.length++;
}

/*
	設定してあるハンドルすべてからread_msr()を発行する関数
*/
bool read_msrs(void)
{
	int i, skip = 0;

	if(counter >= mh_ctl.max_records){
		return false;
	}

	for(i = 0; i < mh_ctl.nr_handles; i++){
		if(read_msr_by_handle(&mh_ctl.handles[i]) == false){
			skip = 1;
		}
	}

	if(skip){
		;
	}
	else{
		counter++;
	}

	return true;
}


/*
	flush_handle_records()のラッパ関数
*/
void flush_handle_records(void)
{
	int i;

	for(i = 0; i < mh_ctl.nr_handles; i++){

		if(mh_ctl.handles[i].tag == NULL){
			;
		}
		else{
			flush_records_by_handle(&mh_ctl.handles[i]);
		}
	}

	if(mh_ctl.list_ctl.ulist_top){	/* リストにハンドルが登録されていれば */
		flush_records_by_list();
	}
}

/*
	IA32_PERF_GLOBAL_CTRLを設定する関数 使えるPMCはすべて有功にする
	return: 有効にしたPMCの数
*/
int setup_PERF_GLOBAL_CTRL(void)
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
		put_msr(i, IA32_PERF_GLOBAL_CTRL, reg);
		printf("%llu, ", get_msr(i, IA32_PERF_GLOBAL_CTRL));
	}

	return nr_pmcs;
}

/*
	MSR_UNCORE_PERF_GLOBAL_CTRLを設定する関数 使えるUNCORE_PMCはすべて有功にする
	return: 有効にしたPMCの数
*/
int setup_UNCORE_PERF_GLOBAL_CTRL(void)
{
	u64 reg = 0;
	int i;

	for(i = 0; i < 8; i++){
		set_nbit64(&reg, i);
	}

	put_msr(0, MSR_UNCORE_PERF_GLOBAL_CTRL, reg);	/* 0番のCPUで実行 */

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
void setup_IA32_PERFEVTSEL_quickly(unsigned int sel, unsigned int umask, unsigned int event)
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
		put_msr(i, sel, reg);
	}
}

/*
	MSR_UNMSR_SCOPE_CORE_PerfEvtSelを設定する関数
	@sel MSR_UNMSR_SCOPE_CORE_PerfEvtSelのアドレス
	@umask UMASKの値
	@event Event Numの値
*/
void setup_UNCORE_PERFEVTSEL_quickly(unsigned int sel, unsigned int umask, unsigned int event)
{
	u64 reg = 0;

	reg |= umask;
	reg = reg << 8;
	reg |= event;

	set_nbit64(&reg, 22);	/* EN bit */

	put_msr(0, sel, reg);	/* UNMSR_SCOPE_COREイベントのMSRのscopeはMSR_SCOPE_PACKAGEなので0番のCPUで実行するだけ */
}

/***
	PERFEVTSELを設定する関数 -詳細版-
***/

/*
	IA32_PERFEVTSELを設定する関数
	@addr PERFEVTSELのアドレス
	@reg IA32_PERFEVTSELxのアドレス これは呼出側で設定されていることを想定
*/
void setup_IA32_PERFEVTSEL(unsigned int addr, union IA32_PERFEVTSELx *reg)
{
	int i, ver, nr_cpus = sysconf(_SC_NPROCESSORS_CONF);

	ver = perfevtsel_version_id();

	printf("version:%d", ver);

	if(ver == 2){
		reg->split.ANY = 0;
	}

	for(i = 0; i < nr_cpus; i++){	/* IA32_PERFEVTSELに書き込み */
		put_msr(i, addr, reg->full);
	}
}

/*
	UNCORE_PERFEVTSELを設定する関数
	@addr PERFEVTSELのアドレス
	@reg UNCORE_PERFEVTSELxのアドレス これは呼出側で設定されていることを想定
*/
void setup_UNCORE_PERFEVTSEL(unsigned int addr, union UNCORE_PERFEVTSELx *reg)
{
	put_msr(0, addr, reg->full);	/* UNCOREイベントのMSRのscopeはPACKAGEなので0番のCPUで実行するだけ */
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
*/
bool activate_handle(MHANDLE *handle, const char *tag, int scope,
		unsigned int addr, bool (*pre_closure)(int handle_id, u64 *cpu_val))
{
	strncpy(handle->tag, tag, STR_MAX_TAG);

	handle->next = NULL;
	handle->scope = scope;
	handle->addr = addr;
	handle->pre_closure = pre_closure;

	if(handle->scope == MSR_SCOPE_THREAD || handle->scope == MSR_SCOPE_CORE){
		handle->flat_records = calloc(mh_ctl.max_records * mh_ctl.nr_cpus, sizeof(u64));
		//printf("mh_ctl.max_records:%d, mh_ctl.nr_cpus:%d\n", mh_ctl.max_records, mh_ctl.nr_cpus);

		if(handle->flat_records == NULL){
			//puts("calloc err");
			return false;
		}
	}
	else{
		handle->flat_records = calloc(mh_ctl.max_records, sizeof(u64));

		if(handle->flat_records == NULL){
			return false;
		}
	}

	if(handle->flat_records){
		handle->active = true;
	}
	else{
		handle->active = false;
	}

	return true;
}

/*
	ハンドルを無効化する関数
	@handle 無効化するハンドル
*/
void deactivate_handle(MHANDLE *handle)
{
	free(handle->flat_records);
	handle->active = false;

	mh_ctl.nr_handles--;
}

/*
	mh_ctlを設定する関数
	@max_records 何回計測するか
	@nr_handles いくつハンドルを使うか（使用するPMCの数）
	return 配列のアドレス
*/
MHANDLE *init_handle_controller(FILE *output, int max_records, int nr_handles)
{
	if(output == NULL){
		char path[24];
		sprintf(path, "records_%d.csv", (int)time(NULL));

		if((mh_ctl.csv = fopen(path, "w")) == NULL){
			return NULL;
		}
	}
	else{
		mh_ctl.csv = output;
	}

	/* list_ctlの初期化 */
	mh_ctl.list_ctl.ulist_top = NULL;
	mh_ctl.list_ctl.ulist_current = NULL;
	mh_ctl.list_ctl.length = 0;

	/* メンバ変数の初期化 */
	mh_ctl.nr_handles = nr_handles;
	mh_ctl.nr_cpus = sysconf(_SC_NPROCESSORS_CONF);

	mh_ctl.handles = (MHANDLE *)calloc(nr_handles, sizeof(MHANDLE));

	//printf("nr_handles:%d, sizeof(MHANDLE):%d\n", nr_handles, sizeof(MHANDLE));

	if(mh_ctl.handles == NULL){
		return NULL;
	}

	mh_ctl.max_records = max_records;

	return mh_ctl.handles;
}

/*
	ライブラリの後始末のための関数
*/
void term_handle_controller(void)
{
	fclose(mh_ctl.csv);
	free(mh_ctl.handles);
}

/*
 pMSR_SCOPE_THREADのcleanup関数で使用されることを想定している
*/
void term_handle_controller_cleanup(void *arg)
{
	int i;

	flush_handle_records();	/* CSVに書き出す */

	for(i = 0; i < mh_ctl.nr_handles; i++){
		deactivate_handle(&mh_ctl.handles[i]);
	}

	fclose(mh_ctl.csv);

	free(mh_ctl.handles);
}


