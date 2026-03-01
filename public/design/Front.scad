/*
  iVisor / Waveshare ESP32-S3 Touch LCD 7" - Wall Mount Enclosure (Horizontal)
  ✅ NO se mueve el LCD, NI la ventana del LCD, NI los holes/standoffs del Waveshare.
  ✅ Frente SIN tornillos visibles (Opción A):
      - Bezel SIN agujeros pasantes
      - Bezel con bosses + insertos térmicos M3 por dentro
      - Caja trasera con agujeros pasantes + rebaje de cabeza (desde atrás)
  ✅ Extensión hacia ABAJO para montar LECTOR centrado.
  ✅ Solo 2 orejas ARRIBA para pared.
  ✅ FIX: ventana interna para paso de cables entre sección superior (LCD) e inferior (lector).
  ✅ FIX VISUAL: contorno 100% continuo (una sola pieza) usando HULL 2D (sin “línea” de unión).
*/

$fn = 64;

// ======================= PARAMETROS PRINCIPALES (mm) =======================
panel_w = 192.96;   // ancho del frente (vidrio)
panel_h = 110.76;   // alto del frente (vidrio)

window_w = 154.88;  // ventana visible (LCD)
window_h = 86.72;
window_x = 19.04;
window_y = 13.04;

bezel_th = 3.0;
bezel_border = 12;
bezel_screw_inset = 8.0;     // inset en Y (arriba/abajo) para la tornillería de unión
bezel_screw_inset_x = 8.0;   // inset en X para la tornillería de unión (NO mover LCD)
bezel_lip = 1.5;
lip_depth = 2.0;

case_depth = 22.0;
wall_th = 2.4;
inner_clear = 1.0;

// ======================= OREJAS PARED (SOLO 2 ARRIBA) =======================
ear_enable = true;
ear_w = 16.0;
ear_h = 16.0;
ear_th = 4.0;
ear_hole_d = 5.5;
ear_offset = -14;

// ======================= TORNILLERIA UNION BEZEL <-> CAJA (OCULTA) =========
m3_clear_d = 3.4;     // pasante caja
m3_head_d  = 6.8;     // rebaje cabeza (pan/socket)
m3_head_h  = 2.2;     // profundidad rebaje cabeza

bezel_insert_od = 9.0;   // diámetro boss del bezel (robusto)
bezel_insert_h  = 8.0;   // altura boss (hacia adentro)
m3_inset_d  = 4.6;       // diámetro para inserto térmico M3

// ======================= CANAL USB-C (lado derecho, mirando de frente) =====
usb_slot_w = 40.0;
usb_slot_h = 10.0;
usb_slot_z = 8.0;

// ======================= WAVESHARE (NO TOCAR POSICIONES) ===================
dev_plate_w = 165.72;
dev_plate_h = 97.60;
dev_hole_x  = 63.10;
dev_hole_y_top = 30.49;
dev_hole_y_bot = -35.16;

dev_boss_od = 9.5;
dev_boss_h  = 8.0;

dev_back_head_d = 6.6;
dev_back_head_h = 2.2;

// ======================= LOGO =======================
logo_text = "Innovative Solutions";
logo_size = 10;
logo_depth = 0.8;

// ======================= EXTENSIÓN PARA LECTOR (ABAJO) =====================
bottom_extension_h = 75.0;  // cuánto baja el apéndice

// ⚠️ Para que se vea “una sola pieza”, usa el MISMO radio que el cuerpo:
corner_r = 5.0;
ext_corner_r = corner_r;

// Medidas lector
reader_body_w = 61.0;
reader_body_h = 58.0;
reader_hole_dx = 44.0;
reader_hole_dy = 51.0;
reader_hole_d  = 3.5;

// Ventana visible lector
reader_window_w = 52.0;
reader_window_h = 40.0;

// Montaje lector desde atrás (en back_case)
reader_boss_od = 9.5;
reader_boss_h  = 8.0;
reader_back_head_d = 6.6;
reader_back_head_h = 2.2;

// Posición del lector: centrado en extensión
reader_center_x = 0;
reader_center_y_offset = 0; // + sube, - baja dentro de extensión

// ======================= PASO DE CABLES ENTRE ARRIBA/ABAJO ==================
cable_pass_enable = true;
cable_pass_w = 30.0;     // ancho en X
cable_pass_h = 12.0;     // alto en Z
cable_pass_side = +1;    // +1 derecha, -1 izquierda, 0 centro
cable_pass_inset_x = 18; // qué tan adentro desde borde
cable_pass_center_z = 12.0;

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

