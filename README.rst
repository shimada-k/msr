msr -C言語用モデル固有レジスタ操作用フレームワーク-
=================================

モデル固有レジスタを扱う時に読み書き、結果出力を抽象化するフレームワークです。

MHANDLEという抽象化されたデータ構造を用いています。結果はCSVで出力されます。

内部でbitopsのAPIを読んでいます。bitopsはhttps://github.com/shimada-k/bitopsにあります。
msr.koをカーネルに組み込む必要があります。

※最近のDebianの場合、デフォルトでmodprobe msrで組み込めます。

::
    gcc -c bitops.c
    gcc -c msr.c
    gcc -c YourProgramName.c
    gcc -o YourProgramName YourProgramName.o bitops.o msr.o

msr.cでは以下のAPIを提供しています。

.. list-table:: API一覧
   :header-rows: 1
   :stub-columns: 1

   * - ビット幅
     - 8ビット
     - 32ビット
     - 64ビット
   * - 指定された桁のビットの値（0 or 1）を調べる関数
     - pick_nbit8
     - pick_nbit32
     - pick_nbit64
   * - 指定された桁のビットを1にする関数
     - set_nbit8
     - set_nbit32
     - set_nbit64
   * - 指定された桁のビットを0にする関数
     - clr_nbit8
     - clr_nbit32
     - clr_nbit64
   * - 指定された桁のビットを反転する関数
     - rotate_nbit8
     - rotate_nbit32
     - rotate_nbit64
   * - 最初に1になっているビットの桁数を返す関数
     - find_first_setbit8
     - find_first_setbit32
     - find_first_setbit64
   * - 指定された桁の次に1になっているビットの桁数を返す関数
     - clr_nbit8
     - clr_nbit32
     - clr_nbit64
   * - 2進数で標準出力に値を出力する関数
     - print_binary8
     - print_binary32
     - print_binary64

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
int activateHandle(MHANDLE *handle, const char *tag, int scope,
		unsigned int addr, int (*preSlosure)(int handle_id, unsigned  long long *cpu_val));
void deactivateHandle(MHANDLE *handle);

