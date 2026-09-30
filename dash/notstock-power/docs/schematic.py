#!/usr/bin/env python3
"""Draws docs/schematic.svg for the NOT STOCK power + CAN board.
Needs schemdraw (pip install schemdraw). Run from this folder:
    python docs/schematic.py

Three rows: +12 V in to +5 V out along the top, the ignition switch for the
regulator under it, CAN-H / CAN-L straight through along the bottom.
"""
import os

import schemdraw
import schemdraw.elements as elm

HERE = os.path.dirname(os.path.abspath(__file__))


def term(d, at, text, loc='left'):
    return d.add(elm.Dot(open=True).at(at).label(text, loc=loc))


with schemdraw.Drawing(show=False) as d:
    d.config(fontsize=12, unit=2.4)

    # ---------------------------------------------------------- power row
    j16 = term(d, (0, 0), 'J1-16\n+12 V (OBD)')
    d.add(elm.Fuse().right().label('F1  1 A slow', loc='top'))
    d.add(elm.Schottky().right().label('D1  SS34', loc='top'))
    a = d.add(elm.Dot())
    d.add(elm.Zener().down().reverse().label('D2\nSMBJ24A', loc='bottom'))
    d.add(elm.Ground())
    d.add(elm.Line().at(a.center).right(2.6))
    b = d.add(elm.Dot())
    d.add(elm.Capacitor(polar=True).down().label('C1\n47 µF\n50 V', loc='bottom'))
    d.add(elm.Ground())
    d.add(elm.Line().at(b.center).right(1.8))
    c = d.add(elm.Dot())
    d.add(elm.Capacitor().down().label('C2\n4.7 µF\n50 V', loc='bottom'))
    d.add(elm.Ground())
    d.add(elm.Line().at(c.center).right(2.4))

    # left side slots count from the bottom
    u1 = d.add(elm.Ic(pins=[elm.IcPin(name='VIN', side='left', slot='3/3'),
                            elm.IcPin(name='EN', side='left', slot='2/3'),
                            elm.IcPin(name='GND', side='left', slot='1/3'),
                            elm.IcPin(name='BOOT', side='right', slot='4/4'),
                            elm.IcPin(name='SW', side='right', slot='3/4'),
                            elm.IcPin(name='FB', side='right', slot='2/4'),
                            elm.IcPin(name='VCC', side='right', slot='1/4')],
                      size=(3.4, 6.0), pinspacing=1.5,
                      label='U1\nLMR36015\n4.2-60 V\n1.5 A buck', lblsize=11)
               .anchor('VIN'))
    d.add(elm.Line().at(u1.GND).left(0.6))
    d.add(elm.Ground())

    # SW -> L1 -> +5 V; the boot cap from BOOT to SW
    d.add(elm.Line().at(u1.SW).right(1.8))
    sw = d.add(elm.Dot())
    d.add(elm.Inductor2(loops=3).right().label('L1  10 µH', loc='bottom'))
    v5 = d.add(elm.Dot())
    d.add(elm.Line().at(u1.BOOT).right(0.7))
    d.add(elm.Capacitor().down().toy(sw.center).label('C3\n100 nF', loc='bottom'))
    d.add(elm.Line().at(u1.VCC).right(0.7))
    d.add(elm.Capacitor().down().label('C4\n1 µF', loc='bottom'))
    d.add(elm.Ground())

    # output caps, feedback divider (VFB 1.0 V: 5.0 V), LED, J2 +5 V
    d.add(elm.Line().at(v5.center).right(1.6))
    o1 = d.add(elm.Dot())
    d.add(elm.Capacitor().down().label('C5, C6\n2x 22 µF\n16 V', loc='bottom'))
    d.add(elm.Ground())
    d.add(elm.Line().at(o1.center).right(2.8))
    o2 = d.add(elm.Dot())
    d.add(elm.Resistor().down(3.8).label('R1\n100 k', loc='bottom'))
    fb = d.add(elm.Dot())
    d.add(elm.Resistor().down().label('R2\n24.9 k', loc='bottom'))
    d.add(elm.Ground())
    d.add(elm.Line().at(fb.center).tox(u1.FB[0] + 1.6))
    d.add(elm.Line().toy(u1.FB))
    d.add(elm.Line().tox(u1.FB))
    d.add(elm.Line().at(o2.center).right(2.2))
    o3 = d.add(elm.Dot())
    d.add(elm.Resistor().down().label('R5\n2.2 k', loc='bottom'))
    d.add(elm.LED().down().label('LED1', loc='bottom'))
    d.add(elm.Ground())
    d.add(elm.Line().at(o3.center).right(1.6))
    term(d, d.here, 'J2-1\n+5 V', loc='right')

    # ------------------------------------------------------ ignition row
    # EN: the ignition wire through R3 / R4 (on above about 2.4 V), or JP1
    # to +12 V for always on
    d.add(elm.Line().at(u1.EN).left(1.0))
    d.add(elm.Line().down(3.6))
    en = d.add(elm.Dot())
    d.add(elm.Resistor().down().label('R4\n100 k', loc='bottom'))
    d.add(elm.Ground())
    d.add(elm.Resistor().at(en.center).left(3.8).label('R3  100 k', loc='bottom'))
    ig = d.add(elm.Dot())
    d.add(elm.Line().left(4.0))
    term(d, d.here, 'J1-IGN\nswitched +12 V')
    d.add(elm.Line().at(ig.center).down(0.8))
    d.add(elm.Switch().down().label('JP1  always on\n(no IGN wire)', loc='bottom', ofst=0.2))
    d.add(elm.Dot(open=True).label('+12 V', loc='bottom'))

    # ----------------------------------------------------------- CAN row
    y = -9.5
    h1 = term(d, (0, y), 'J1-6\nCAN-H')
    d.add(elm.Line().right(5.0))
    hd = d.add(elm.Dot())
    d.add(elm.Line().right(4.0))
    hr = d.add(elm.Dot())
    d.add(elm.Line().right(6.0))
    term(d, d.here, 'J2-2\nCAN-H', loc='right')
    l1 = term(d, (0, y - 3.0), 'J1-14\nCAN-L')
    d.add(elm.Line().right(5.0))
    ld = d.add(elm.Dot())
    d.add(elm.Line().right(4.0))
    lr = d.add(elm.Dot())
    d.add(elm.Line().right(6.0))
    term(d, d.here, 'J2-3\nCAN-L', loc='right')
    # ESD: PESD1CAN, a diode pair from each line to ground
    d.add(elm.Zener().at(hd.center).down(1.3).label('D3a', loc='bottom'))
    d.add(elm.Line().right(1.4))
    gnd_e = d.add(elm.Dot())
    d.add(elm.Line().down(1.7))
    d.add(elm.Line().right(0.001))
    d.add(elm.Ground())
    d.add(elm.Zener().at(ld.center).up(1.3).label('D3b', loc='top'))
    d.add(elm.Line().right(1.4))
    d.add(elm.Line().toy(gnd_e.center))
    d.add(elm.Label().at((hd.center[0] - 2.6, y - 1.5)).label('D3\nPESD1CAN'))
    # optional termination
    d.add(elm.Resistor().at(hr.center).down(1.5).label('R6 120 Ω', loc='bottom'))
    d.add(elm.Switch().down().toy(lr.center).label('JP2 (term.)', loc='bottom'))

    # grounds
    term(d, (0, y - 6.0), 'J1-4/5\nGND')
    d.add(elm.Line().right(1.5))
    d.add(elm.Ground())
    d.add(elm.Line().right(13.5))
    term(d, d.here, 'J2-4\nGND', loc='right')

    d.save(os.path.join(HERE, 'schematic.svg'))
print('wrote docs/schematic.svg')
