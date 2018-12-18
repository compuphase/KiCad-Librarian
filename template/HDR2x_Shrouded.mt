#version 1
#brief Vertical DIL shrouded header
#note Vertical DIL shrouded header with polarity slot (through-hole)
#pins 8 10 ...
#param 4 @?PT   PT 2 * @PT   "sqcircle" @PSH \
#      2.54 @PP   2.54 @SH   1.6 @PW   1.6 @PL   1.0 @DS   0.2 @BP   0.65 @TS   15 @TW \
#      8.89 @BW   PT 2 / 1 - PP * 10.16 + @BL
#model HDR2x_Shrouded
$MODULE {NAME}
AR HDR2x_Shrouded
Po 0 0 0 15 00000000 00000000 ~~
Li {NAME}
Cd {DESCR}
Kw {TAGS}
Op 0 0 0
At STD
{BW 2 / @X2   X2 ~ @X1   BL 2 / @Y2   Y2 ~ @Y1}
{TS @?TSR   TS ~ @?TSV   "V" "H" TSR 0 >= ? @TRvis   "V" "H" TSV 0 >= ? @TVvis}
{TSR abs @TSR   TSV abs @TSV   TW 100 / TSR * @TRpen   TW 100 / TSV * @TVpen}
T0 {X1 TSR - BP 2 * -} 0 {TSR} {TSR} 900 {TRpen} N {TRvis} 21 N "REF**"
T1 {X2 TSV + BP +} 0 {TSV} {TSV} 900 {TVpen} N {TVvis} 21 N "VAL**"
DS {X1} {Y1} {X2} {Y1} {BP} 21
DS {X1} {Y2} {X2} {Y2} {BP} 21
DS {X1} {Y1} {X1} {Y2} {BP} 21
DS {X2} {Y1} {X2} {Y2} {BP} 21
{0.8 @THK   2.1 @YS2   YS2 ~ @YS1}
DS {X1 THK +} {Y1 THK +} {X2 THK -} {Y1 THK +} {BP} 21
DS {X1 THK +} {Y2 THK -} {X2 THK -} {Y2 THK -} {BP} 21
DS {X2 THK -} {Y1 THK +} {X2 THK -} {Y2 THK -} {BP} 21
DS {X1 THK +} {Y1 THK +} {X1 THK +} {YS1} {BP} 21
DS {X1 THK +} {Y2 THK -} {X1 THK +} {YS2} {BP} 21
DS {X1} {YS1} {X1 THK +} {YS1} {BP} 21
DS {X1} {YS2} {X1 THK +} {YS2} {BP} 21
$PAD
{?PRR 0 <}Sh "{PN}" {PSH} {PW} {PL} 0 0 0
{?PRR 0 >=}Sh "{PN}" {PSH} {PW} {PL} 0 0 0 {PRR}
Dr {DS} 0 0
At STD N 00E0FFFF
Ne 0 ""
{? PN odd}Po {SH 2 / ~} {PT -0.25 * 0.5 - PN 2 / round + PP *}
{? PN even}Po {SH 2 /} {PT -0.25 * 0.5 - PN 2 / round + PP *}
$EndPAD
$SHAPE3D
Na "{NAME}.wrl"
Sc 0.3937 0.3937 0.3937
Of 0 0 0
Ro 0 0 0
$EndSHAPE3D
$EndMODULE {NAME}
