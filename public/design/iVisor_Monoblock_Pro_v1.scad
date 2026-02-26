/*
  iVisor PRO "Monoblock Retail" (V1.9 - POWER EXIT BOTTOM)
  - Profundidad y alturas como el primer diseño “perfecto”:
      body_depth = 22mm, wall_th = 2.4mm, ws_standoff_h = 8mm
  - Orejas para colgar en pared: KEYHOLE en la tapa (back_cover)
  - Debug: tornillos dummy alineados al plano real de la tapa
  - NUEVO: salida de cable de alimentación ABAJO (en back_cover) + canal interno
*/

$fn = 64;

// ======================= DISPLAY (mm) =======================
panel_w = 192.96;
panel_h = 110.76;

window_w = 154.88;
window_h = 86.72;
window_x = 19.04;
window_y = 13.04;

bezel_border = 12;
corner_r = 10.0;

// Waveshare holes
dev_plate_w = 165.72;
dev_plate_h = 97.60;
dev_hole_x     = 63.10;
dev_hole_y_top = 30.49;
dev_hole_y_bot = -35.16;

// ======================= LECTOR (mm) =======================
reader_w = 61.0;
reader_h = 58.0;
reader_hole_d  = 3.5;
reader_hole_dx = 44.0;
reader_hole_dy = 51.0;

reader_front_window_w = 52.0;
reader_front_window_h = 40.0;

reader_seat_clear = 0.35;
reader_seat_depth = 1.8;

reader_slot_len = 2.5;
reader_slot_d   = 3.6;

// Canal USB lector (en tapa)
usb_channel_w = 10.0;
usb_channel_h = 4.0;
usb_channel_len = 35.0;
usb_channel_offset_y = 0;
usb_channel_side = -1;

// ===== POWER: salida de cable ABAJO (en tapa) =====
pwr_slot_w = 14.0;        // ancho del hueco hacia abajo (USB-C típico 12-14)
pwr_slot_h = 7.5;         // alto del hueco
pwr_slot_inset = 8.0;     // qué tanto entra desde el borde inferior hacia adentro

pwr_channel_w = 8.0;     // ancho del canal interno en tapa
pwr_channel_h = 4.0;      // altura del canal (rebaje)
pwr_channel_len = 10.0;   // largo del canal hacia arriba desde abajo (ajústalo)
pwr_channel_offset_x = 0; // mueve izquierda/derecha la bajada del cable (0 = centro)

// ======================= CAJA (mm) =======================
wall_th      = 2.4;
front_th     = 3.0;
body_depth   = 22.0;

lip_h        = 2.0;
lip_clear    = 0.25;

gap_between  = 14.0;
side_margin  = 12.0;
top_margin   = 12.0;

// ======================= TORNILLERIA =======================
m3_clear_d   = 3.4;
insert_d     = 4.6;

boss_od      = 8.5;
boss_h       = 8.0;

cover_th       = wall_th;
cover_screw_d  = 3.4;
cover_head_d   = 6.8;
cover_head_h   = 2.2;

cover_mount_head_d = 6.8;
cover_mount_head_h = 2.2;

// WAVESHARE standoffs
ws_standoff_h  = 8.0;
ws_standoff_od = 9.5;

// ======================= OREJAS PARED (KEYHOLE) =======================
wall_ear_enabled = true;

// placa de refuerzo por fuera
wall_ear_w = 26.0;
wall_ear_h = 18.0;
wall_ear_th = 4.0;

// ubicación keyhole
wall_ear_inset_x = 14.0; // desde borde lateral hacia adentro
wall_ear_offset_y = 0;

// Keyhole: cabeza + canal (IMPORTANTE: canal hacia ABAJO para colgar y bajar)
keyhole_head_d = 8.5;
keyhole_shank_w = 4.5;
keyhole_len = 10.0;     // longitud del canal hacia abajo
keyhole_clear = 0.25;

// Debug: muestra tornillos de pared (solo visual)
DEBUG_SHOW_WALL_SCREWS = true;
wall_screw_head_d = 8.0;  // aprox cabeza
wall_screw_shank_d = 4.0; // aprox vástago
wall_screw_head_h = 2.2;
wall_screw_len = 8;       // sobresale de pared (para colgar)

// ======================= LOGO =======================
logo_text   = "Innovative Solutions";
logo_size   = 12;
logo_depth  = 0.9;
logo_relief = true;

