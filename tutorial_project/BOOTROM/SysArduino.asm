;----------------------------------------------------------------------
;			 Arduino通信システム
;
;----------------------------------------------------------------------

; ESP32 バースト転送用ワーク

BURST_PCNT	   EQU TMP_SV0		; 圧縮カウント
BURST_DCNT	   EQU TMP_SV1		; データカウント
BURST_DATA	   EQU TMP_SV2		; 最終読み出しデータ
BURST_IDX	   EQU TMP_SV3		; データインデックス
BURST_FLIP	   EQU TMP_SV4		; 同期制御
BURST_TMOUT	   EQU TMP_SV5		; タイムアウト判定用
BURST_COUNT	   EQU TMP_COUNT	; データサイズカウント用

;-------------------------------------------------------------

SYSCOM_INIT   = $F2		; 初期化コマンド
SYSCOM_ESPCOM = $F3		; ESP32 システムコマンド
SYSCOM_MP3_A  = $F4		; MP3 制御コマンドA
SYSCOM_MP3_B  = $F5		; MP3 制御コマンドB
SYSCOM_SAVE   = $F6		; マイクロSDにバックアップ
SYSCOM_WIFI   = $F7		; WIFI通信制御

SYSCOM_RDATA  = $FA		; データ読み出し
SYSCOM_RBURST = $FB		; データ読み出し(バースト転送モード)

STAT_INIT  = $00		;
STAT_WAIT  = $40		; 初期状態
STAT_WAIT1 = $41		; コマンド処理中
STAT_WAIT2 = $42		; コマンド処理中 ダウンロード中など長時間かかるコマンド時 STAT_WAITとトルグ

STAT_NOEXA  = $50		; 拡張アダプター無し

STAT_BOOT  = $80		; 起動中
STAT_SEALED = $81		; 封印中（封印解除コマンド以外は無視する）
STAT_OTASD = $82		; SDカードからOTAアップデート
STAT_WIFI_CONNECT = $83	; WIFI接続開始
STAT_WIFI_CON_OK = $84	; WIFI接続開始成功
STAT_WIFI_CGI = $85		; CGI実行
STAT_WIFI_CGI_OK = $86	; CGI実行OK
STAT_UPDATE_CNT = $87	; UPDATE継続中
STAT_UPDATE_OK  = $88	; UPDATEダウンロード終了
STAT_UPDATE_END = $89	; UPDATE終了

STAT_WIFI_VS    = $90	; 対戦マッチング中
STAT_WIFI_VS_OK = $91	; 対戦マッチング成立
STAT_WIFI_VS_NG = $92	; 対戦マッチング不成立
STAT_WIFI_WAIT  = $93	; WIFIコマンド処理中

; エラーコード
ERR_CODE_MIN = $C0
ERR_SD_TALKING = $C1
ERR_SD_ATTACH = $C2
ERR_LOAD_INIFILE = $C3
ERR_WIFI_CONNECT = $C4
ERR_WIFI_CGI = $C5

ERR_WIFI_MATVS = $C6
ERR_WIFI_DOWNLD = $C7

; WIFIコマンド
COM_WIFI_SCORE = $00		; スコアアップロード
COM_WIFI_MANAGER = $F0		; WIFI設定マネージャー
COM_WIFI_UPDATE  = $F1		; WIFIアップデーター
COM_WIFI_LSTSSID = $F2		; WIFI SSID LIST取得
COM_WIFI_TSTSSID = $F3		; WIFI SSID LIST内番号を指定して接続テスト
COM_WIFI_GETNAME = $F4		; WIFI NICNEAME 取得
COM_WIFI_SETNAME = $F5		; WIFI NICNEAME 設定


COM_WIFI_SYNC    = $FC		; ゲームデータデータ同期
COM_WIFI_MATCHING_VS = $FD	; 対戦マッチング
COM_WIFI_CONNECT = $FE		; WIFI接続
COM_WIFI_DISCONNECT = $FF	; WIFI切断


;----------------------------
; セーブ・ロード暗号化キー
;----------------------------
SVDT_KEY0	EQU $3C
SVDT_KEY1	EQU $A1
SVDT_KEY2	EQU $BB
SVDT_KEY3	EQU $7F




;-------------------------------
; WIFI設定マネージャー
;-------------------------------
;WIFI_MANAGER:
;	lda  #COM_WIFI_MANAGER	; WIFI設定マネージャー
;	bne  WIFI_COMMAND_82


;-------------------------------
; WIFI SSID LIST取得
;-------------------------------
WIFI_LSTSSID:
	lda  #COM_WIFI_LSTSSID		; WIFI SSID LIST取得
	bne  WIFI_COMMAND_82

;-------------------------------
; WIFI接続
;-------------------------------
WIFI_CONNECT:
	lda  #COM_WIFI_CONNECT	; WIFI接続
	bne  WIFI_COMMAND_82

