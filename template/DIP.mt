#version 1
#brief Through-hole DIP
#note Normal and wide versions.
#pins 8 10 ...
#param 8 @?PT   "sqcircle" @PSH \
#      2.54 @PP  7.62 @SH   1.6 @PW    1.6 @PL   0.2 @STP   0.2 @BP   0.65 @TS   15 @TW \
#      5.5 @BW   PT 2 / floor 1 - PP * 2.2 + @BL
#model DIP
$MODULE {NAME}
AR DIP
Po 0 0 0 15 00000000 00000000 ~~
Li {NAME}
Cd {DESCR}
Kw {TAGS}
Op 0 0 0
At STD
#determine text size and visibility from the text parameter(s)
{TS @?TSR   TS ~ @?TSV   "V" "H" TSR 0 >= ? @TRvis   "V" "H" TSV 0 >= ? @TVvis}
{TSR abs @TSR   TSV abs @TSV   TW 100 / TSR * @TRpen   TW 100 / TSV * @TVpen}
#body size is bound to a maximum, because the drawing must not run over the pads
{SH PW - STP 2 * - BW min @BW}
#draw labels and body
T0 0 {BL 2 / TSR + ~} {TSR} {TSR} 0 {TRpen} N {TRvis} 21 N "REF**"
T1 0 {BL 2 / TSV +} {TSV} {TSV} 0 {TVpen} N {TVvis} 21 N "VAL**"
DS {BW 2 / ~} {BL 2 / ~} {BW 2 /} {BL 2 / ~} {BP} 21
DS {BW 2 /} {BL 2 / ~} {BW 2 /} {BL 2 /} {BP} 21
DS {BW 2 /} {BL 2 /} {BW 2 / ~} {BL 2 /} {BP} 21
DS {BW 2 / ~} {BL 2 /} {BW 2 / ~} {BL 2 / ~} {BP} 21
#draw polarity marker
{BW 4 / 0.1 - 0.5 min @RAD   0.5 @OFFS   RAD BW 2 / - OFFS + @XC   RAD BL 2 / - OFFS + @YC}
DC {XC} {YC} {XC RAD +} {YC} {BP} 21
$PAD
{?PRR 0 <}Sh "{PN}" {PSH} {PW} {PL} 0 0 0
{?PRR 0 >=}Sh "{PN}" {PSH} {PW} {PL} 0 0 0 {PRR}
Dr 0.8 0 0
At STD N 00E0FFFF
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
