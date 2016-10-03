#version 1
#brief Ball Grid Array
#note Square grid, either full grid or with centre void
#pins 9 16 24 25 32 36 40 48 49 56 60 64 72 80 81 84 88 96 100 104 108 112 120 121 132 140 144 156 160 169 176 180 192 196 200 208 220 225 256 240
#param 48 @PT 0.5 @PP 0.23 @PW 0.23 @PL 4.75 @BW 4.75 @BL 0.2 @BP 0.65 @TS 15 @TW
#model BGA
$MODULE {NAME}
AR BGA
Po 0 0 0 15 00000000 00000000 ~~
Li {NAME}
Cd {DESCR}
Kw BGA
Op 0 0 0
At SMD
{TS @?TSR   TS ~ @?TSV   "V" "H" TSR 0 >= ? @TRvis   "V" "H" TSV 0 >= ? @TVvis}
{TSR abs @TSR   TSV abs @TSV   TW 100 / TSR * @TRpen   TW 100 / TSV * @TVpen}
T0 0 {BL PW + 2 / TSR + ~} {TSR} {TSR} 0 {TRpen} N {TRvis} 21 N "REF**"
T1 0 {BL PW + 2 / TSV +} {TSV} {TSV} 0 {TVpen} N {TVvis} 21 N "VAL**"
T2 {BW 2 / BP + ~} {BL 2 / BP + ~} {BP 2 *} {BP 2 *} 0 {BP 2 /} N V 21 N "○"
{BW 2 / @X2   X2 ~ @X1   BL 2 / @Y2   Y2 ~ @Y1   0.4 @OFFS}
DS {X1 OFFS +} {Y1} {X2} {Y1} {BP} 21
DS {X2} {Y1} {X2} {Y2} {BP} 21
DS {X2} {Y2} {X1} {Y2} {BP} 21
DS {X1} {Y2} {X1} {Y1 OFFS +} {BP} 21
DS {X1} {Y1 OFFS +} {X1 OFFS +} {Y1} {BP} 21
#calculate the matrix size
{?PT 9 =}{3 @MTX}
{?PT 16 =}{4 @MTX}
{?PT ( 24 25 )=}{5 @MTX}
{?PT ( 32 36 )=}{6 @MTX}
{?PT ( 40 49 )=}{7 @MTX}
{?PT ( 48 60 64 )=}{8 @MTX}
{?PT ( 56 72 81 )=}{9 @MTX}
{?PT ( 84 100 )=}{10 @MTX}
{?PT ( 96 112 121 )=}{11 @MTX}
{?PT ( 80 108 140 144 )=}{12 @MTX}
{?PT ( 88 120 169 )=}{13 @MTX}
{?PT ( 132 160 180 196 )=}{14 @MTX}
{?PT ( 104 176 200 225 )=}{15 @MTX}
{?PT ( 156 192 220 256 )=}{16 @MTX}
{?PT ( 208 240 )=}{17 @MTX}
#INNER = size of inner (empty) square, e.g. for a 40-pin BGA, MTX==7 and INNER==3
#RING = number of rings, e.g. for a 40-pin BGA, RING==2; for a full matrix, RING = MTX
{MTX MTX * PT - sqrt round @INNER   MTX MTX INNER - 2 / INNER 0 = ? @RING}
#TOP = pin numbers 1 .. TOP fall in the top rings
#BOT = pin numbers BOT+1 .. PN fall in the bottom rings
{MTX RING * @TOP   PT TOP - @BOT}
{?BOT 0 =}{TOP @BOT}
$PAD
{?PN TOP <=}{PN 1 - MTX / floor 1 + @ROW   PN ROW 1 - MTX * - @COL}
{?PN BOT >}{PN BOT - 1 - MTX / floor 1 + @ROW   PN BOT - ROW 1 - MTX * - @COL   ROW RING + INNER + @ROW}
{?PN TOP > PN BOT <= &}{PN TOP - 1 - MTX INNER - / floor 1 + @ROW   PN TOP - ROW 1 - MTX INNER - * - @COL   ROW RING + @ROW   COL COL INNER + COL RING <= ? @COL}
Sh "{ROW gacol}{COL}" C {PW} {PL} 0 0 0
Dr 0 0 0
At SMD N 00888000
Ne 0 ""
Po {MTX -0.5 * 0.5 - COL + PP *} {MTX -0.5 * 0.5 - ROW + PP *}
$EndPAD
$SHAPE3D
Na "{NAME}.wrl"
Sc 0.3937 0.3937 0.3937
Of 0 0 0
Ro 0 0 0
$EndSHAPE3D
$EndMODULE {NAME}
