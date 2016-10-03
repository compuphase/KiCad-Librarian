#version 1
#brief Quad Flat Package w/ Pins, Exposed pad
#note With exposed pad.
#pins 29 33 ...
#param 33 @PT  0.8 @PP  8.5 @SH 8.5 @SV  1.3 @PW 0.45 @PL  5 @PLA 5 @PWA 6.8 \
#      @BW 6.8 @BL  0.2 @BP  0.65 @TS 15 @TW  20 @PSRA  0.2 @STP
#model QFP
$MODULE {NAME}
AR QFPX
Po 0 0 0 15 00000000 00000000 ~~
Li {NAME}
Cd {DESCR}
Kw QFP TQFP
Op 0 0 0
At SMD
#determine text size and visibility from the text parameter(s)
{TS @?TSR   TS ~ @?TSV   "V" "H" TSR 0 >= ? @TRvis   "V" "H" TSV 0 >= ? @TVvis}
{TSR abs @TSR   TSV abs @TSV   TW 100 / TSR * @TRpen   TW 100 / TSV * @TVpen}
#body size is bound to a maximum, because the drawing must not run over the pads
{SH PW - STP 2 * - BW min @BW   SV PW - STP 2 * - BL min @BL}
#calculate drawing extents and the "chamfer" that functions as the polarity marker
{BW 2 / @X2   X2 ~ @X1   BL 2 / @Y2   Y2 ~ @Y1   0.4 @OFFS}
#draw labels and body
T0 0 {SV PW + 2 / TSR + ~} {TSR} {TSR} 0 {TRpen} N {TRvis} 21 N "REF**"
T1 0 {SV PW + 2 / TSV +} {TSV} {TSV} 0 {TVpen} N {TVvis} 21 N "VAL**"
T2 {SH 2 / ~ BP +} {SV 2 / ~ BP +} {BP 2 *} {BP 2 *} 0 {BP 2 /} N V 21 N "○"
DS {X1 OFFS +} {Y1} {X2} {Y1} {BP} 21
DS {X2} {Y1} {X2} {Y2} {BP} 21
DS {X2} {Y2} {X1} {Y2} {BP} 21
DS {X1} {Y2} {X1} {Y1 OFFS +} {BP} 21
DS {X1} {Y1 OFFS +} {X1 OFFS +} {Y1} {BP} 21
#number of pins per side
{PT 1 - 4 / round @PINS}
$PAD
{PN 1 - PINS / floor @ROW}
{? ROW 0 = ROW 2 = |}Sh "{PN}" O {PW} {PL} 0 0 0
{? ROW 1 = ROW 3 = |}Sh "{PN}" O {PW} {PL} 0 0 900
{? ROW 4 =}Sh "{PN}" R {PWA} {PLA} 0 0 0
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
