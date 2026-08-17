;/// @file SysArduino.asm
;/// @brief Legacy FC-EXA / ESP32 side channel at `$5000`.
;/// @ingroup bootrom
;///
;/// @warning **Vestigial on FC PICO.** This file talks to an expansion adapter
;///          that an FC PICO cartridge does not have. It is assembled but
;///          unreachable except for @ref setWRAM_BANK, which `SysNES.asm` still
;///          calls. Retained because the boot probe references its status codes.
;///
;/// Documented here so that nobody mistakes it for part of the cartridge
;/// protocol -- that lives in `SysPico.asm`. @see @ref protocol, @ref conventions
;----------------------------------------------------------------------
;			 Arduino通信システム
;
;----------------------------------------------------------------------

; ESP32 バースト転送用ワーク

BURST_PCNT	   EQU TMP_SV0   ;///< Burst transfer: page counter. ; 圧縮カウント
BURST_DCNT	   EQU TMP_SV1   ;///< Burst transfer: byte counter. ; データカウント
BURST_DATA	   EQU TMP_SV2   ;///< Burst transfer: current byte. ; 最終読み出しデータ
BURST_IDX	   EQU TMP_SV3   ;///< Burst transfer: buffer index. ; データインデックス
BURST_FLIP	   EQU TMP_SV4   ;///< Burst transfer: toggling acknowledgement flag. ; 同期制御
BURST_TMOUT	   EQU TMP_SV5   ;///< Burst transfer: timeout counter. ; タイムアウト判定用
BURST_COUNT	   EQU TMP_COUNT   ;///< Burst transfer: remaining blocks. ; データサイズカウント用

;-------------------------------------------------------------

SYSCOM_INIT   = $F2   ;///< Adapter command: initialise. ; 初期化コマンド
SYSCOM_ESPCOM = $F3   ;///< Adapter command: pass through to the ESP32. ; ESP32 システムコマンド
SYSCOM_MP3_A  = $F4   ;///< Adapter command: MP3 control, channel A. ; MP3 制御コマンドA
SYSCOM_MP3_B  = $F5   ;///< Adapter command: MP3 control, channel B. ; MP3 制御コマンドB
SYSCOM_SAVE   = $F6   ;///< Adapter command: write save data. ; マイクロSDにバックアップ
SYSCOM_WIFI   = $F7   ;///< Adapter command: Wi-Fi operation. ; WIFI通信制御

SYSCOM_RDATA  = $FA   ;///< Adapter command: read data. ; データ読み出し
SYSCOM_RBURST = $FB   ;///< Adapter command: burst read. ; データ読み出し(バースト転送モード)

STAT_INIT  = $00   ;///< Adapter status: initialising. 
STAT_WAIT  = $40   ;///< Adapter status: idle. ; 初期状態
STAT_WAIT1 = $41   ;///< Adapter status: idle, variant 1. ; コマンド処理中
STAT_WAIT2 = $42   ;///< Adapter status: idle, variant 2. ; コマンド処理中 ダウンロード中など長時間かかるコマンド時 STAT_WAITとトルグ

STAT_NOEXA  = $50   ;///< Adapter status: no adapter present. @note The value an FC PICO cartridge produces. ; 拡張アダプター無し

STAT_BOOT  = $80   ;///< Adapter status: booting. ; 起動中
STAT_SEALED = $81   ;///< Adapter status: sealed. ; 封印中（封印解除コマンド以外は無視する）
STAT_OTASD = $82   ;///< Adapter status: over-the-air update from SD. ; SDカードからOTAアップデート
STAT_WIFI_CONNECT = $83   ;///< Adapter status: connecting to Wi-Fi. ; WIFI接続開始
STAT_WIFI_CON_OK = $84   ;///< Adapter status: Wi-Fi connected. ; WIFI接続開始成功
STAT_WIFI_CGI = $85   ;///< Adapter status: CGI request in flight. ; CGI実行
STAT_WIFI_CGI_OK = $86   ;///< Adapter status: CGI request complete. ; CGI実行OK
STAT_UPDATE_CNT = $87   ;///< Adapter status: update progress. ; UPDATE継続中
STAT_UPDATE_OK  = $88   ;///< Adapter status: update succeeded. ; UPDATEダウンロード終了
STAT_UPDATE_END = $89   ;///< Adapter status: update finished. ; UPDATE終了

