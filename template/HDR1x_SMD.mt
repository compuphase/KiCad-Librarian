#version 1
#brief Zig-zag In-line Package
#note Surface-mount single-line headers and sockets.
#pins 3 4 ...
#param 6 @PT    "rect" @PSH \
#      2.54 @PP   2.5 @SH   3.0 @PW   1.2 @PL   0.3 @STP   0.2 @BP   0.65 @TS   15 @TW \
#      2.5 @BW   15.14 @BL
#model HDR1x_SMD
$MODULE {NAME}
AR HDR1x_SMD
Po 0 0 0 15 00000000 00000000 ~~
Li {NAME}
Cd {DESCR}
Kw {TAGS}
Op 0 0 0
At SMD
{TS @?TSR   TS ~ @?TSV   "V" "H" TSR 0 >= ? @TRvis   "V" "H" TSV 0 >= ? @TVvis}
{TSR abs @TSR   TSV abs @TSV   TW 100 / TSR * @TRpen   TW 100 / TSV * @TVpen}
T0 0 {BL 2 / TSR + 0.2 + ~} {TSR} {TSR} 0 {TRpen} N {TRvis} 21 N "REF**"
T1 0 {BL 2 / TSV +} {TSV} {TSV} 0 {TVpen} N {TVvis} 21 N "VAL**"
T2 {BW 2 / ~ BP 2 * -} {PT -0.5 * 0.5 + PP * PL - BP - 0.1 -} {BP 2 *} {BP 2 *} 0 {BP 2 /} N V 21 N "○"
{BW 2 / @X2   X2 ~ @X1   BL 2 / @Y2   Y2 ~ @Y1}
{BL PP PT 1 - * - PL - 2 / STP - 0 max @SEGM} #SEGM must not be less than zero
DS {X1} {Y1} {X2} {Y1} {BP} 21
DS {X1} {Y2} {X2} {Y2} {BP} 21
DS {X1} {Y1} {X1} {Y1 SEGM +} {BP} 21
DS {X1} {Y2} {X1} {Y2 SEGM -} {BP} 21
DS {X2} {Y1} {X2} {Y1 SEGM +} {BP} 21
DS {X2} {Y2} {X2} {Y2 SEGM -} {BP} 21
$PAD
{?PRR 0 <}Sh "{PN}" {PSH} {PW} {PL} 0 0 0
{?PRR 0 >=}Sh "{PN}" {PSH} {PW} {PL} 0 0 0 {PRR}
Dr 0 0 0
At SMD N 00888000
Ne 0 ""
{? PN odd}Po {SH 2 / ~} {PT -0.5 * 0.5 - PN + PP *}
{? PN even}Po {SH 2 /} {PT -0.5 * 0.5 - PN + PP *}
$EndPAD
$SHAPE3D
Na "{NAME}.wrl"
Sc 0.3937 0.3937 0.3937
Of 0 0 0
Ro 0 0 0
$EndSHAPE3D
$EndMODULE {NAME}
