#version 1
#brief Small Outline w/ Pins, Exposed pad
#note Suitable for SOIC and SSOP parts. With exposed pad.
#pins 9 11 ...
#param 9 @PT  1.27 @PP  5.6 @SH  1.75 @PW  0.65 @PL  2.2 @PWA  3.0 @PLA  0.65 @TS  15 @TW \
#      3.6 @BW  5.0 @BL  0.2 @BP  20 @PSRA  0.2 @STP
#model SOP
$MODULE {NAME}
AR SOPX
Po 0 0 0 15 00000000 00000000 ~~
Li {NAME}
Cd {DESCR}
Kw SOIC SSOP TSSOP
Op 0 0 0
At SMD
#determine text size and visibility from the text parameter(s)
{TS @?TSR   TS ~ @?TSV   "V" "H" TSR 0 >= ? @TRvis   "V" "H" TSV 0 >= ? @TVvis}
{TSR abs @TSR   TSV abs @TSV   TW 100 / TSR * @TRpen   TW 100 / TSV * @TVpen}
#body size is bound to a maximum, because the drawing must not run over the pads
{SH PW - STP 2 * - BW min @BW}
#calculate the position of the (tiny) polarity marker
{BW 6 / 0.1 - 0.3 min @RAD   0.25 @OFFS   RAD BW 2 / - OFFS + @XC   RAD BL 2 / - OFFS + @YC}
#draw labels and body
T0 0 {BL 2 / TSR + ~} {TSR} {TSR} 0 {TRpen} N {TRvis} 21 N "REF**"
T1 0 {BL 2 / TSV +} {TSV} {TSV} 0 {TVpen} N {TVvis} 21 N "VAL**"
T2 {BW 2 / ~ BP 2 * -} {PT 1 - -0.25 * 0.5 + PP * PL - BP -} {BP 2 *} {BP 2 *} 0 {BP 2 /} N V 21 N "○"
DS {BW 2 / ~} {BL 2 / ~} {BW 2 /} {BL 2 / ~} {BP} 21
DS {BW 2 /} {BL 2 / ~} {BW 2 /} {BL 2 /} {BP} 21
DS {BW 2 /} {BL 2 /} {BW 2 / ~} {BL 2 /} {BP} 21
DS {BW 2 / ~} {BL 2 /} {BW 2 / ~} {BL 2 / ~} {BP} 21
DC {XC} {YC} {XC RAD +} {YC} {BP} 21
{PT 1 - @PINS}
$PAD
{? PN PINS <=}Sh "{PN}" O {PW} {PL} 0 0 0
{? PN PINS >}Sh "{PN}" R {PWA} {PLA} 0 0 0
{? PN PINS >}.SolderPasteRatio {PSRA -200 /}
Dr 0 0 0
At SMD N 00888000
Ne 0 ""
{? PN PINS 2 / <=}Po {SH 2 / ~} {PINS -0.25 * 0.5 - PN + PP *}
{? PN PINS 2 / > PN PINS <= &}Po {SH 2 /} {PINS 0.75 * 0.5 + PN - PP *}
{? PN PINS >}Po 0 0
$EndPAD
$SHAPE3D
Na "{NAME}.wrl"
Sc 0.3937 0.3937 0.3937
Of 0 0 0
Ro 0 0 0
$EndSHAPE3D
$EndMODULE {NAME}