STAT_WIFI_VS    = $90   ;///< Adapter status: versus matchmaking. ; 対戦マッチング中
STAT_WIFI_VS_OK = $91   ;///< Adapter status: match found. ; 対戦マッチング成立
STAT_WIFI_VS_NG = $92   ;///< Adapter status: matchmaking failed. ; 対戦マッチング不成立
STAT_WIFI_WAIT  = $93   ;///< Adapter status: waiting on the network. ; WIFIコマンド処理中

; エラーコード
ERR_CODE_MIN = $C0   ;///< Lowest adapter error code; values at or above this are failures. 
ERR_SD_TALKING = $C1   ;///< Adapter error: SD card busy. 
ERR_SD_ATTACH = $C2   ;///< Adapter error: no SD card. 
ERR_LOAD_INIFILE = $C3   ;///< Adapter error: configuration file could not be read. 
ERR_WIFI_CONNECT = $C4   ;///< Adapter error: Wi-Fi connection failed. 
ERR_WIFI_CGI = $C5   ;///< Adapter error: CGI request failed. 

ERR_WIFI_MATVS = $C6   ;///< Adapter error: matchmaking failed. 
ERR_WIFI_DOWNLD = $C7   ;///< Adapter error: download failed. 

; WIFIコマンド
COM_WIFI_SCORE = $00   ;///< Wi-Fi command: submit a score. ; スコアアップロード
COM_WIFI_MANAGER = $F0   ;///< Wi-Fi command: open the manager. ; WIFI設定マネージャー
COM_WIFI_UPDATE  = $F1   ;///< Wi-Fi command: check for updates. ; WIFIアップデーター
COM_WIFI_LSTSSID = $F2   ;///< Wi-Fi command: list access points. ; WIFI SSID LIST取得
COM_WIFI_TSTSSID = $F3   ;///< Wi-Fi command: test an access point. ; WIFI SSID LIST内番号を指定して接続テスト
COM_WIFI_GETNAME = $F4   ;///< Wi-Fi command: read the player name. ; WIFI NICNEAME 取得
COM_WIFI_SETNAME = $F5   ;///< Wi-Fi command: set the player name. ; WIFI NICNEAME 設定


COM_WIFI_SYNC    = $FC   ;///< Wi-Fi command: synchronise peers. ; ゲームデータデータ同期
COM_WIFI_MATCHING_VS = $FD   ;///< Wi-Fi command: find an opponent. ; 対戦マッチング
COM_WIFI_CONNECT = $FE   ;///< Wi-Fi command: connect. ; WIFI接続
COM_WIFI_DISCONNECT = $FF   ;///< Wi-Fi command: disconnect. ; WIFI切断


;----------------------------
; セーブ・ロード暗号化キー
;----------------------------
SVDT_KEY0	EQU $3C   ;///< Save-data XOR key, byte 0. 
SVDT_KEY1	EQU $A1   ;///< Save-data XOR key, byte 1. 
SVDT_KEY2	EQU $BB   ;///< Save-data XOR key, byte 2. 
SVDT_KEY3	EQU $7F   ;///< Save-data XOR key, byte 3. 




;-------------------------------
; WIFI設定マネージャー
;-------------------------------
;WIFI_MANAGER:
;	lda  #COM_WIFI_MANAGER	; WIFI設定マネージャー
;	bne  WIFI_COMMAND_82


;-------------------------------
; WIFI SSID LIST取得
;-------------------------------
;/// @brief Lists visible SSIDs. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
WIFI_LSTSSID:
	lda  #COM_WIFI_LSTSSID		; WIFI SSID LIST取得
	bne  WIFI_COMMAND_82

