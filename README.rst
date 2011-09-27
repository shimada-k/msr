msr -C言語用モデル固有レジスタ操作用フレームワーク-
=================================

モデル固有レジスタを扱う時に読み書き、結果出力を抽象化するフレームワークです。

MHANDLEという抽象化されたデータ構造を用いています。結果はCSVで出力されます。

内部でbitopsのAPIを読んでいます。'bitopsは<https://github.com/shimada-k/bitops>'にあります。
msr.koをカーネルに組み込む必要があります。

※最近のDebianの場合、デフォルトでmodprobe msrで組み込めます。

::

    gcc -c bitops.c
    gcc -c msr.c
    gcc -c YourProgramName.c
    gcc -o YourProgramName YourProgramName.o bitops.o msr.o


msr.cを用いるには以下の要領でレジスタを初期化し、読み込み、終了処理を行います。

プロトタイプ
-------------

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
int activateHandle(MHANDLE *handle, const char *tag, int scope, unsigned int addr, int (*preSlosure)(int handle_id, unsigned  long long *cpu_val));
void deactivateHandle(MHANDLE *handle);

