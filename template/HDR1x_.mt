#version 1
#brief Through-hole pin array, single row
#note Suitable for pin headers or resistor/capacitor arrays
#pins 2 3 ...
#param 5 @?PT   2.54 @PP   1.6 @PW   1.6 @PL   1.0 @DS   0.2 @BP   0.65 @TS   15 @TW
#model HDR1x_
$MODULE {NAME}
AR HDR1x_
Po 0 0 0 15 00000000 00000000 ~~
Li {NAME}
Cd {DESCR}
Kw HDR SIL
Op 0 0 0
At STD
{PP @BW   PT PP * @BL}
{BW 2 / @X2   X2 ~ @X1   BL 2 / @Y2   Y2 ~ @Y1}
{TS @?TSR   TS ~ @?TSV   "V" "H" TSR 0 >= ? @TRvis   "V" "H" TSV 0 >= ? @TVvis}
{TSR abs @TSR   TSV abs @TSV   TW 100 / TSR * @TRpen   TW 100 / TSV * @TVpen}
T0 {X1 TSR - BP 2 * -} 0 {TSR} {TSR} 900 {TRpen} N {TRvis} 21 N "REF**"
T1 {X2 TSV + BP +} 0 {TSV} {TSV} 900 {TVpen} N {TVvis} 21 N "VAL**"
DS {X1} {Y1} {X2} {Y1} {BP} 21
DS {X1} {Y2} {X2} {Y2} {BP} 21
DS {X1} {Y1} {X1} {Y2} {BP} 21
DS {X2} {Y1} {X2} {Y2} {BP} 21
{?PT 2 >}DS {X1} {Y1 PP +} {X2} {Y1 PP +} {BP} 21
$PAD
{? PN 1 =}Sh "{PN}" R {PW} {PL} 0 0 0
{? PN 1 >}Sh "{PN}" C {PW} {PL} 0 0 0
Dr {DS} 0 0
At STD N 00E0FFFF
Ne 0 ""
Po 0 {PT -0.5 * 0.5 - PN + PP *}
$EndPAD
$SHAPE3D
Na "{NAME}.wrl"
Sc 0.3937 0.3937 0.3937
Of 0 0 0
Ro 0 0 0
$EndSHAPE3D
$EndMODULE {NAME}
