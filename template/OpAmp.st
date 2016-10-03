#version 1
#brief Operational amplifiers
#note Supports additional input and auxiliary pins, single or dual outputs.
#param 5 @?PT   1 @PT:1   PT 1 - 2 / @PT:0   PT PT:0 - PT:1 - 2 / @PT:2   PT PT:0 - PT:1 - PT:2 - @PT:3   12 @BW   12 @BL   1.5 @TS
#prefix U
{BW 2 / mil snap 100 max @DX   BL 2 / mil snap 100 max @DY   200 @LEN   DY DX / 2 / @SLOPE}
DEF {NAME} {REF} 0 40 Y Y 1 F N
F0 "{REF}" {DX ~} {DY ~ 50 -} {TS mil} H V C CNN
F1 "{NAME}" -50 0 {TS mil} H V C CNN
F4 "OpAmp" {DX ~ 20 +} {DY ~ 20 +} 5 H I L BNN
DRAW
P 4 0 1 0 {DX ~} {DY ~} {DX} 0 {DX ~} {DY} {DX ~} {DY ~} N
{?PS 1 <=}{DY PTS 1 + / snap 2 * 100 max @SPAN}
{?PS 2 >=}{DX PTS 1 + / snap 2 * 100 max @SPAN   LEN 50 - @VLEN}
{PTS 1 - 2 / SPAN * @OFFS}
{?PS 0 =}X {PLB} {PN} {DX ~ LEN -} {PNS 1 - SPAN * OFFS - ~} {LEN} R {TS 0.75 * mil} {TS 0.75 * mil} 1 1 {PTY} {PSH}
{?PS 1 =}X {PLB} {PN} {DX LEN +} {PNS 1 - SPAN * OFFS - ~ @Y Y} {LEN} L {TS 0.75 * mil} {TS 0.75 * mil} 1 1 {PTY} {PSH}
{?PS 2 =}X {PLB} {PN} {PNS 1 - SPAN * OFFS - ~ @X X} {DY VLEN +} {VLEN DY 2 / X SLOPE * + +} D {TS 0.75 * mil} {TS 0.75 * mil} 1 1 {PTY} {PSH}
{?PS 3 =}X {PLB} {PN} {PNS 1 - SPAN * OFFS - @X X} {DY ~ VLEN -} {VLEN DY 2 / X SLOPE * + +} U {TS 0.75 * mil} {TS 0.75 * mil} 1 1 {PTY} {PSH}
{?:OUTPUT PS 1 = PT:1 1 > &}
{DX Y 2 / abs - @XP   DX XP - SLOPE * @YP   YP YP ~ Y 0 >= ? @YP}P 2 0 1 0 {DX} {Y} {XP} {YP} N
:OUTPUT
ENDDRAW
ENDDEF
$CMP {NAME}
D {DESCR}
$ENDCMP
