#version 1
#brief SMD pin array, dual row
#note Standard dual row pin headers, SMD version
#pins 4 6 ...
#param 4 @?PT  PT 2 * @PT \
#      2.54 @PP  5.08 @SH  3.0 @PW  1.4 @PL  0.2 @BP  0.65 @TS  15 @TW  0.2 @STP \
#      5.08 @BW  PT 2 / PP * @BL
#model HDR2x_SMD
$MODULE {NAME}
AR HDR2x_SMD
Po 0 0 0 15 00000000 00000000 ~~
Li {NAME}
Cd {DESCR}
Kw HDR DIL
Op 0 0 0
At SMD
{BW 2 / @X2   X2 ~ @X1   BL 2 / @Y2   Y2 ~ @Y1}
{TS @?TSR   TS ~ @?TSV   "V" "H" TSR 0 >= ? @TRvis   "V" "H" TSV 0 >= ? @TVvis}
{TSR abs @TSR   TSV abs @TSV   TW 100 / TSR * @TRpen   TW 100 / TSV * @TVpen}
T0 {SH PW + -0.5 * TSR - BP -} 0 {TSR} {TSR} 900 {TRpen} N {TRvis} 21 N "REF**"
T1 {SH PW + 0.5 * TSV +} 0 {TSV} {TSV} 900 {TVpen} N {TVvis} 21 N "VAL**"
T2 {BW 2 / ~ BP 2 * -} {PT -0.25 * 0.5 + PP * PL - BP -} {BP 2 *} {BP 2 *} 0 {BP 2 /} N V 21 N "○"
{BL 2 / PT 4 / 0.5 - PP * - PL 2 / - STP - @OFFS}
DS {X1} {Y1} {X2} {Y1} {BP} 21
DS {X1} {Y1} {X1} {Y1 OFFS +} {BP} 21
DS {X2} {Y1} {X2} {Y1 OFFS +} {BP} 21
DS {X1} {Y2} {X2} {Y2} {BP} 21
DS {X1} {Y2} {X1} {Y2 OFFS -} {BP} 21
DS {X2} {Y2} {X2} {Y2 OFFS -} {BP} 21
$PAD
Sh "{PN}" R {PW} {PL} 0 0 0
Dr 0 0 0
At SMD N 00888000
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
