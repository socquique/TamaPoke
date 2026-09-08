// ============================================================
//  TamaPoke - Carcasa sobre el kit Pokeball "23"
//  Modifica el "Lid 23 Flat" (mismo domo para ambas mitades):
//    top    -> ventana de pantalla inclinada + cuna del board
//    bottom -> cunas de bateria/altavoz + huecos USB/PWR
//  Reutiliza Rings + Button + Pins del kit.
//
//  El Lid original: rim (boca) en z=0, domo hacia +Z hasta z=31.
//  Centro de la esfera ~ z=-3.5, R~34.3.  Frente = +Y.
// ============================================================

part = "top";   // top | bottom | all
$fn = 96;

KIT = "/Users/quique/Downloads/Basic+Parts";
LID_OFF = [-107.22, -101.95, 0];   // centra el Lid en XY (rim en z=0)

// ---- esfera del domo (para construir cortes) ----
Rsph   = 34.3;
Czsph  = -3.5;     // z del centro de la esfera

// ---- pantalla / board ----
EL          = 58;     // elevacion de la pantalla (grados sobre horizontal)
facet_d     = 22.5;   // distancia plano-facet al centro de la esfera
win_r       = 22.3;   // ventana visible (Ø44.6)
board_bore_r= 23.6;   // alojamiento del board (Ø46 + holgura)
lip_t       = 1.3;    // pestania frontal que retiene el board
board_stack = 11;     // profundidad board+componentes
board_t     = 1.6;

// ---- conjunto ----
explode = 0;

module lid() translate(LID_OFF) import(str(KIT,"/Lid 23 Flat.stl"), convexity=8);

// recortes de pantalla en frame local (+Z = normal de la pantalla)
module screen_cut() {
  translate([0,0,facet_d+50]) cube([300,300,100], center=true);        // cara plana
  translate([0,0,facet_d-lip_t]) cylinder(r=win_r, h=80);              // ventana
  translate([0,0,facet_d-lip_t-board_stack]) cylinder(r=board_bore_r, h=board_stack+0.02); // alojamiento
  // vaciado extra ancho: elimina el boss del agujero del boton original del Lid
  // (seguro: el eje pasa por el centro de la esfera, r<31.5 no perfora la pared)
  translate([0,0,facet_d-lip_t-board_stack-6]) cylinder(r=26, h=10);
}
module screen_at() translate([0,0,Czsph]) rotate([EL-90,0,0]) children();

// limpia el resto interno ciego del kit (queda como isla suelta al frente-bajo)
module boss_clear() translate([0,20,-2]) cylinder(r=11, h=16);

// topes de retencion del board (3 bultos tras el PCB)
module board_bumps() screen_at() for (a=[30,150,270])
  rotate([0,0,a]) translate([board_bore_r-0.3, 0, facet_d-lip_t-board_t-1.2]) sphere(r=1.5);

// ---- TOP: Lid con ventana + retencion ----
module top_half() {
  union() {
    difference() {
      lid();
      screen_at() screen_cut();
      boss_clear();
    }
    board_bumps();
  }
}

// ---- BOTTOM: Lid tal cual (de momento) ----
module bottom_half() lid();

// mock del board para comprobar encaje
module mock_board() screen_at() translate([0,0,facet_d-lip_t-board_t/2]) {
  color("#0b0b0b") cylinder(r=22, h=0.5, center=true);                 // pantalla activa
  color("#1a3a1a") translate([0,0,-board_t]) cylinder(r=23, h=board_t, center=true); // PCB
  color("#0a0a0a") translate([0,0,-board_t-3]) cylinder(r=17, h=6, center=true);     // componentes
}

// ---- selector ----
if (part=="topfit") { color("#d62828") top_half(); mock_board(); }
else if (part=="top") top_half();
else if (part=="bottom") bottom_half();
else {
  color("#d62828") translate([0,0,explode]) top_half();
  // mitad inferior: domo hacia abajo (rim arriba en z=0)
  color("#f4f4f4") mirror([0,0,1]) bottom_half();
}
