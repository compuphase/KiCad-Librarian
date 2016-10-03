#version 1
#brief ZIF socket for FPC cables, right-angle
#note ZIF socket with horizontal entry
#pins 4 6 ...
#flags mechanic(2) rebuild
#param 8 @PT  1.0 @PP  2.75 @SH 11.6 @SV  1.3 @PW 0.6 @PL  1.8 @PLA 2 @PWA \
#      5.8 @BW 12.6 @BL  0.2 @BP  0.65 @TS 15 @TW  0.2 @STP
#model ZIF-FPC
$MODULE {NAME}
AR ZIF-FPC
Po 0 0 0 15 00000000 00000000 ~~
Li {NAME}
Cd {DESCR}
Kw ZIF FPC
Op 0 0 0
At SMD
#determine text size and visibility from the text parameter(s)
{TS @?TSR   TS ~ @?TSV   "V" "H" TSR 0 >= ? @TRvis   "V" "H" TSV 0 >= ? @TVvis}
{TSR abs @TSR   TSV abs @TSV   TW 100 / TSR * @TRpen   TW 100 / TSV * @TVpen}
#calculate the body extents
{PW 2 / SH - BP + @X1   X1 BP - BW + @X2   BL 2 / @Y2   Y2 ~ @Y1   BW 2 / @TX}
{PT 2 / 0.5 - PP * PL 2 / + STP + @DY2   DY2 ~ @DY1   PWA 2 / STP + @DX2   DX2 ~ @DX1}
#draw labels and body
T0 {TX} {BL 2 / TSR + ~} {TSR} {TSR} 0 {TRpen} N {TRvis} 21 N "REF**"
T1 {TX} {BL 2 / TSV +} {TSV} {TSV} 0 {TVpen} N {TVvis} 21 N "VAL**"
DS {DX2} {Y1} {X2} {Y1} {BP} 21
DS {X2} {Y1} {X2} {Y2} {BP} 21
DS {X2} {Y2} {DX2} {Y2} {BP} 21
DS {X1} {DY2} {X1} {Y2} {BP} 21
DS {X1} {Y2} {DX1} {Y2} {BP} 21
DS {X1} {DY1} {X1} {Y1} {BP} 21
DS {X1} {Y1} {DX1} {Y1} {BP} 21
# pads
$PAD
{? PN PT <=}Sh "{PN}" O {PW} {PL} 0 0 0
{? PN PT >}Sh "" R {PWA} {PLA} 0 0 0
Dr 0 0 0
At SMD N 00888000
Ne 0 ""
{? PN PT <=}Po {SH ~} {PT -0.5 * 0.5 - PN + PP *}
{? PN PT 1 + =}Po 0 {SV 2 /}
{? PN PT 2 + =}Po 0 {SV 2 / ~}
$EndPAD
$SHAPE3D
Na "{NAME}.wrl"
Sc 0.3937 0.3937 0.3937
Of 0 0 0
Ro 0 0 0
$EndSHAPE3D
$EndMODULE {NAME}