;-------------------------------
; WIFI接続
;-------------------------------
;/// @brief Connects to an access point. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
WIFI_CONNECT:
	lda  #COM_WIFI_CONNECT	; WIFI接続
	bne  WIFI_COMMAND_82

;-------------------------------
; WIFI切断
;-------------------------------
;/// @brief Disconnects from the access point. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
WIFI_DISCONNECT:
	lda  #COM_WIFI_DISCONNECT	; WIFI切断
;	bne  WIFI_COMMAND_82

;-------------------------------
; WIFIコマンド共通処理
;-------------------------------
;/// @brief Issues expansion command `$82`. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
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
;/// @brief Uploads a score to the network service. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
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
;/// @brief Selects a cartridge RAM bank, 0-3. @note The only routine in this file still called.
;/// @ingroup bootrom
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


;/// @brief Sends a command to the expansion adapter. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
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
;/// @brief Sets adapter MP3 volume. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
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
;/// @brief Starts adapter MP3 playback. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
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


;/// @brief Stops adapter MP3 playback. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
stop_MP3:
	lda  #SYSCOM_MP3_A
	ldy  #$82			; 送信開始
	jsr  WB_Arduino_start
	lda  #$FF			; ストップコマンド
	jsr  WB_Arduino_send
	jmp  WB_Arduino_end


;/// @brief Writes save data to the adapter, XOR-obfuscated. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
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

;/// @brief Writes one byte to the `$5000` port. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
WB_Arduino:
	ldy  #$81		; 送信開始
	jsr  WB_Arduino_start

;/// @brief Ends a `$5000` transfer. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
WB_Arduino_end:
	ldx  #0
	stx	 $5000
	rts



;-------------------------------------
;	 arduino 送信ソフトウェイト（要調整）
;-------------------------------------
;/// @brief Fixed delay between `$5000` accesses. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
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
;/// @brief Writes a multi-byte block from #SRC_ADR. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
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
;/// @brief Begins a `$5000` transfer, declaring the byte count. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
WB_Arduino_start:
	sty	 $5000
	jsr  WB_Arduino_wait

;/// @brief Sends one byte and waits for the adapter. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
WB_Arduino_send:
	sta	 $5000
	jsr  WB_Arduino_wait
	lda	 $5000			; コマンド送信
	jmp  WB_Arduino_wait

;-------------------------------------
;	リードステータス
;-------------------------------------
;/// @brief Reads adapter status from `$5000`. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
RSTAT_Arduino:
	lda  $5000
	rts


;-------------------------------------
;	バーストモードバッファリード
;
;	SET_DATA_DST xxxxx	読み込みバッファアドレス
;
;-------------------------------------

;/// @brief Reads a burst block into a buffer. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
ReadBuf_Burst:
	SET_DATA_DST HTTP_BUF
;/// @brief Continuation of @ref ReadBuf_Burst. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
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

;/// @brief Reads a burst block of a given size. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
ReadBuf_BurstSiz:
	SET_DATA_DST HTTP_BUF
;/// @brief Continuation of @ref ReadBuf_BurstSiz. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
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
;/// @brief Starts a burst read. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
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


;/// @brief Reads one burst byte, handshaking via a toggling flag. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
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
;/// @brief Four-character game identifier sent to the adapter. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
EXS_GAMEID:
	db "A000"

;/// @brief Initialises the expansion adapter. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
EXS_INIT:
	jsr  RSTAT_Arduino			; ダミーリード
	SET_DATA_SRC EXS_GAMEID
	ldy  #$85
	lda  #SYSCOM_INIT
	jmp  WB_ArduinoMB



;-------------------------------------
;	拡張システムリセット
;-------------------------------------
;/// @brief Resets the expansion adapter. @note Vestigial FC-EXA feature.
;/// @ingroup bootrom
EXS_RESET:
	jsr  RSTAT_Arduino			; ダミーリード

	lda  #SYSCOM_INIT
	ldy  #$82		; 送信開始
	jsr  WB_Arduino_start

	lda  #0
	jsr  WB_Arduino_send
	jmp  WB_Arduino_end

