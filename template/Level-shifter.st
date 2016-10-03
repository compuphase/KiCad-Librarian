#version 1
#brief Level shifters and transceivers; pins on 2 sides.
#note Pins can individually be assigned to top/bottom sides, or to the IN/OUT sections.
#section IN left 0.3
#section OUT right 0.3
#param 10 @?PT   PT 2 / floor @PT:0   PT PT:0 - @PT:1   10 @BW   15 @BL   1.5 @TS
#prefix U
{BW 2 / mil snap @DX   BL 2 / mil snap @DY   200 @LEN}
{BL PSC:4 abs * @SECTL   SECTL BL SECTL - PSC:4 0 > ? mil @SECTL}
{BL PSC:5 abs * @SECTR   SECTR BL SECTR - PSC:5 0 > ? mil @SECTR}
DEF {NAME} {REF} 0 40 Y Y 1 F N
F0 "{REF}" {DX ~ 50 +} {DY ~ 50 -} {TS mil} H V C CNN
F1 "{NAME}" 0 {DY SECTL - 2 /} {TS mil} V V C CNN
F4 "Level-shifter" {DX ~ 20 +} {DY ~ 20 +} 5 H I L BNN
DRAW
S {DX ~} {DY ~} {DX} {DY} 0 1 0 N
P 2 0 1 0 {DX ~} {SECTL DY -} {DX} {SECTR DY -} N
P 2 0 1 0 0 {DY ~} 0 {SECTL SECTR + 2 / DY -} N
T 900 {TS 0.6 * mil ~} {SECTL 2 / DY -} {TS 0.75 * mil} 0 0 1 IN Italic 0 C C
T 900 {TS 0.6 * mil} {SECTR 2 / DY -} {TS 0.75 * mil} 0 0 1 OUT Italic 0 C C
{20 @SPAN}
{?PS 0 =}{DY 2 * SECTL - PTS 1 + / snap 100 max @SPAN}
{?PS 1 =}{DY 2 * SECTR - PTS 1 + / snap 100 max @SPAN}
{?PS 2 = PS 3 = |}{DX PTS 1 + / snap 2 * 100 max @SPAN}
{?PS 4 =}{SECTL PTS 1 + / snap 100 max @SPAN}
{?PS 5 =}{SECTR PTS 1 + / snap 100 max @SPAN}
{PTS 1 - 2 / SPAN * @OFFS}
{?PS 0 =}{OFFS SECTL 2 / + @OFFS}
{?PS 1 =}{OFFS SECTR 2 / - @OFFS}
{?PS 4 =}{OFFS DY SECTL 2 / - - @OFFS}
{?PS 5 =}{OFFS DY SECTR 2 / - + @OFFS}
{?PS 0 =}X {PLB} {PN} {DX ~ LEN -} {PNS 1 - SPAN * OFFS - ~} {LEN} R {TS 0.75 * mil} {TS 0.75 * mil} 1 1 {PTY} {PSH}
{?PS 1 =}X {PLB} {PN} {DX LEN +} {PNS 1 - SPAN * OFFS -} {LEN} L {TS 0.75 * mil} {TS 0.75 * mil} 1 1 {PTY} {PSH}
{?PS 2 =}X {PLB} {PN} {PNS 1 - SPAN * OFFS - ~} {DY LEN +} {LEN} D {TS 0.75 * mil} {TS 0.75 * mil} 1 1 {PTY} {PSH}
{?PS 3 =}X {PLB} {PN} {PNS 1 - SPAN * OFFS -} {DY ~ LEN -} {LEN} U {TS 0.75 * mil} {TS 0.75 * mil} 1 1 {PTY} {PSH}
{?PS 4 =}X {PLB} {PN} {DX ~ LEN -} {PNS 1 - SPAN * OFFS - ~} {LEN} R {TS 0.75 * mil} {TS 0.75 * mil} 1 1 {PTY} {PSH}
{?PS 5 =}X {PLB} {PN} {DX LEN +} {PNS 1 - SPAN * OFFS -} {LEN} L {TS 0.75 * mil} {TS 0.75 * mil} 1 1 {PTY} {PSH}
ENDDRAW
ENDDEF
$CMP {NAME}
D {DESCR}
$ENDCMP
