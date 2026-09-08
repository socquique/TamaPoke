// ============================================================
//  TamaPoke - Carcasa Pokeball para Waveshare ESP32-S3-AMOLED-1.75
//  Clamshell con bisagra externa trasera + cierre por imanes delante.
//  Pantalla plana embutida e inclinada arriba-frente. Boton clasico detras.
//  Base blanca con patitas. Aloja board (Ø46), bateria (42x25.5x11) y
//  altavoz (30.5x20.5x6.7).
//
//  Ejes: Z arriba, +Y frente (pantalla/usuario), -Y atras (bisagra+boton).
//  Ecuador (costura) en z=0. Domo rojo z>0, base blanca z<0.
//
//  Partes (variable `part`): "top" | "bottom" | "all"
//  Conjunto: usa `explode` (separacion) u `open_ang` (giro bisagra).
// ============================================================

part = "all";   // all | top | bottom
$fn = 110;

// ---------- Esfera / casco ----------
outer_d   = 84;
wall      = 2.6;
Rout      = outer_d/2;
inner_d   = outer_d - 2*wall;
Rin       = inner_d/2;

// ---------- Junta de ecuador (spigot) ----------
lip_h     = 6;
lip_wall  = 1.8;
lip_gap   = 0.30;
spig_ro   = Rin - lip_gap;          // radio exterior del spigot
spig_ri   = Rin - lip_gap - lip_wall;

// ---------- Pantalla / board ----------
EL          = 50;
facet_d     = 33;
win_r       = 22.3;
board_bore_r= 23.6;
lip_t       = 1.2;
board_stack = 11;
board_t     = 1.6;
board_front = facet_d - lip_t;         // cara de pantalla (local +Z)
board_back  = board_front - board_t;   // cara trasera del PCB

// ---------- Bateria / altavoz ----------
bat        = [42, 25.5, 11];
bat_pos    = [0, -8, -13];
spk        = [30.5, 20.5, 6.7];
spk_pos    = [0, 14, -20];

// ---------- Boton trasero cosmetico ----------
btn_d      = 17;
btn_ring   = 23;
btn_EL     = 12;

// ---------- Patitas ----------
foot_n     = 3;
foot_r     = 4.5;
foot_drop  = 2.2;

// ---------- Imanes (cierre delantero) ----------
mag_d = 6.2; mag_h = 3.2;
mag_ang = [42, -42];                // azimut desde +Y (frente)
mag_rb  = spig_ri - mag_d/2 - 0.4;  // radio del centro del iman

// ---------- Bisagra externa trasera ----------
hin_off   = 3.0;                    // cuanto sobresale el eje por detras
hin_y     = -(Rout + hin_off);
hin_R     = 4.6;                    // radio del nudillo
pin_r     = 1.1;                    // taladro del pasador (varilla 2mm)
kn_top    = 6;                      // ancho nudillo central (top)
kn_gap    = 0.5;
kn_side   = 7;                      // ancho de cada nudillo lateral (bottom)

// ---------- USB-C / PWR (posiciones aproximadas en el borde del board) ----------
usb_phi   = 0;     // USB-C mira al frente-abajo (hacia la apertura)
pwr_phi   = 180;   // PWR en el borde opuesto (arriba-atras)

// ---------- Conjunto ----------
explode  = 0;
open_ang = 0;

// ============================================================
//  Utilidades
// ============================================================
module shell() { difference() { sphere(Rout); sphere(Rin); } }

// coloca hijos en azimut phi (desde +Y) a radio r y altura z
module at(phi, r, z=0) translate([r*sin(phi), r*cos(phi), z]) children();

module to_screen() { rotate([EL-90,0,0]) children(); }     // local +Z = normal pantalla
module to_button() { rotate([90-btn_EL,0,0]) children(); }  // hacia el boton (atras, sobre ecuador)

