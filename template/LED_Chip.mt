#version 1
#brief Chip LED
#note Chip LED (multi-colour) in zig-zag pin numbering, SMD version
#pins 2 4 6
#param 4 @?PT    "rect" @PSH \
#      2.2 @PP   4.2 @SH   1.6 @PW   1.4 @PL   0.2 @BP   0.65 @TS   15 @TW   0.1 @STP \
#      5.2 @BW   3.4 @BL   1 @PPDIR
#flags rebuild
#model LED_Chip
$MODULE {NAME}
AR LED_Chip
Po 0 0 0 15 00000000 00000000 ~~
Li {NAME}
Cd {DESCR}
Kw {TAGS}
Op 0 0 0
At SMD
# in this template, pitch is vertical and span is horizontal, regardless of which
# one is smaller -> so swap if the librarian has those reversed
{? PPDIR 0 =}{PP @TMP   SV @PP   TMP @SH}
{TS @?TSR   TS ~ @?TSV   "V" "H" TSR 0 >= ? @TRvis   "V" "H" TSV 0 >= ? @TVvis}
{TSR abs @TSR   TSV abs @TSV   TW 100 / TSR * @TRpen   TW 100 / TSV * @TVpen}
T0 0 {PP PL + -0.5 * TSR - BP -} {TSR} {TSR} 0 {TRpen} N {TRvis} 21 N "REF**"
T1 0 {PP PL + 0.5 * TSV +} {TSV} {TSV} 0 {TVpen} N {TVvis} 21 N "VAL**"
{BW -2 / BP 2 * - @T2X   TS -4 / @T2Y}
{? SH PW + BW >}{SH PW + -2 / BP 2 * - @T2X}
T2 {T2X} {T2Y} {BP 2 *} {BP 2 *} 0 {BP 2 /} N V 21 N "○"
{BW 2 / @X2   X2 ~ @X1   BL 2 / @Y2   Y2 ~ @Y1}
{PP PL +   PL   PT 2 > ? @YSPAN   YSPAN STP 2 * + @YSPAN}
{? YSPAN BL >=}{SH PW - 2 / STP - @X2   X2 ~ @X1}
{YSPAN 2 / @Y2A   Y2A ~ @Y1A}
{?:HORLINES X1 X2 <}
DS {X1} {Y1} {X2} {Y1} {BP} 21
DS {X1} {Y2} {X2} {Y2} {BP} 21
:HORLINES
{?:VERTOUTLINE YSPAN BL <}
DS {X1} {Y1} {X1} {Y1A} {BP} 21
DS {X1} {Y2} {X1} {Y2A} {BP} 21
DS {X2} {Y1} {X2} {Y1A} {BP} 21
DS {X2} {Y2} {X2} {Y2A} {BP} 21
:VERTOUTLINE
{? YSPAN BL >=}{PP PL - 2 / STP - @Y2   Y2 ~ @Y1   BW 2 / @X2   X2 ~ @X1}
{?:VERLINES Y1 Y2 <}
DS {X1} {Y1} {X1} {Y2} {BP} 21
DS {X2} {Y1} {X2} {Y2} {BP} 21
:VERLINES
$PAD
{?PRR 0 <}Sh "{PN}" {PSH} {PW} {PL} 0 0 0
{?PRR 0 >=}Sh "{PN}" {PSH} {PW} {PL} 0 0 0 {PRR}
Dr 0 0 0
At SMD N 00888000
Ne 0 ""
{PN odd 0.5 - SH * @XPOS}
{PN 2 / 0.1 + round 1 -  PT 2 / round 1 - 2 / - PP * @YPOS}
Po {XPOS} {YPOS}
$EndPAD
$SHAPE3D
Na "{NAME}.wrl"
Sc 0.3937 0.3937 0.3937
Of 0 0 0
Ro 0 0 0
$EndSHAPE3D
$EndMODULE {NAME}
