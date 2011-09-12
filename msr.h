#include <stdio.h>
#include <stdbool.h>
#include "bitops.h"

/*
	レジスタビットマップ用データ構造
	reserved領域は必ず0であること
*/

union IA32_PERFEVTSELx{
	struct {
		unsigned int EvtSel:8;
		unsigned int UMASK:8;
		unsigned int USER:1;
		unsigned int OS:1;
		unsigned int E:1;
		unsigned int PC:1;
		unsigned int INT:1;
		unsigned int ANY:1;	/* FACILITY-2だとここはreserved */
		unsigned int EN:1;
		unsigned int INV:1;
		unsigned int CounterMask:8;
		unsigned int RV2:32;	/* reserved */
	} split;
	u64 full;
};


union UNCORE_PERFEVTSELx {
	struct {
		unsigned int EvtSel:8;
		unsigned int UMASK:8;
		unsigned int RV1:1;	/* reserved */
		unsigned int OCC_CTR_RST:1;
		unsigned int E:1;
		unsigned int RV2:1;	/* reserved */
		unsigned int PMI:1;
		unsigned int RV3:1;	/* reserved */
		unsigned int EN:1;
		unsigned int INV:1;
		unsigned int CounterMask:8;
		unsigned int RV4:32;	/* reserved */
	} split;
	u64 full;
};

/* レジスタビットマップ用構造体 ここまで */

#define MSR_SCOPE_THREAD		0x00
#define MSR_SCOPE_CORE		0x01
#define MSR_SCOPE_PACKAGE		0x02

#define STR_MAX_TAG	64

struct msr_handle{
	char tag[STR_MAX_TAG];
	int scope;
	unsigned int addr;		/* レジスタのアドレス(get_msr()の引数になる) */
	bool active;			/* レジスタが使用可能になったらtrueになる */
	u64 *flat_records;		/* バッファ */

	struct msr_handle *next;	/* 統合形式でCSVを出力するイベントのリスト */
	bool (*pre_closure)(int handle_id, u64 *cpu_val);	/* バッファに格納する前に生データに対して行う処理 */
};

typedef struct msr_handle MHANDLE;

/* GLOBAL_CTRLの設定関数 */
int setup_PERF_GLOBAL_CTRL(void);
int setup_UNCORE_PERF_GLOBAL_CTRL(void);

/* PERFEVTSELxの設定関数 */

/* 簡易版 */
void setup_IA32_PERFEVTSEL_quickly(unsigned int sel, unsigned int umask, unsigned int event);
void setup_UNCORE_PERFEVTSEL_quickly(unsigned int sel, unsigned int umask, unsigned int event);
/* 詳細版 */
void setup_IA32_PERFEVTSEL(unsigned int addr, union IA32_PERFEVTSELx *reg);
void setup_UNCORE_PERFEVTSEL(unsigned int addr, union UNCORE_PERFEVTSELx *reg);

/* 初期化、終了関数 */
MHANDLE *init_handle_controller(FILE *output, int max_records, int nr_handles);
void term_handle_controller(void);

/* 計測用関数 */
bool read_msrs(void);

void add_unified_list(MHANDLE *handle);

/* CSV書き出し関数 */
void flush_handle_records(void);

/* ハンドル有効化関数 */
bool activate_handle(MHANDLE *handle, const char *tag, int scope,
		unsigned int addr, bool (*pre_closure)(int handle_id, u64 *cpu_val));
void deactivate_handle(MHANDLE *handle);