// anillo del spigot (solido, luego se le restan notches)
module spigot_ring() {
  translate([0,0,-lip_h])
    difference() { cylinder(r=spig_ro, h=lip_h); translate([0,0,-0.1]) cylinder(r=spig_ri, h=lip_h+0.2); }
}

// ============================================================
//  Recortes de pantalla (frame local +Z)
// ============================================================
module screen_cut() {
  translate([0,0,facet_d+50]) cube([260,260,100], center=true);  // cara plana
  translate([0,0,board_front]) cylinder(r=win_r, h=60);          // ventana
  translate([0,0,board_back-board_stack]) cylinder(r=board_bore_r, h=board_stack+board_t+0.01); // alojamiento
}

// clips de retencion del board (3 pestanias tras el PCB)
module board_clips() {
  to_screen() for (a=[20,140,260])
    rotate([0,0,a]) translate([board_bore_r-0.6, 0, board_back-1.2])
      rotate([0,-90,0]) cylinder(r1=0, r2=1.6, h=1.6);
}

// ============================================================
//  Imanes
// ============================================================
// dir = -1 -> mitad superior (bolsillo hacia abajo, en el spigot)
// dir = +1 -> mitad inferior (bolsillo hacia arriba, en el reborde)
module magnet_bosses(dir) {
  for (phi=mag_ang) at(phi, mag_rb, 0)
    if (dir<0) translate([0,0,-(mag_h+1.2)]) cylinder(r=mag_d/2+1.6, h=mag_h+1.2);
    else       cylinder(r=mag_d/2+1.6, h=mag_h+1.2);
}
module magnet_pockets(dir) {
  for (phi=mag_ang) at(phi, mag_rb, 0)
    if (dir<0) translate([0,0,-mag_h]) cylinder(r=mag_d/2, h=mag_h+0.1);
    else       translate([0,0,-0.1])   cylinder(r=mag_d/2, h=mag_h+0.1);
}

// ============================================================
//  Bisagra externa
// ============================================================
module pin_hole() translate([-15,hin_y,0]) rotate([0,90,0]) cylinder(r=pin_r, h=30);

// patch sobre el casco para soldar el web (z>0 top / z<0 bottom)
module hinge_top() {
  // nudillo central
  difference() {
    union() {
      translate([-kn_top/2,hin_y,0]) rotate([0,90,0]) cylinder(r=hin_R, h=kn_top);
      // web hacia el casco superior
      hull() {
        translate([-kn_top/2,hin_y,0]) rotate([0,90,0]) cylinder(r=hin_R-0.5, h=kn_top);
        at(180, Rout-2, 5) cube([kn_top, 4, 4], center=true);
      }
    }
    pin_hole();
  }
}
module hinge_bottom() {
  difference() {
    union() {
      for (s=[-1,1]) translate([s*(kn_top/2+kn_gap), hin_y, 0])
        rotate([0,90,0]) cylinder(r=hin_R, h=kn_side*(s>0?1:-1));
      hull() {
        for (s=[-1,1]) translate([s*(kn_top/2+kn_gap+ (s>0?kn_side:-kn_side)), hin_y,0])
          rotate([0,90,0]) cylinder(r=hin_R-0.5, h=0.1);
        at(180, Rout-2, -5) cube([kn_top+2*kn_side+2, 4, 4], center=true);
      }
    }
    pin_hole();
  }
}

// ============================================================
//  Notches: paso de cables (atras) y hueco USB-C (frente)
// ============================================================
module cable_notch() at(180, spig_ro-2, -lip_h/2) cube([12, 12, lip_h+2], center=true);
module usb_notch()   at(usb_phi, spig_ro-1, -lip_h/2) cube([13, 14, lip_h+2], center=true);

// acceso al PWR: taladro radial pequeno alineado al borde del board
module pwr_access() {
  to_screen() rotate([0,0,pwr_phi]) translate([22, 0, board_back+2])
    rotate([0,-90,0]) cylinder(r=1.8, h=14);
}