// ✅ Contorno 2D CONTINUO (sin “línea” de unión) usando HULL entre cuerpo y extensión
module outline2d_continuo(ow, oh, ext_h, r_top, r_ext){
  ext_cy = -(oh/2 + ext_h/2);
  hull(){
    translate([0, 0])     rrect(ow, oh, r_top);
    translate([0, ext_cy]) rrect(ow, ext_h, r_ext);
  }
}

// Boss con inserto térmico (en bezel)
module insert_boss(x,y,z0, od, h, id){
  translate([x,y,z0])
    difference(){
      cylinder(d=od, h=h);
      cylinder(d=id, h=h+0.2);
    }
}

// Standoff para fijar módulo desde atrás (en caja)
module device_mount_standoff(x, y, od, h){
  translate([x, y, wall_th])
    difference(){
      cylinder(d=od, h=h);
      cylinder(d=m3_clear_d, h=h+0.6);
    }
}

// ======================= POSICIONES UNION OCULTA (NO MOVER LCD) ============
function outer_w() = panel_w + 2*bezel_border;
function outer_h() = panel_h + 2*bezel_border;

function ux(sx) = sx*(outer_w()/2 - bezel_screw_inset_x);
function uy(sy) = sy*(outer_h()/2 - bezel_screw_inset);

// ======================= FRONT BEZEL (SIN TORNILLOS VISIBLES) ===============
module front_bezel(){
  ow = outer_w();
  oh = outer_h();
  ext_cy = -(oh/2 + bottom_extension_h/2);

  difference(){
    // ✅ UNA SOLA PIEZA (contorno continuo) extruida una vez
    linear_extrude(height=bezel_th)
      outline2d_continuo(ow, oh, bottom_extension_h, corner_r, ext_corner_r);

    // Ventana LCD (NO tocar posiciones)
    translate([-(ow/2) + (bezel_border + window_x) + window_w/2,
               -(oh/2) + (bezel_border + window_y) + window_h/2,
               bezel_th/2])
      cube([window_w, window_h, bezel_th+0.2], center=true);

    // Rebaje interior para apoyar vidrio/panel (NO tocar)
    translate([0,0,bezel_th - lip_depth])
      difference(){
        rrect3(panel_w + 2*inner_clear, panel_h + 2*inner_clear, corner_r-1.0, lip_depth+0.2);
        rrect3(panel_w - 2*bezel_lip, panel_h - 2*bezel_lip, corner_r-2.0, lip_depth+0.4);
      }

    // Ventana del lector (SOLO en extensión)
    translate([reader_center_x, ext_cy + reader_center_y_offset, bezel_th/2])
      cube([reader_window_w, reader_window_h, bezel_th+0.2], center=true);

    // Logo visible al frente
    translate([0, -(oh/2) + 10, -logo_depth+0.05])
      mirror([1,0,0])
      linear_extrude(height=logo_depth)
        text(logo_text, size=logo_size, halign="center", valign="center",
             font="Liberation Sans:style=Bold");
  }

  // Bosses con insertos M3 por detrás (no visibles)
  for (sx=[-1,1], sy=[-1,1]){
    insert_boss(ux(sx), uy(sy), bezel_th, bezel_insert_od, bezel_insert_h, m3_inset_d);
  }
}

// ======================= BACK CASE (TORNILLOS ENTRAN DESDE ATRÁS) ===========
module back_case(){
  ow = outer_w();
  oh = outer_h();
  ext_cy = -(oh/2 + bottom_extension_h/2);

  y_join = -oh/2;

  x_pass =
    (cable_pass_side==0) ? 0 :
    (cable_pass_side>0)  ? ( ow/2 - wall_th - cable_pass_inset_x - cable_pass_w/2 ) :
                           (-ow/2 + wall_th + cable_pass_inset_x + cable_pass_w/2 );

