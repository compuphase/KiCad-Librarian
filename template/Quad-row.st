#version 1
#brief Pins on 4 sides
#note Pins can individually be reassigned to other sides.
#param 16 @?PT   PT 4 / floor @PT:2   PT:2 @PT:3   PT PT:2 - PT:3 - 2 / @PT:0   PT PT:0 - PT:2 - PT:3 - @PT:1  15 @BW   15 @BL   1.5 @TS
#prefix U
{BW 2 / mil snap @DX   BL 2 / mil snap @DY   200 @LEN}
DEF {NAME} {REF} 0 40 Y Y 1 F N
F0 "{REF}" {DX ~} {DY ~ 50 -} {TS mil} H V C CNN
F1 "{NAME}" 0 0 {TS mil} H V C CNN
F4 "Quad-row" {DX ~ 20 +} {DY ~ 20 +} 5 H I L BNN
DRAW
S {DX ~} {DY ~} {DX} {DY} 0 1 0 N
{?PS 1 <=}{DY PTS 1 + / snap 2 * 100 max @SPAN}
{?PS 2 >=}{DX PTS 1 + / snap 2 * 100 max @SPAN}
{PTS 1 - 2 / SPAN * @OFFS}
{?PS 0 =}X {PLB} {PN} {DX ~ LEN -} {PNS 1 - SPAN * OFFS - ~} {LEN} R {TS 0.75 * mil} {TS 0.75 * mil} 1 1 {PTY} {PSH}
{?PS 1 =}X {PLB} {PN} {DX LEN +} {PNS 1 - SPAN * OFFS - ~} {LEN} L {TS 0.75 * mil} {TS 0.75 * mil} 1 1 {PTY} {PSH}
{?PS 2 =}X {PLB} {PN} {PNS 1 - SPAN * OFFS - ~} {DY LEN +} {LEN} D {TS 0.75 * mil} {TS 0.75 * mil} 1 1 {PTY} {PSH}
{?PS 3 =}X {PLB} {PN} {PNS 1 - SPAN * OFFS -} {DY ~ LEN -} {LEN} U {TS 0.75 * mil} {TS 0.75 * mil} 1 1 {PTY} {PSH}
ENDDRAW
ENDDEF
$CMP {NAME}
D {DESCR}
$ENDCMP
