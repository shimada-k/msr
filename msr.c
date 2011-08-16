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

struct handle_controller{
	int nr_cpus;
	int max_records;	/* 何回計測するか */
	FILE *csv;		/* レポートのCSVファイル */
	int nr_handles;	/* 現在使用しているmsr_handleの数 */
	MHANDLE *handles;
};

/*
	後はエラー処理を施して、ネーミングを考えて完成
 */

static unsigned int counter;
struct handle_controller mh_ctl;


/*
	msr.koで作られるデバイスファイルにアクセスしてMSRから値を読み込む
	@cpu CPU番号
	@offset レジスタのアドレス
*/
u64 get_msr(int cpu, off_t offset)
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
void put_msr(int cpu, off_t offset, u64 val)
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
	ハンドルを確保する関数
*/
MHANDLE *alloc_handle(void)
{
	MHANDLE *handle;

	handle = &mh_ctl.handles[mh_ctl.nr_handles];
	mh_ctl.nr_handles++;

	return handle;
}

/*
	ハンドルでMSRからデータを読み、バッファに格納する関数
	@handle 読み込みに使用するハンドル
*/
bool read_msr_by_handle(MHANDLE *handle)
{
	int handle_id = handle - mh_ctl.handles;

	if(handle->active == false){	/* activateされてなかったら終了 */
		fprintf(stderr, "this handle is not active. skiped.\n");
		return;
	}

	if(handle->scope == thread || handle->scope == core){

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

		if(handle->pre_closure){	/* 前回のデータとの差分をとって、今回のデータをクロージャ内に置いてくる */
			if(handle->pre_closure(handle_id, &val) == false){
				return false;
			}
		}

		/* ハンドル内バッファに格納 */
		handle->flat_records[counter] = val;
	}

	return true;
}

/*
	設定してあるハンドルすべてからread_msr()を発行する関数
*/
bool read_msr(void)
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
	ハンドル内バッファに溜まったデータを書き出す関数
	@handle 書き出す対象のハンドル
*/
void flush_records_by_handle(MHANDLE *handle)
{
	int i, j;

	fprintf(mh_ctl.csv, "%s\n", handle->tag);

	if(handle->scope == thread || handle->scope == core){
		for(i = 0; i < mh_ctl.nr_cpus; i++){
			fprintf(mh_ctl.csv, ",CPU%d", i);
		}
	}
	else{
		fprintf(mh_ctl.csv, ",value");
	}

	fprintf(mh_ctl.csv, "\n");

	if(handle->scope == thread || handle->scope == core){
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
	flush_handle_records()のラッパ関数
*/
void flush_records(void)
{
	int i;

	for(i = 0; i < mh_ctl.nr_handles; i++){
		flush_records_by_handle(&mh_ctl.handles[i]);
	}
}

/*
	IA32_PERF_GLOBAL_CTRLを設定する関数 使えるPMCはすべて有功にする
	return: 有効にしたPMCの数
*/
int setup_PERF_GLOBAL_CTRL(void)
{
	int i, nr_pmcs;
	u64 reg = 0;

	/* CPUID.EAXから必要なレポートを取得 */
	nr_pmcs = ia32_nr_pmcs();

	printf("nr_pmc:%d\n", nr_pmcs);	/* debug */

	/* PMCのenableビットを立てる */
	for(i = 0; i < nr_pmcs; i++){
		set_nbit64(&reg, i);
	}

	/* 各CPUに対して書き込み */
	for(i = 0; i < mh_ctl.nr_cpus; i++){
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

	for(i = 0; i < mh_ctl.nr_cpus; i++){
		put_msr(i, MSR_UNCORE_PERF_GLOBAL_CTRL, reg);
	}

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
	int i;

	u64 reg = 0;

	reg |= umask;
	reg = reg << 8;
	reg |= event;

	set_nbit64(&reg, 16);	/* USER bit */
	set_nbit64(&reg, 17);	/* OS bit */

	set_nbit64(&reg, 22);	/* EN bit */

	for(i = 0; i < mh_ctl.nr_cpus; i++){	/* IA32_PERFEVTSELに書き込み */
		put_msr(i, sel, reg);
	}
}

/*
	MSR_UNCORE_PerfEvtSelを設定する関数
	@sel MSR_UNCORE_PerfEvtSelのアドレス
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

	put_msr(0, sel, reg);	/* UNCOREイベントのMSRのscopeはPackageなので0番のCPUで実行するだけ */
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
	int i, ver;

	ver = perfevtsel_version_id();

	printf("version:%d", ver);

	if(ver == 2){
		reg->split.ANY = 0;
	}

	for(i = 0; i < mh_ctl.nr_cpus; i++){	/* IA32_PERFEVTSELに書き込み */
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
	put_msr(0, addr, reg->full);	/* UNCOREイベントのMSRのscopeはPackageなので0番のCPUで実行するだけ */
}

/***
	PMCのためのレジスタ設定関数　ここまで
***/


/*
	ハンドルにパラメータを設定する関数
	@tag: ハンドルを識別する文字列
	@scop: MSRのスコープ
	@addr: MSRのアドレス
	@pre_closure: MSRから得たデータをバッファに格納する前に処理する関数のアドレス
*/
bool activate_handle(MHANDLE *handle, const char *tag, enum msr_scope scope,
		unsigned int addr, bool (*pre_closure)(int handle_id, u64 *cpu_val))
{
	snprintf(handle->tag, sizeof(char) * STR_MAX_TAG, "%s", tag);
	handle->scope = scope;
	handle->addr = addr;
	handle->pre_closure = pre_closure;

	if(handle->scope == thread || handle->scope == core){
		u64 val[mh_ctl.nr_cpus];
		int i;

		for(i = 0; i < mh_ctl.nr_cpus; i++){
			val[i] = get_msr(i, (off_t)handle->addr);
		}

		handle->flat_records = calloc(mh_ctl.max_records * mh_ctl.nr_cpus, sizeof(u64));

		if(handle->flat_records == NULL){
			return false;
		}
	}
	else{
		u64 val;

		val = get_msr(0, (off_t)handle->addr);

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
*/
bool init_handle_controller(int max_records, int nr_handles)
{
	char path[24];

	sprintf(path, "records_%d.csv", (int)time(NULL));

	mh_ctl.nr_cpus = sysconf(_SC_NPROCESSORS_CONF);
	mh_ctl.handles = calloc(nr_handles, sizeof(MHANDLE));
	mh_ctl.max_records = max_records;

	if((mh_ctl.csv = fopen(path, "w")) == NULL){
		return false;
	}

	return true;
}

/*
	ライブラリの後始末のための関数 pthreadのcleanup関数で使用されることを想定している
*/
void term_handle_controller(void *arg)
{
	int i;

	flush_records();	/* CSVに書き出す */

	for(i = 0; i < mh_ctl.nr_handles; i++){
		deactivate_handle(&mh_ctl.handles[i]);
	}
}