// ======================= DEBUG =======================
DEBUG_ASSEMBLY      = true;
DEBUG_EXPLODE       = 20;     // separa tapa hacia atrás
DEBUG_SHOW_SCREWS   = false;
DEBUG_SHOW_DUMMIES  = false;

// ======================= DERIVADOS =======================
display_outer_w = panel_w + 2*bezel_border;
display_outer_h = panel_h + 2*bezel_border;

body_w = side_margin*2 + reader_w + gap_between + display_outer_w;
body_h = max(display_outer_h + 2*top_margin, reader_h + 2*top_margin);

display_cx = ( body_w/2 - side_margin - display_outer_w/2 );
display_cy = 0;

reader_cx  = -( body_w/2 - side_margin - reader_w/2 );
reader_cy  = 0;

cover_pts = [
  [ body_w/2 - (side_margin+10),  body_h/2 - (top_margin+10) ],
  [-body_w/2 + (side_margin+10),  body_h/2 - (top_margin+10) ],
  [ body_w/2 - (side_margin+10), -body_h/2 + (top_margin+10) ],
  [-body_w/2 + (side_margin+10), -body_h/2 + (top_margin+10) ],
  [ 0,  body_h/2 - (top_margin+10) ],
  [ 0, -body_h/2 + (top_margin+10) ]
];

ws_pts = [
  [display_cx + dev_hole_x, display_cy + dev_hole_y_top],
  [display_cx - dev_hole_x, display_cy + dev_hole_y_top],
  [display_cx + dev_hole_x, display_cy + dev_hole_y_bot],
  [display_cx - dev_hole_x, display_cy + dev_hole_y_bot]
];

reader_pts = [
  [ reader_cx + reader_hole_dx/2, reader_cy + reader_hole_dy/2],
  [ reader_cx - reader_hole_dx/2, reader_cy + reader_hole_dy/2],
  [ reader_cx + reader_hole_dx/2, reader_cy - reader_hole_dy/2],
  [ reader_cx - reader_hole_dx/2, reader_cy - reader_hole_dy/2]
];

// ======================= HELPERS =======================
module rrect(w,h,r){
  minkowski(){
    square([w-2*r, h-2*r], center=true);
    circle(r=r);
  }
}
module rrect3(w,h,r,t){
  linear_extrude(height=t) rrect(w,h,r);
}
module thru_cyl(x,y,d,h,z0=-0.1){
  translate([x,y,z0]) cylinder(d=d, h=h+0.2);
}
module slot_through(x,y,len,d,h){
  translate([x,y,-0.1])
    hull(){
      translate([-len/2,0,0]) cylinder(d=d,h=h+0.2);
      translate([ len/2,0,0]) cylinder(d=d,h=h+0.2);
    }
}
module boss_insert(x,y,z0, od, h, id){
  translate([x,y,z0])
    difference(){
      cylinder(d=od, h=h);
      cylinder(d=id, h=h+0.4);
    }
}
module standoff_ws_from_cover(x,y){
  translate([x,y,0])
    difference(){
      cylinder(d=ws_standoff_od, h=ws_standoff_h);
      cylinder(d=m3_clear_d, h=ws_standoff_h+0.6);
    }
}
module screw_dummy(len=18, shank_d=3.0, head_d=6.8, head_h=2.2){
  // cabeza en Z=0..head_h, vástago hacia +Z
  union(){
    cylinder(d=head_d, h=head_h);
    translate([0,0,head_h]) cylinder(d=shank_d, h=len);
  }
}

// Keyhole 2D: CABEZA ARRIBA, CANAL HACIA ABAJO (–Y)
// Así cuelgas (entra por el círculo) y luego BAJAS para trabar.
module keyhole2d_down(head_d, shank_w, len){
  hull(){
    translate([0,0]) circle(d=head_d);        // círculo grande
    translate([0,-len]) circle(d=shank_w);    // canal hacia abajo
  }
}