;-------------------------------
; WIFI切断
;-------------------------------
WIFI_DISCONNECT:
	lda  #COM_WIFI_DISCONNECT	; WIFI切断
;	bne  WIFI_COMMAND_82

;-------------------------------
; WIFIコマンド共通処理
;-------------------------------
WIFI_COMMAND_82:
	pha
	lda  #SYSCOM_WIFI
	ldy  #$82			; 送信開始
	jsr  WB_Arduino_start
	pla
	jsr  WB_Arduino_send
	jmp  WB_Arduino_end


;-------------------------------
; スコアアップロード
;-------------------------------
WIFI_SCORE:
	lda  #SYSCOM_WIFI
	ldy  #$85			; 送信開始
	jsr  WB_Arduino_start
	lda  #COM_WIFI_SCORE	; スコア
	jsr  WB_Arduino_send
	lda  HISCORES+2
	jsr  WB_Arduino_send
	lda  HISCORES+1
	jsr  WB_Arduino_send
	lda  HISCORES+0
	jsr  WB_Arduino_send
	jmp  WB_Arduino_end


;-------------------------------
; WRAMのバンク切り替え
;  Areg = BANK値（0-3）
;-------------------------------
setWRAM_BANK:
	sta  <TMP_SVA
	jsr  setESPCOM
	ldy  #200
.loop
	dey
	beq  .timeout
	jsr  RSTAT_Arduino
	cmp  <TMP_SVA
	bne  .loop
	clc
	rts
.timeout
	sec
	rts


setESPCOM:
	pha
	lda  #SYSCOM_ESPCOM
	ldy  #$82			; 送信開始
	jsr  WB_Arduino_start
	pla
	jsr  WB_Arduino_send
	jmp  WB_Arduino_end



;-------------------------------
; MPプレーヤーボリューム設定
;  Areg = VOL値（0-21） 
;-------------------------------
setVol_MP3:
	pha
	lda  #SYSCOM_MP3_A
	ldy  #$83			; 送信開始
	jsr  WB_Arduino_start
	lda  #$FE			; ボリュームセット
	jsr  WB_Arduino_send
	pla
	jsr  WB_Arduino_send
	jmp  WB_Arduino_end


;-------------------------------
; MPプレーヤー再生
;  Areg = 再生MP3番号
;  MP3_BANK = 再生バンク番号
;-------------------------------
play_MP3:
	pha
	ldx  #SYSCOM_MP3_A
	and  #$80
	beq  .noloop
	ldx  #SYSCOM_MP3_B
.noloop
	txa
	ldy  #$83			; 送信開始
	jsr  WB_Arduino_start
	lda  MP3_BANK
	jsr  WB_Arduino_send
	pla
	and  #$7F
	jsr  WB_Arduino_send
	jmp  WB_Arduino_end


stop_MP3:
	lda  #SYSCOM_MP3_A
	ldy  #$82			; 送信開始
	jsr  WB_Arduino_start
	lda  #$FF			; ストップコマンド
	jsr  WB_Arduino_send
	jmp  WB_Arduino_end


save_SAVEDATA:
	lda  #SYSCOM_SAVE
	ldy  #$88			; 送信開始
	jsr  WB_Arduino_start

	lda  #$FC			; バックアップデータヘッダー
	jsr  WB_Arduino_send

	lda  MP3_VOL		; マスターボリューム
	jsr  WB_Arduino_send

	lda  MP3_BANK		; BGMバンク
	jsr  WB_Arduino_send

	lda  HISCORES+2
	eor  HISCORES+1
	eor  HISCORES+0
	sta  <TMP_SVA

	lda  HISCORES+2
	eor  #SVDT_KEY0
	jsr  WB_Arduino_send

	lda  HISCORES+1
	eor  #SVDT_KEY1
	jsr  WB_Arduino_send

	lda  HISCORES+0
	eor  #SVDT_KEY2
	jsr  WB_Arduino_send

	lda  <TMP_SVA
	eor  #SVDT_KEY3
	jsr  WB_Arduino_send

	jmp  WB_Arduino_end


;-------------------------------------
;	 arduino 1バイト書き込み ※改造するとタイミング取れなくなる
;-------------------------------------

WB_Arduino:
	ldy  #$81		; 送信開始
	jsr  WB_Arduino_start

WB_Arduino_end:
	ldx  #0
	stx	 $5000
	rts



;-------------------------------------
;	 arduino 送信ソフトウェイト（要調整）
;-------------------------------------
WB_Arduino_wait:
	ldx  #200
;	ldx  #50
.wait2
	dex
	bne .wait2
	rts


