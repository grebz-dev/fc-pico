;=====================================
;
;	雑魚制御システム
;
;
;=====================================

;	.INCLUDE	".\CFG\cfgEnemy.h"

;----------------------------
; 敵の弾のコンフィグデータ取得
; Areg = 敵の弾の種類番号 (0でコール禁止）
; ret-> TMP_WRK0,TMP_WRK1,TMP_WRK2,TMP_WRK3 にセット
;----------------------------
getEnemyNTcfg2:
	cmp  <CACHE_GET_NENMY_NT_FG
	beq  getEnemyNTcfg_end
	sta  <CACHE_GET_NENMY_NT_FG
getEnemyNTcfg:
	stx  <TMP_SYS
	asl  a
	asl  a
	tax
	lda  tblEnemyNTcfg-4,x
	sta  <TMP_WRK0
	lda  tblEnemyNTcfg-3,x
	sta  <TMP_WRK1
	lda  tblEnemyNTcfg-2,x
	sta  <TMP_WRK2
	lda  tblEnemyNTcfg-1,x
	sta  <TMP_WRK3
	ldx  <TMP_SYS
getEnemyNTcfg_end:
	rts


;-------------------
; 自機通常弾あたり
;-------------------
hit_ShotA:
	lda  #12
	sta  <PRM_WT_POS
	lda  #24
	sta  <PRM_W_POS

	lda  #12
	sta  <PRM_HT_POS
	lda  #24
	sta  <PRM_H_POS
	jsr  hit_ShotA2
	bcc  .end
	cpy  #0
	beq  .end
	lda  #0
	sta  POS_PLY_Y,Y	; 弾を消す
	lda  #PS_NOMAL_POW		; 通常弾のダメージ
.end
	rts


hit_ShotA2:
	sty  <TMP_SVY
	ldy  #0

	lda  PLY_MUTEKI_TM
	bne  .next_nt

	; プレーヤー当たり判定（サイズ小さめ）
	lda  <PRM_Y_POS
	cmp  #SP_CLR_Y
	beq  .next_nt
	sec
	sbc  POS_PLY_Y,Y
	adc  #PLY_HIT_ADD_Y
    cmp  #PLY_HIT_SIZ_H
	bcs	.next_nt

	
	lda  <PRM_X_POS
	sec
	sbc  POS_PLY_X,Y
	clc
	adc  #PLY_HIT_ADD_X
    cmp  #PLY_HIT_SIZ_W
	bcs .next_nt

	; 当たり
	sec
	rts


	; 通常弾の当たり判定
.loop_nt
	lda  POS_PLY_Y,y
	beq  .next_nt

	lda  <PRM_X_POS
	sec
	sbc  POS_PLY_X,Y
	clc
	adc  <PRM_WT_POS
    cmp  <PRM_W_POS
	bcs .next_nt

	lda  <PRM_Y_POS
	sec
	sbc  POS_PLY_Y,Y
	adc  <PRM_HT_POS
    cmp  <PRM_H_POS
	bcs	.next_nt

	; 当たり
	sec
	rts

.next_nt
	iny
	iny
	cpy  <TMP_SVY
	bne  .loop_nt

	clc
	rts


 