// ======================= FRONT SHELL =======================
module front_shell(){
  difference(){
    rrect3(body_w, body_h, corner_r, body_depth);

    translate([0,0,front_th])
      rrect3(body_w - 2*wall_th,
             body_h - 2*wall_th,
             corner_r-1.2,
             body_depth - front_th + 0.2);

    translate([0,0,body_depth - lip_h])
      rrect3(body_w - 2*(wall_th + lip_clear),
             body_h - 2*(wall_th + lip_clear),
             corner_r-1.8,
             lip_h + 0.3);

    // Ventana pantalla
    win_local_x = -panel_w/2 + window_x + window_w/2;
    win_local_y = -panel_h/2 + window_y + window_h/2;
    translate([display_cx + win_local_x,
               display_cy + win_local_y,
               -0.05])
      cube([window_w, window_h, front_th+0.2], center=true);

    // Ventana lector
    translate([reader_cx, reader_cy, -0.05])
      cube([reader_front_window_w, reader_front_window_h, front_th+0.2], center=true);

    // Asiento lector
    translate([reader_cx, reader_cy, front_th - reader_seat_depth])
      cube([reader_w + 2*reader_seat_clear,
            reader_h + 2*reader_seat_clear,
            reader_seat_depth + 0.2], center=true);

    // Logo grabado/relieve VISIBLE por el frente:
    // hacemos "grabado" desde el frente (corte) para evitar que salga adentro
    if (logo_relief){
      translate([0, -(body_h/2) + 10,-logo_depth+0.05])
        mirror([1,0,0])
        rotate([0,0,180])
        linear_extrude(height=logo_depth)
          text(logo_text, size=logo_size, halign="center", valign="center",
               font="Liberation Sans:style=Bold");
    }
  }

  // LECTOR: bosses con inserto (tornillos entran por tapa)
  for(p = reader_pts)
    boss_insert(p[0], p[1], front_th, boss_od, boss_h, insert_d);

  // TAPA: bosses con inserto (6)
  for(p = cover_pts)
    boss_insert(p[0], p[1], front_th, 9.0, 10.0, insert_d);

  // WAVESHARE: NO bosses en front_shell (va desde atrás)
}

// ======================= BACK COVER =======================
module back_cover(){
  cover_w = body_w - 2*(wall_th + lip_clear);
  cover_h = body_h - 2*(wall_th + lip_clear);

  difference(){
    rrect3(cover_w, cover_h, corner_r-1.8, cover_th);

    // 6 tornillos tapa
    for(p=cover_pts){
      thru_cyl(p[0], p[1], cover_screw_d, cover_th);
      translate([p[0], p[1], -0.1]) cylinder(d=cover_head_d, h=cover_head_h);
    }

    // slots lector
    for(p=reader_pts){
      slot_through(p[0], p[1], reader_slot_len, reader_slot_d, cover_th);
      translate([p[0], p[1], -0.1]) cylinder(d=cover_mount_head_d, h=cover_mount_head_h);
    }

    // agujeros waveshare
    for(p=ws_pts){
      thru_cyl(p[0], p[1], m3_clear_d, cover_th);
      translate([p[0], p[1], -0.1]) cylinder(d=cover_mount_head_d, h=cover_mount_head_h);
    }

    // canal USB lector
    translate([reader_cx + usb_channel_side*(cover_w/2 - 8),
               reader_cy + usb_channel_offset_y,
               cover_th - usb_channel_h/2])
      cube([usb_channel_len, usb_channel_w, usb_channel_h], center=true);

    // ===== POWER: salida ABAJO + canal interno (solo alimentación) =====
    // Hueco en el borde inferior de la tapa:
    translate([pwr_channel_offset_x,
               -cover_h/2 + pwr_slot_inset/2,
               cover_th - pwr_slot_h/2])
      cube([pwr_slot_w, pwr_slot_inset + 0.2, pwr_slot_h + 0.2], center=true);

    // Canal superficial por dentro (rebaje) para que el cable no se pellizque al cerrar
    translate([pwr_channel_offset_x,
               -cover_h/2 + (pwr_channel_len/2),
               cover_th - pwr_channel_h/2])
      cube([pwr_channel_w, pwr_channel_len, pwr_channel_h], center=true);

    // ===== KEYHOLES (corte en tapa) =====
    if (wall_ear_enabled){
      ear_x = cover_w/2 - wall_ear_inset_x;
      ear_y = wall_ear_offset_y;

      // Derecha
      translate([ +ear_x, ear_y, -0.1])
        linear_extrude(height=cover_th+0.3)
          offset(r=keyhole_clear)
            keyhole2d_down(keyhole_head_d, keyhole_shank_w, keyhole_len);

      // Izquierda
      translate([ -ear_x, ear_y, -0.1])
        linear_extrude(height=cover_th+0.3)
          offset(r=keyhole_clear)
            keyhole2d_down(keyhole_head_d, keyhole_shank_w, keyhole_len);
    }
  }

