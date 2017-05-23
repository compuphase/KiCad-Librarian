#version 1
#brief SOT23 and similar
#note Supports 3, 5, 6 and 8-pin parts, but not 4-pin. Also suitable for SOT353 series and others.
#pins 3 5 6 8
#param 3 @?PT 0.95 @PP 2.3 @SH 1.1 @PW 1.0 @PL 0.8 @BW 2.9 @BL 0.2 @BP 0.65 @TS 15 @TW
#model SOT23
$MODULE {NAME}
AR SOT23
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
{? PT 6 >=}T2 {BW 2 / ~ BP 2 * -} {PT -0.25 * 0.5 + PP * PL - BP - 0.1 -} {BP 2 *} {BP 2 *} 0 {BP 2 /} N V 21 N "○"
DS {BW 2 / ~} {BL 2 / ~} {BW 2 /} {BL 2 / ~} {BP} 21
DS {BW 2 /} {BL 2 / ~} {BW 2 /} {BL 2 /} {BP} 21
DS {BW 2 /} {BL 2 /} {BW 2 / ~} {BL 2 /} {BP} 21
DS {BW 2 / ~} {BL 2 /} {BW 2 / ~} {BL 2 / ~} {BP} 21
DC {BW 4 / 0.15 - @RAD RAD BW 2 / - 0.15 +} {RAD BL 2 / - 0.2 +} {0.15 BW 2 / -} {0.2 BL 2 / - 0.1 +} {BP} 21
$PAD
Sh "{PN}" R {PW} {PL} 0 0 0
Dr 0 0 0
At SMD N 00888000
Ne 0 ""
{? PT 3 = PN 3 < &}Po {SH 2 / ~} {PN 1.5 - 2 * PP *}
{? PT 3 = PN 3 = &}Po {SH 2 /} 0
{? PT 3 > PN PT 2 / ceil 1 + < &}Po {SH 2 / ~} {PT 2 / ceil 1 - 2 / PP * ~ PN 1 - PP * +}
{? PT 5 = PN 3 > &}Po {SH 2 /} {4.5 PN - 2 * PP *}
{? PT 5 > PN PT 2 / > &}Po {SH 2 /} {PT 0.75 * 0.5 + PN - PP *}
$EndPAD
$SHAPE3D
Na "{NAME}.wrl"
Sc 0.3937 0.3937 0.3937
Of 0 0 0
Ro 0 0 0
$EndSHAPE3D
$EndMODULE {NAME}