;-------------------------------------
;	arduino 複数バイト書き込み
;
;	Areg = 送信コマンド
;	Yreg = $80+コマンドバイト数
;	SRC_ADR：送信コマンドアドレス
;-------------------------------------
WB_ArduinoMB:
	jsr  WB_Arduino_start
	tya
	and  #$7F
	sta	 <TMP_LOOP_CNT
	ldy  #0
.loop
	dec	 <TMP_LOOP_CNT
	beq  WB_Arduino_end
	lda  [SRC_ADR],y
	iny
	jsr  WB_Arduino_send
	beq  .loop

	jmp  WB_Arduino_end

;-------------------------------------
;	arduino データ送信開始
;
;	Areg = 送信コマンド
;	Yreg = $80+コマンドバイト数
;-------------------------------------
WB_Arduino_start:
	sty	 $5000
	jsr  WB_Arduino_wait

WB_Arduino_send:
	sta	 $5000
	jsr  WB_Arduino_wait
	lda	 $5000			; コマンド送信
	jmp  WB_Arduino_wait

;-------------------------------------
;	リードステータス
;-------------------------------------
RSTAT_Arduino:
	lda  $5000
	rts


;-------------------------------------
;	バーストモードバッファリード
;
;	SET_DATA_DST xxxxx	読み込みバッファアドレス
;
;-------------------------------------

ReadBuf_Burst:
	SET_DATA_DST HTTP_BUF
ReadBuf_Burst2:
	jsr  START_Burst
.loop
	jsr  RB_Burst
	php
	jsr  setDST_ADR_DATA
	plp
 	bcc  .loop			; 最終データまで読み出す
	rts

;-------------------------------------
;	バーストモードバッファリード：サイズヘッダ版
;
;	SET_DATA_DST xxxxx	読み込みバッファアドレス
;
;	ret  エラー時はキャリーフラグセット
;-------------------------------------

ReadBuf_BurstSiz:
	SET_DATA_DST HTTP_BUF
ReadBuf_BurstSiz2:
	jsr  START_Burst

	jsr  RB_Burst
	bcs  .error_end
	sta  <BURST_COUNT+1
	jsr  RB_Burst
	bcs  .error_end
	sta  <BURST_COUNT+0

.loop
	lda  <BURST_COUNT+0
	ora  <BURST_COUNT+1
	beq  .end
	decw  <BURST_COUNT
	jsr  RB_Burst
	php
	jsr  setDST_ADR_DATA
	plp
	bcc  .loop
.end
	lda  #0
	jsr  setDST_ADR_DATA	;データ終端として0を出力
.error_end
	rts


;-------------------------------------
;	バーストモードリード
;-------------------------------------
START_Burst:
	lda  #SYSCOM_RBURST
	jsr  WB_Arduino		; バースト転送モード開始
	sta  <BURST_DATA	; 開始時の$5000の読み出し結果を保存
	lda  #0
	sta  <BURST_PCNT
	sta  <BURST_DCNT
	sta  <BURST_IDX
	sta  <BURST_FLIP
	rts


RB_Burst:
	lda  <BURST_PCNT
	bne  .burst_pdt
	lda  <BURST_DCNT
	bne  .burst_ddt

	; コマンドバイト受信
	jsr  .waitChgR5000
	bcs  .end
	tay
	and  #$1F
	beq  .end
	tax
	tya
	and  #$20
	beq  .burst_dcom

	stx  <BURST_PCNT
	jsr  .waitChgR5000
	bcs  .end

.burst_pdt
	dec  <BURST_PCNT
	lda  <BURST_DATA
	clc
	rts

.end
	sec
	rts

.burst_dcom
	stx  <BURST_DCNT
.burst_ddt
	dec  <BURST_DCNT
	
.waitChgR5000
	lda  #0
	sta  <BURST_TMOUT
.waitChgR5000_loop
;	ldy  #100
;.wait_00
;	dey
;	bne  .wait_00
	dec  <BURST_TMOUT
	beq  .end
	ldy  $5000
	cpy  <BURST_DATA
	beq  .waitChgR5000_loop
	sty  <BURST_DATA

	; 受信出来たので同期信号応答
	lda  <BURST_FLIP
	eor  #$80
	sta  <BURST_FLIP
	sta  $5000
	tya
	clc
	rts

;-------------------------------------
;	拡張システム封印解除
;-------------------------------------
EXS_GAMEID:
	db "A000"

EXS_INIT:
	jsr  RSTAT_Arduino			; ダミーリード
	SET_DATA_SRC EXS_GAMEID
	ldy  #$85
	lda  #SYSCOM_INIT
	jmp  WB_ArduinoMB



;-------------------------------------
;	拡張システムリセット
;-------------------------------------
EXS_RESET:
	jsr  RSTAT_Arduino			; ダミーリード

	lda  #SYSCOM_INIT
	ldy  #$82		; 送信開始
	jsr  WB_Arduino_start

	lda  #0
	jsr  WB_Arduino_send
	jmp  WB_Arduino_end

