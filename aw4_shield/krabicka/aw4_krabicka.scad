// AW4 shield v0.5 – krabička pro 3D tisk (OpenSCAD). Dva díly: PREDNI (displej) a ZADNI (kryt součástek).
// Souřadnice: X/Y = deska při pohledu shora na součástky (X = x_KiCad - 100, Y = 200 - y_KiCad), 0..100 mm.
// Z = 0 spodní strana desky (strana displeje), deska 0..1,6, součástky Z > 1,6, displej Z < 0.
// Díl: part = "predni" | "zadni" | "sestava"
part = "sestava";

$fn = 48;
BOARD = 100;          // deska 100 x 100
PCB_T = 1.6;
GAP = 1.0;            // vůle deska–stěna
WALL = 2.2;           // tloušťka stěn
FACE = 2.0;           // tloušťka čela
DISP_Z = 17.35;       // povrch skla displeje pod spodní stranou desky (změřeno)
REAR_IN = 22;         // vnitřní hloubka zadního krytu nad deskou (relé 15,7 mm + pojistkové pouzdro)
R_OUT = 4;            // zaoblení rohů

// Sklo displeje 37,1 x 58,8 mm (na výšku). PŘEDPOKLAD: horní hrana skla lícuje s horní hranou desky (strana USB-C)
// a sklo je vystředěné na modulu. Ověřte změřením a případně upravte GLASS_X0 / GLASS_Y1.
GLASS_W = 37.1; GLASS_H = 58.8;
GLASS_X0 = 18.0 - GLASS_W / 2;    // střed modulu X = 18
GLASS_Y1 = 100;                   // horní hrana skla
GLASS_CLR = 0.3;                  // vůle okna kolem skla

// Montážní díry desky (M3): H1, H2, H3
H1 = [85, 97.1]; H2 = [3.5, 48]; H3 = [92.5, 44.5];
SCREW_HOLE = 2.6;     // do sloupků v předním dílu – M3 samořezný šroub (pro zalisovací matici M3 dejte 4.0)
SCREW_CLR = 3.4;      // průchozí díra v zadním krytu
HEAD_D = 6.2;         // zahloubení hlavy šroubu

// USB-C modulu u horní hrany (Y = 100), střed X = 18
USB_W = 14; USB_Z0 = -16; USB_Z1 = -7;

// Tlačítka v čele vedle displeje (z předního pohledu vlevo od displeje, displej je vpravo nahoře).
// Za čelem je do desky jen ~13 mm (vývody na spodní straně desky) – spínače musí být krátké!
BTN_D = 12;
BTN = [[78, 82], [55, 82]];          // [READY, LOCKUP] v souřadnicích desky shora (z předu zrcadlově)
BTN_TXT = ["READY", "LOCKUP"];

// Závitové vložky na zatavení v zádech (držák): M4 – díra Ø5,6 hloubka 6 (upravte podle vložek)
INSERT_D = 5.6; INSERT_DEPTH = 6.0; INSERT_BOSS = 10;
INSERTS = [[10, 10], [90, 10], [10, 90], [90, 90]];   // rozteč 80 x 80 mm

// Pojistkové pouzdro Ø12 v pravé boční stěně zadního krytu (vevnitř 25 mm, prochází nad patici F1 – F1 neosazovat, propojit)
FUSE_D = 12; FUSE_Y = 28; FUSE_Z = PCB_T + 12;

// Výřezy pro vodiče u svorkovnic (dolní hrana, Y = 0): J3/J5/J4 a J2/J1
SLOT = [[0, 40], [59, 95]]; SLOT_H = 10;

IN0 = -GAP; IN1 = BOARD + GAP;            // vnitřní prostor X/Y
O0 = IN0 - WALL; O1 = IN1 + WALL;         // vnější obrys X/Y
Z_FRONT = -DISP_Z;                        // vnější čelo (sklo lícuje s čelem)
Z_SPLIT = PCB_T;                          // dělicí rovina = horní plocha desky
Z_TOP = PCB_T + REAR_IN + WALL;           // vnější záda

module rbox(x0, y0, z0, x1, y1, z1, r) {
  hull() for (x = [x0 + r, x1 - r], y = [y0 + r, y1 - r]) translate([x, y, z0]) cylinder(r = r, h = z1 - z0);
}

