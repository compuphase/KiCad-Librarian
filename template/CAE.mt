#version 1
#brief Aluminium Electrolytic Capacitor
#pins 2
#param 2 @PT   "rect" @PSH \
#      4 @PP    2.5 @PW   1.0 @PL   0.2 @STP   0.2 @BP   0.65 @TS   15 @TW \
#      5.3 @BW  5.3 @BL
#model CAE
$MODULE {NAME}
AR CAE
Po 0 0 0 15 51D5CF28 00000000 ~~
Li {NAME}
Cd {DESCR}
Kw {TAGS}
Op 0 0 0
{TS @?TSR   TS ~ @?TSV   "V" "H" TSR 0 >= ? @TRvis   "V" "H" TSV 0 >= ? @TVvis}
{TSR abs @TSR   TSV abs @TSV   TW 100 / TSR * @TRpen   TW 100 / TSV * @TVpen}
T0 0 {BL 2 / TSR 1.2 * + ~} {TSR} {TSR} 0 {TRpen} N {TRvis} 21 N "REF**"
T1 0 {BL 2 / TSV 1.2 * +} {TSV} {TSV} 0 {TVpen} N {TVvis} 21 N "VAL**"
{BL 2 / 0.15 - @RAD   75 cos RAD * @DX   75 sin RAD * @DY}
DS {DX} {DY ~} {DX} {DY} {BP} 21
{25 cos RAD * @DX   25 sin RAD * @DY   PL 2 / STP + @OFF   BL 5 / @CUT}
DA 0 0 {DX} {DY} 1300 {BP} 21
DA 0 0 {DX ~} {DY ~} 1300 {BP} 21
DS {BW 2 / ~} {BL 2 / CUT - ~} {BW 2 / ~} {OFF ~} {BP} 21
DS {BW 2 / ~} {BL 2 / CUT -} {BW 2 / ~} {OFF} {BP} 21
DS {BW 2 / CUT - ~} {BL 2 / ~} {BW 2 /} {BL 2 / ~} {BP} 21
DS {BW 2 / CUT - ~} {BL 2 /} {BW 2 /} {BL 2 /} {BP} 21
DS {BW 2 / CUT - ~} {BL 2 / ~} {BW 2 / ~} {BL 2 / CUT - ~} {BP} 21
DS {BW 2 / CUT - ~} {BL 2 /} {BW 2 / ~} {BL 2 / CUT -} {BP} 21
DS {BW 2 /} {BL 2 / ~} {BW 2 /} {OFF ~} {BP} 21
DS {BW 2 /} {BL 2 /} {BW 2 /} {OFF} {BP} 21
$PAD
{?PRR 0 <}Sh "{PN}" {PSH} {PW} {PL} 0 0 0
{?PRR 0 >=}Sh "{PN}" {PSH} {PW} {PL} 0 0 0 {PRR}
Dr 0 0 0
At SMD N 00888000
Ne 0 ""
Po {PN 1.5 - PP *} 0
$EndPAD
$SHAPE3D
Na "{NAME}.wrl"
Sc 0.3937 0.3937 0.3937
Of 0 0 0
Ro 0 0 0
$EndSHAPE3D
$EndMODULE {NAME}
