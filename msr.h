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
	unsigned  long long full;
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
	unsigned  long long full;
};

/* レジスタビットマップ用構造体 ここまで */

#define MSR_SCOPE_THREAD		0x00
#define MSR_SCOPE_CORE		0x01
#define MSR_SCOPE_PACKAGE		0x02

#define STR_MAX_TAG	64

struct MsrHandle{
	char tag[STR_MAX_TAG];
	int scope;
	unsigned int addr;		/* レジスタのアドレス(get_msr()の引数になる) */
	int active;			/* レジスタが使用可能になったら1になる */
	unsigned  long long *flat_records;		/* バッファ */

	struct MsrHandle *next;	/* 統合形式でCSVを出力するイベントのリスト */
	int (*preStore)(int handle_id, unsigned  long long *cpu_val);	/* バッファに格納する前に生データに対して行う処理 */
};

typedef struct MsrHandle MHANDLE;

/* GLOBAL_CTRLの設定関数 */
int set_IA32_PERF_GLOBAL_CTRL(void);
int set_UNC_PERF_GLOBAL_CTRL(void);

/* PERFEVTSELxの設定関数 */

/* 簡易版 */
void set_IA32_PERFEVTSEL_handy(unsigned int sel, unsigned int umask, unsigned int event);
void set_UNC_PERFEVTSEL_handy(unsigned int sel, unsigned int umask, unsigned int event);
/* 詳細版 */
void set_IA32_PERFEVTSEL(unsigned int addr, union IA32_PERFEVTSELx *reg);
void set_UNC_PERFEVTSEL(unsigned int addr, union UNCORE_PERFEVTSELx *reg);

/* 初期化、終了関数 */
MHANDLE *initHandleController(FILE *output, int max_records, int nr_handles);
void termHandleController(void);

/* 計測用関数 */
int getEventValues(void);

/* リストに追加する関数 */
void addUnifiedList(MHANDLE *handle);

/* CSV書き出し関数 */
void flushHandleRecords(void);

/* ハンドル有効化関数 */
int activateHandle(MHANDLE *handle, const char *tag, int scope,
		unsigned int addr, int (*preSlosure)(int handle_id, unsigned  long long *cpu_val));
void deactivateHandle(MHANDLE *handle);