// ============================================================
//  Banda negra del ecuador (ranura para pintar / aro)
// ============================================================
module band_groove() rotate_extrude($fn=160)
  translate([Rout-0.35, 0]) circle(r=1.6, $fn=24);

// ============================================================
//  SEMIESFERA SUPERIOR (roja)
// ============================================================
module half_top() {
  difference() {
    union() {
      intersection() { shell(); translate([0,0,500]) cube(1000, center=true); }
      spigot_ring();
      magnet_bosses(-1);
      board_clips();
      hinge_top();
      // boss del boton trasero
      to_button() translate([0,0,Rout-1.4]) cylinder(r=btn_ring/2, h=1.8);
    }
    to_screen() screen_cut();
    magnet_pockets(-1);
    cable_notch();
    usb_notch();
    pwr_access();
    band_groove();
  }
}

// ============================================================
//  SEMIESFERA INFERIOR (blanca)
// ============================================================
module half_bottom() {
  difference() {
    union() {
      intersection() { shell(); translate([0,0,-500]) cube(1000, center=true); }
      for (i=[0:foot_n-1]) rotate([0,0,120*i+90]) translate([0,18,-Rout+1]) sphere(r=foot_r);
      translate(bat_pos) cradle(bat, 2.0);
      translate(spk_pos) cradle(spk, 2.0);
      magnet_bosses(1);
      hinge_bottom();
    }
    translate([0,0,-Rout-10+foot_drop]) cube([1000,1000,20], center=true);
    translate(bat_pos) box_pocket(bat, 0.6);
    translate(spk_pos) box_pocket(spk, 0.6);
    spk_grille();
    magnet_pockets(1);
    cable_notch();
    band_groove();
  }
}

module cradle(size, t) cube([size[0]+t*2, size[1]+t*2, size[2]+t], center=true);
module box_pocket(size, clr=0.5) cube([size[0]+clr, size[1]+clr, size[2]+clr], center=true);
module spk_grille() for (a=[0:40:359])
  translate([spk_pos[0]+8*cos(a), 0, spk_pos[2]+8*sin(a)])
    rotate([90,0,0]) translate([0,0,-Rout-1]) cylinder(r=1.1, h=wall+6);

// ============================================================
//  MOCKS (solo "all")
// ============================================================
module mock_board() {
  to_screen() translate([0,0,board_back-board_t/2]) {
    color("#1a3a1a") cylinder(r=23, h=board_t, center=true);
    color("#101010") translate([0,0,-board_t/2-2.5]) cylinder(r=18,h=5,center=true);
    color("#0a0a0a") translate([0,0,board_t/2+0.2]) cylinder(r=22, h=0.4);
  }
  // USB-C (mock) en el borde +Y del board
  color("#888") to_screen() rotate([0,0,usb_phi]) translate([22,0,board_back+1]) cube([9,5,3.2], center=true);
}
module mock_bat()  color("#2b6cb0") translate(bat_pos) cube(bat, center=true);
module mock_spk()  color("#222")    translate(spk_pos) cube(spk, center=true);
module mock_button() {
  color("#202020") to_button() translate([0,0,Rout-1.6]) cylinder(r=btn_ring/2+0.4, h=1.9);
  color("#c8c8cc") to_button() translate([0,0,Rout-0.1]) cylinder(r=btn_d/2, h=1.6);
}
module mock_pin() color("#444") translate([-13,hin_y,0]) rotate([0,90,0]) cylinder(r=pin_r-0.1, h=26);

// ============================================================
//  SELECTOR
// ============================================================
module top_group() { color("#d62828") half_top(); mock_board(); mock_button(); }

if (part=="top")    half_top();
else if (part=="bottom") half_bottom();
else {
  // giro de bisagra (open_ang) o separacion vertical (explode)
  translate([0,0,explode])
    translate([0,hin_y,0]) rotate([-open_ang,0,0]) translate([0,-hin_y,0]) top_group();
  color("#f4f4f4") half_bottom();
  mock_bat(); mock_spk(); mock_pin();
}
