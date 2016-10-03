#version 1
#brief Quad Flat, No pins, Exposed pad
#note With exposed pad.
#pins 17 21 ...
#param 33 @PT 0.5 @PP 4.8 @SH 4.8 @SV 0.7 @PW 0.25 @PL 3.2 @PWA 3.2 @PLA 5.0 @BW 5.0 @BL 0.2 @STP 0.2 @BP 0.65 @TS 15 @TW 20 @PSRA
#model QFN
$MODULE {NAME}
AR QFNX
Po 0 0 0 15 00000000 00000000 ~~
Li {NAME}
Cd {DESCR}
Kw QFN TQFN
Op 0 0 0
At SMD
{TS @?TSR   TS ~ @?TSV   "V" "H" TSR 0 >= ? @TRvis   "V" "H" TSV 0 >= ? @TVvis}
{TSR abs @TSR   TSV abs @TSV   TW 100 / TSR * @TRpen   TW 100 / TSV * @TVpen}
T0 0 {SV PW + 2 / TSR + ~} {TSR} {TSR} 0 {TRpen} N {TRvis} 21 N "REF**"
T1 0 {SV PW + 2 / TSV +} {TSV} {TSV} 0 {TVpen} N {TVvis} 21 N "VAL**"
T2 {SH 2 / BP + ~} {SV 2 / BP + ~} {BP 2 *} {BP 2 *} 0 {BP 2 /} N V 21 N "○"
{BW 2 / @X2   X2 ~ @X1   BL 2 / @Y2   Y2 ~ @Y1   PT 4 / round @PINS}
{BW PL - 2 / PINS 2 / 0.5 - PP * - STP - 0 max @XSEG} #XSEG must not be less than zero
{BL PL - 2 / PINS 2 / 0.5 - PP * - STP - 0 max @YSEG} #same for YSEG
DS {X1} {Y1 YSEG +} {X1 XSEG +} {Y1} {BP} 21
DS {X2} {Y1} {X2 XSEG -} {Y1} {BP} 21
DS {X2} {Y1} {X2} {Y1 YSEG +} {BP} 21
DS {X1} {Y2} {X1 XSEG +} {Y2} {BP} 21
DS {X1} {Y2} {X1} {Y2 YSEG -} {BP} 21
DS {X2} {Y2} {X2 XSEG -} {Y2} {BP} 21
DS {X2} {Y2} {X2} {Y2 YSEG -} {BP} 21
{PT 1 - 4 / round @PINS}
$PAD
{PN 1 - PINS / floor @ROW} #row=0..3, or 4 for the exposed pad
{? ROW 0 = ROW 2 = |}Sh "{PN}" O {PW} {PL} 0 0 0
{? ROW 1 = ROW 3 = |}Sh "{PN}" O {PW} {PL} 0 0 900
{? ROW 4 =}Sh "{PN}" R {PWA} {PLA} 0 0 900
{? ROW 4 =}.SolderPasteRatio {PSRA -200 /}
Dr 0 0 0
At SMD N 00888000
Ne 0 ""
{? ROW 0 =}Po {SH -0.5 *} {PINS -0.5 * 0.5 - PN + PP *}
{? ROW 1 =}Po {PINS -1.5 * 0.5 - PN + PP *} {SV 0.5 *}
{? ROW 2 =}Po {SH 0.5 *} {PINS 2.5 * 0.5 + PN - PP *}
{? ROW 3 =}Po {PINS 3.5 * 0.5 + PN - PP *} {SV -0.5 *}
{? ROW 4 =}Po 0 0
$EndPAD
$SHAPE3D
Na "{NAME}.wrl"
Sc 0.3937 0.3937 0.3937
Of 0 0 0
Ro 0 0 0
$EndSHAPE3D
$EndMODULE {NAME}
