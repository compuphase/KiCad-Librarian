#version 1
#brief Small Outline w/ Pins
#note Suitable for SOIC and SSOP parts. Without exposed pad.
#pins 6 8 ...
#param 8 @?PT  1.27 @PP  5.6 @SH  1.75 @PW  0.65 @PL  0.2 @BP  0.65 @TS  15 @TW  0.2 @STP \
#      3.6 @BW  PT 2 / floor 1 - PP * 1.2 + @BL
#model SOP
$MODULE {NAME}
AR SOP
Po 0 0 0 15 00000000 00000000 ~~
Li {NAME}
Cd {DESCR}
Kw {TAGS}
Op 0 0 0
At SMD
#determine text size and visibility from the text parameter(s)
{TS @?TSR   TS ~ @?TSV   "V" "H" TSR 0 >= ? @TRvis   "V" "H" TSV 0 >= ? @TVvis}
{TSR abs @TSR   TSV abs @TSV   TW 100 / TSR * @TRpen   TW 100 / TSV * @TVpen}
#body size is bound to a maximum, because the drawing must not run over the pads
{SH PW - STP 2 * - BW min @BW}
#calculate the position of the polarity marker
{BW 4 / 0.1 - 0.5 min @RAD   0.25 @OFFS   RAD BW 2 / - OFFS + @XC   RAD BL 2 / - OFFS + @YC}
#draw labels and body
T0 0 {BL 2 / TSR + 0.2 + ~} {TSR} {TSR} 0 {TRpen} N {TRvis} 21 N "REF**"
T1 0 {BL 2 / TSV +} {TSV} {TSV} 0 {TVpen} N {TVvis} 21 N "VAL**"
T2 {BW 2 / ~ BP 2 * -} {PT -0.25 * 0.5 + PP * PL - BP - 0.1 -} {BP 2 *} {BP 2 *} 0 {BP 2 /} N V 21 N "○"
DS {BW 2 / ~} {BL 2 / ~} {BW 2 /} {BL 2 / ~} {BP} 21
DS {BW 2 /} {BL 2 / ~} {BW 2 /} {BL 2 /} {BP} 21
DS {BW 2 /} {BL 2 /} {BW 2 / ~} {BL 2 /} {BP} 21
DS {BW 2 / ~} {BL 2 /} {BW 2 / ~} {BL 2 / ~} {BP} 21
DC {XC} {YC} {XC RAD +} {YC} {BP} 21
$PAD
Sh "{PN}" O {PW} {PL} 0 0 0
Dr 0 0 0
At SMD N 00888000
Ne 0 ""
{? PN PT 2 / 1 + <}Po {SH 2 / ~} {PT -0.25 * 0.5 - PN + PP *}
{? PN PT 2 / >}Po {SH 2 /} {PT 0.75 * 0.5 + PN - PP *}
$EndPAD
$SHAPE3D
Na "{NAME}.wrl"
Sc 0.3937 0.3937 0.3937
Of 0 0 0
Ro 0 0 0
$EndSHAPE3D
$EndMODULE {NAME}