  difference(){
    // ✅ UNA SOLA PIEZA (contorno continuo) extruida una vez
    linear_extrude(height=case_depth)
      outline2d_continuo(ow, oh, bottom_extension_h, corner_r, ext_corner_r);

    // Cavidad interior (original + extensión) también continua
    translate([0,0,wall_th])
      linear_extrude(height=case_depth - wall_th + 0.2)
        outline2d_continuo(ow - 2*wall_th, oh - 2*wall_th, bottom_extension_h - 2*wall_th, corner_r-1.0, ext_corner_r-1.0);

    // ✅ Ventana interna para PASO DE CABLES
    if (cable_pass_enable){
      translate([x_pass, y_join, cable_pass_center_z])
        cube([cable_pass_w, wall_th*3, cable_pass_h], center=true);
    }

    // Canal USB-C lateral derecho (original)
    translate([ow/2 - wall_th/2, 0, usb_slot_z])
      rotate([0,90,0])
        cylinder(d=usb_slot_h, h=wall_th+1.0);

    translate([ow/2 - wall_th/2, 0, usb_slot_z- usb_slot_h/2])
      cube([wall_th+1.0, usb_slot_w, usb_slot_h], center=true);

    translate([ow/2 - 8, 0, usb_slot_z- usb_slot_h/2])
      cube([16, usb_slot_w+6, case_depth], center=true);

    // =================== UNION OCULTA: agujeros desde ATRÁS =================
    for (sx=[-1,1], sy=[-1,1]){
      translate([ux(sx), uy(sy), -0.1])
        cylinder(d=m3_clear_d, h=wall_th + 0.8);
      translate([ux(sx), uy(sy), -0.1])
        cylinder(d=m3_head_d, h=m3_head_h + 0.2);
    }

    // =================== WAVESHARE: agujeros desde atrás (NO tocar) =========
    for (sx=[-1,1], sy=[1,-1])
      let(
        x = sx*dev_hole_x,
        y = (sy==1) ? dev_hole_y_top : dev_hole_y_bot
      ){
        translate([x, y, -0.1])
          cylinder(d=m3_clear_d, h=wall_th + 0.6);
        translate([x, y, -0.1])
          cylinder(d=dev_back_head_d, h=dev_back_head_h + 0.2);
      }

    // =================== LECTOR: agujeros desde atrás (en extensión) ========
    for (sx=[-1,1], sy=[-1,1]){
      x = reader_center_x + sx*(reader_hole_dx/2);
      y = (ext_cy + reader_center_y_offset) + sy*(reader_hole_dy/2);

      translate([x, y, -0.1])
        cylinder(d=reader_hole_d+0.3, h=wall_th + 0.6);
      translate([x, y, -0.1])
        cylinder(d=reader_back_head_d, h=reader_back_head_h + 0.2);
    }
  }

  // =================== STANDOFFS WAVESHARE (NO tocar) =======================
  device_mount_standoff( dev_hole_x,  dev_hole_y_top, dev_boss_od, dev_boss_h);
  device_mount_standoff(-dev_hole_x,  dev_hole_y_top, dev_boss_od, dev_boss_h);
  device_mount_standoff( dev_hole_x,  dev_hole_y_bot, dev_boss_od, dev_boss_h);
  device_mount_standoff(-dev_hole_x,  dev_hole_y_bot, dev_boss_od, dev_boss_h);

  // =================== STANDOFFS LECTOR (en extensión) ======================
  device_mount_standoff( reader_center_x + reader_hole_dx/2, (ext_cy + reader_center_y_offset) + reader_hole_dy/2, reader_boss_od, reader_boss_h);
  device_mount_standoff( reader_center_x - reader_hole_dx/2, (ext_cy + reader_center_y_offset) + reader_hole_dy/2, reader_boss_od, reader_boss_h);
  device_mount_standoff( reader_center_x + reader_hole_dx/2, (ext_cy + reader_center_y_offset) - reader_hole_dy/2, reader_boss_od, reader_boss_h);
  device_mount_standoff( reader_center_x - reader_hole_dx/2, (ext_cy + reader_center_y_offset) - reader_hole_dy/2, reader_boss_od, reader_boss_h);

  // =================== OREJAS SOLO ARRIBA (2) ===============================
  if (ear_enable){
    for (sx=[-1,1]){
      translate([
          sx*(ow/2 - ear_offset - ear_w/2 - 30),
          (oh/2 - ear_offset - ear_h/2),
          ear_th/2
        ])
        difference(){
          cube([ear_w, ear_h, ear_th], center=true);
          translate([0,0,-ear_th]) cylinder(d=ear_hole_d, h=ear_th*3);
        }
    }
  }
}

// ======================= ENSAMBLE / EXPORT =======================
// Si quieres exportar bezel:
front_bezel();
//translate([0,0,-case_depth-5])
// Si quieres exportar caja:
//back_case();

// Para ver ambas en preview:
// front_bezel();
// translate([0,0,-case_depth-5]) back_case();