module predni() {
  difference() {
    union() {
      difference() {
        rbox(O0, O0, Z_FRONT, O1, O1, Z_SPLIT, R_OUT);
        rbox(IN0, IN0, Z_FRONT + FACE, IN1, IN1, Z_SPLIT + 1, R_OUT - WALL > 0.5 ? R_OUT - WALL : 0.5);
      }
      // sloupky pod H1 a H3 (H2 je pod přesahem skla – tam drží deska jen shora)
      for (h = [H1, H3]) translate([h[0], h[1], Z_FRONT + FACE - 0.01]) cylinder(d = 7, h = DISP_Z - FACE + 0.01);
      // opěrné lišty pod pravou a dolní hranou desky
      translate([IN1 - GAP - 1.2, 10, -2]) cube([GAP + 1.2, 60, 2]);
      translate([45, IN0, -2]) cube([40, GAP + 1.2, 2]);
    }
    // okno pro sklo displeje (prochází čelem)
    translate([GLASS_X0 - GLASS_CLR, GLASS_Y1 - GLASS_H - GLASS_CLR, Z_FRONT - 1])
      cube([GLASS_W + 2 * GLASS_CLR, GLASS_H + 2 * GLASS_CLR, FACE + 2]);
    // otvory pro šrouby ve sloupcích
    for (h = [H1, H3]) translate([h[0], h[1], Z_FRONT + FACE - 0.5]) cylinder(d = SCREW_HOLE, h = DISP_Z);
    // USB-C
    translate([18 - USB_W / 2, IN1 - 1, USB_Z0]) cube([USB_W, WALL + 3, USB_Z1 - USB_Z0]);
    // tlačítka READY / LOCKUP
    for (b = BTN) translate([b[0], b[1], Z_FRONT - 1]) cylinder(d = BTN_D, h = FACE + 2);
    // vyryté popisky pod tlačítky (čtou se zepředu)
    for (i = [0, 1]) translate([BTN[i][0], BTN[i][1] - BTN_D / 2 - 6, Z_FRONT - 0.01]) mirror([1, 0, 0])
      linear_extrude(0.6) text(BTN_TXT[i], size = 3.5, halign = "center", font = "DejaVu Sans:style=Bold");
  }
}

module zadni() {
  difference() {
    union() {
      difference() {
        rbox(O0, O0, Z_SPLIT, O1, O1, Z_TOP, R_OUT);
        rbox(IN0, IN0, Z_SPLIT - 1, IN1, IN1, Z_TOP - WALL, R_OUT - WALL > 0.5 ? R_OUT - WALL : 0.5);
      }
      // středicí límec – zasune se do předního dílu mezi stěnu a desku
      difference() {
        rbox(IN0 + 0.15, IN0 + 0.15, -PCB_T + 0.2 + PCB_T - 1.4, IN1 - 0.15, IN1 - 0.15, Z_SPLIT + 0.01, 0.5);
        rbox(IN0 + 0.95, IN0 + 0.95, -5, IN1 - 0.95, IN1 - 0.95, Z_SPLIT + 1, 0.5);
      }
      // sloupky, které tlačí desku: H1, H3 (se šroubem) a H2
      for (h = [H1, H2, H3]) translate([h[0], h[1], Z_SPLIT]) cylinder(d = 7, h = REAR_IN + 0.01);
      // zesílení zad pod závitové vložky
      for (v = INSERTS) translate([v[0], v[1], Z_TOP - WALL - (INSERT_DEPTH + 1.5 - WALL)]) cylinder(d = INSERT_BOSS, h = INSERT_DEPTH + 1.5 - WALL + 0.01);
    }
    for (h = [H1, H3]) {
      translate([h[0], h[1], Z_SPLIT - 1]) cylinder(d = SCREW_CLR, h = REAR_IN + WALL + 2);
      translate([h[0], h[1], Z_TOP - 2.5]) cylinder(d = HEAD_D, h = 3);
    }
    // výřezy pro vodiče u svorkovnic (dolní stěna)
    for (s = SLOT) translate([s[0], O0 - 1, Z_SPLIT - 1]) cube([s[1] - s[0], WALL + 2.5, SLOT_H + 1]);
    // díry pro závitové vložky (zatavit páječkou zvenku)
    for (v = INSERTS) translate([v[0], v[1], Z_TOP - INSERT_DEPTH]) cylinder(d = INSERT_D, h = INSERT_DEPTH + 1);
    // pojistkové pouzdro Ø12 v pravé stěně
    translate([IN1 - 1, FUSE_Y, FUSE_Z]) rotate([0, 90, 0]) cylinder(d = FUSE_D, h = WALL + 3);
    // límec nesmí kolidovat se svorkovnicemi u výřezů
    for (s = SLOT) translate([s[0], IN0 - 0.5, -5]) cube([s[1] - s[0], 2, 10]);
  }
}

if (part == "predni") predni();
else if (part == "zadni") zadni();
else { color("dimgray") predni(); color("silver", 0.6) translate([0, 0, 8]) zadni(); }
