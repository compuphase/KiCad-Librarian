#version 1
#brief molded chip, polarized
#note Polarized shape, for tantalum capacitors.
#prefix MP
#pins 2
#param 2 @PT   "rect" @PSH \
#      5.0 @PP  1.7 @PW   2.2 @PL   0.2 @STP   0.2 @BP   0.65 @TS   15 @TW \
#      5.8 @BW  3.2 @BL 
#model MLDPOL
$MODULE {NAME}
AR MLDPOL
Po 0 0 0 15 00000000 00000000 ~~
Li {NAME}
Cd {DESCR}
Kw {TAGS}
Op 0 0 0
At SMD
{TS @?TSR   TS ~ @?TSV   "V" "H" TSR 0 >= ? @TRvis   "V" "H" TSV 0 >= ? @TVvis}
{TSR abs @TSR   TSV abs @TSV   TW 100 / TSR * @TRpen   TW 100 / TSV * @TVpen}
T0 0 {BL 2 / TSR + ~} {TSR} {TSR} 0 {TRpen} N {TRvis} 21 N "REF**"
T1 0 {BL 2 / TSV +} {TSV} {TSV} 0 {TVpen} N {TVvis} 21 N "VAL**"
{? BL PL STP 2 * + <}{PL STP 2 * + @BL}  #minimum width of the body = pad width + STP
{BW 2 / @X2   X2 ~ @X1   BL 2 / @Y2   Y2 ~ @Y1   BL PL - 2 / STP - 0 max @SEGM} #SEGM must not be less than zero
DS {X1} {Y1} {X2} {Y1} {BP} 21
DS {X1} {Y2} {X2} {Y2} {BP} 21
DS {X1} {Y1} {X1} {Y1 SEGM +} {BP} 21
DS {X1} {Y2} {X1} {Y2 SEGM -} {BP} 21
DS {X2} {Y1} {X2} {Y1 SEGM +} {BP} 21
DS {X2} {Y2} {X2} {Y2 SEGM -} {BP} 21
{BL 0.3 * 1 min @SYMSIZE}
T2 {PP PW + 2 / ~ SYMSIZE 2 / -} {PL 2 / ~ SYMSIZE 0.7 * -} {SYMSIZE} {SYMSIZE} 0 {SYMSIZE 4 /} N V 21 N "+"
{X1 BP 1.5 * + @X1}
DS {X1} {Y1} {X1} {Y1 SEGM +} {BP} 21
DS {X1} {Y2} {X1} {Y2 SEGM -} {BP} 21
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