  // WAVESHARE STANDOFFS desde cara interna de tapa
  translate([0,0,cover_th])
    for(p=ws_pts)
      standoff_ws_from_cover(p[0], p[1]);

  // Refuerzo orejas por fuera (material extra)
  if (wall_ear_enabled){
    cover_w = body_w - 2*(wall_th + lip_clear);
    ear_x = cover_w/2 - wall_ear_inset_x;
    ear_y = wall_ear_offset_y;

    for(sx=[-1,1]){
      translate([sx*ear_x, ear_y, -wall_ear_th])  // por fuera (lado pared)
        linear_extrude(height=wall_ear_th)
          rrect(wall_ear_w, wall_ear_h, 3.0);
    }
  }
}

// ======================= DEBUG =======================
module waveshare_dummy(){
  pcb_th = 12;

  // La tapa se coloca en:
  cover_z0 = body_depth - cover_th + DEBUG_EXPLODE;      // cara externa
  z_board_center = cover_z0 + cover_th + ws_standoff_h + pcb_th/2;

  translate([display_cx, display_cy, z_board_center])
    color([0.2,0.7,1,0.25])
      cube([dev_plate_w, dev_plate_h, pcb_th], center=true);

  for(p=ws_pts)
    translate([p[0], p[1], z_board_center + pcb_th/2])
      color([1,0.2,0.2,0.9]) cylinder(d=4, h=2);
}

module reader_dummy(){
  body_th = 18;
  translate([reader_cx, reader_cy, front_th + 1])
    color([0.1,1,0.3,0.20])
      cube([reader_w, reader_h, body_th], center=true);

  for(p=reader_pts)
    translate([p[0], p[1], front_th + 2])
      color([1,0.6,0,0.9]) cylinder(d=4, h=2);
}

module back_cover_with_screws(){
  cover_z0 = body_depth - cover_th + DEBUG_EXPLODE; // cara externa (lado pared)

  // Tapa
  translate([0,0,cover_z0])
    back_cover();

  if(DEBUG_SHOW_SCREWS){
    // Tornillos tapa (6): cabeza por fuera, vástago hacia adentro
    for(p=cover_pts){
      translate([p[0], p[1], cover_z0 - cover_head_h])
        screw_dummy(len=20, shank_d=3.0, head_d=cover_head_d, head_h=cover_head_h);
    }

    // Tornillos Waveshare: entran por atrás y atornillan a standoff/PCB
    for(p=ws_pts){
      translate([p[0], p[1], cover_z0 - cover_mount_head_h])
        screw_dummy(len=ws_standoff_h + 16, shank_d=3.0, head_d=cover_mount_head_d, head_h=cover_mount_head_h);
    }

    // Tornillos lector
    for(p=reader_pts){
      translate([p[0], p[1], cover_z0 - cover_mount_head_h])
        screw_dummy(len=18, shank_d=3.0, head_d=cover_mount_head_d, head_h=cover_mount_head_h);
    }

    // Tornillos de pared (visual) para checar keyhole
    if (DEBUG_SHOW_WALL_SCREWS && wall_ear_enabled){
      cover_w = body_w - 2*(wall_th + lip_clear);
      ear_x = cover_w/2 - wall_ear_inset_x;
      ear_y = wall_ear_offset_y;

      for(sx=[-1,1]){
        // vástago apunta hacia +Z y "llega" a cover_z0
        translate([sx*ear_x, ear_y, cover_z0 - wall_screw_len])
          union(){
            cylinder(d=wall_screw_shank_d, h=wall_screw_len); // termina en cover_z0
            translate([0,0,-wall_screw_head_h])
              cylinder(d=wall_screw_head_d, h=wall_screw_head_h); // cabeza en pared
          }
      }
    }
  }
}

module debug_assembly(){
  front_shell();
  //back_cover_with_screws();
  if(DEBUG_SHOW_DUMMIES){
    waveshare_dummy();
    reader_dummy();
  }
}

// ======================= EJECUCIÓN =======================
if(DEBUG_ASSEMBLY){
  debug_assembly();
} else {
  // Exporta por separado:
  //front_shell();
  //translate([0,0,body_depth+2]) back_cover();
  front_shell();
}