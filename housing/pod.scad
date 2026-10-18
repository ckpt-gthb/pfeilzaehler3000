// PfeilZaehler3000 pod housing v3 – free-turning mount bolt, pod stays aligned
// (c) 2026 ckpt-gthb – CC BY-NC-ND 4.0, no commercial use (see LICENSE-hardware)
// ---------------------------------------------------------------------------
//  The 5/16"-24 socket head bolt turns freely in its own column at one end of the pod.
//  Hold the pod straight on the riser and tighten the bolt with a 1/4" hex key through
//  the hole in the lid – the pod clamps in exactly the alignment you hold it.
//
//  base  : bolt column, 4 screw bosses for the ADXL375,
//          cradle (4 pads + corner guides + end stop) for the XIAO
//  frame : drops onto the post shoulders, fingers press the XIAO into its cradle,
//          battery lies on top of it
//  lid   : battery pocket whose walls clamp the frame when the lid is screwed on, hex key hole
//
// Print base + lid in PETG-CF, frame in plain PETG; 0.2 mm, 4 walls, no supports. Base: 100 % rectilinear infill (gyroid leaves a line at ~7 mm); frame + lid: 40 % gyroid.
//   base: open side up   frame: fingers up   lid: flat face down
// Hardware: 8x M2 brass heat-set inserts + 8x M2x12 countersunk screws (4 hold the ADXL375, 4 hold the lid),
//           5/16"-24 UNF socket head cap screw 5/8" (16 mm), 1 mm foam pad on the battery.
// Insert holes are set by ins_d / ins_l below – PfeilZaehler3000-insert-test.stl (part "test") has 2.7–3.0 mm holes.
//
// part = "base" | "frame" | "lid" | "lid_inlay" | "test" | "plate" (all three, print layout)
//        "washer" (TPU, between pod and riser: stops the pod from turning)
//        "assembly" | "exploded" | "topview" (coloured previews with dummy boards)
part = "plate";

// ---------------- main dimensions (mm) ----------------
inner_l = 60;   inner_w = 30;   inner_h = 16;
wall = 2.4;     floor_t = 8;    lid_t = 3.0;   corner_r = 3;

// ---------------- boards ----------------
adxl_l = 25.4;  adxl_w = 17.78; adxl_pcb = 1.6;          // Adafruit 5374
adxl_hole_dx = 12.7; adxl_hole_dy = 20.32;               // hole grid (short x long)
adxl_standoff = 3;                                       // M2 insert reaches down into the floor
// ---------------- mount bolt column (at the -X end) ----------------
bolt_d    = 8.4;                 // 5/16" shank clearance
col_bore  = 12.7;                // socket head is 11.9 mm (0.469")
col_wall  = 1.6;
col_od    = col_bore + 2*col_wall;
col_x     = -inner_l/2 + col_od/2 - 1;   // tube merges into the end wall
col_floor = 5;                   // plastic under the bolt head (clamped against the riser)
key_hole  = 8.5;                 // lid hole for a 1/4" hex key
adxl_cx = col_x + col_od/2 + 1.2 + adxl_w/2;              // board long axis along Y
xiao_l = 21;    xiao_w = 17.8;  xiao_pcb = 1.2;          // Seeed XIAO nRF52840 Sense
xiao_z  = 2.5;                                           // PCB underside above floor (room for BAT wires)
xiao_gap = 1.3;                                          // PCB end to inner wall (USB-C sticks out ~1 mm)
xiao_x1 = inner_l/2 - xiao_gap;  xiao_x0 = xiao_x1 - xiao_l;
usb_h_above_pcb = 3.2;                                   // USB-C receptacle height

bolt_x = col_x;

// ---------------- frame / battery ----------------
frame_z = 8;       frame_t = 1.6;                        // frame underside above floor
bat_l = 35.5;  bat_w = 25.5;                             // pocket for a xx2535 cell (25 x 35 mm) + clearance
cell_t = 4.0;  foam_t = 1.0;                             // 402535 = 4.0 mm thick cell, 1 mm foam pad
lid_pocket_h = inner_h - frame_z - frame_t;               // pocket walls reach down and clamp the frame
bat_x = 6.3;                                             // battery between bolt column and +X posts

// ---------------- USB-C slot ----------------
usb_w = 12.5;  usb_hh = 7;
usb_z = xiao_z + xiao_pcb + 1.63;                        // receptacle centre above floor

// ---------------- screws / posts ----------------
post_inset = 1.2;  post_low_d = 7;  post_up_d = 6;
// M2 heat-set inserts from the M2/M3 300 pcs kit: use M2 x 4 x 3.2 (thread x length x outer diameter)
ins_d = 2.9;       ins_l = 4.5;        // hole = OD - 0.3 mm; depth = 4 mm insert + 0.5 mm
m2_clear = 2.3;                        // hole below the insert, room for the screw tip
screw_l = 12;                          // M2x12 countersunk (DIN 965 / ISO 7046, 90°, head ~3.8 mm)
lid_hole = 2.4;    head_d = 4.1;          // countersink diameter at the lid surface
csk_h = (head_d - lid_hole)/2;            // 90° cone depth
$fn = 48;

outer_l = inner_l + 2*wall;  outer_w = inner_w + 2*wall;  zf = floor_t;
posts = [for (x=[-1,1], y=[-1,1]) [x*(inner_l/2-post_inset), y*(inner_w/2-post_inset)]];
adxl_holes = [for (x=[-1,1], y=[-1,1]) [adxl_cx + x*adxl_hole_dx/2, y*adxl_hole_dy/2]];
pillars = [for (x=[-1,1], y=[-1,1]) [x*(inner_l/2-4.2), y*(inner_w/2-5.2)]];

module rbox(l, w, h, r) {
  hull() for (x=[-1,1], y=[-1,1]) translate([x*(l/2-r), y*(w/2-r), 0]) cylinder(r=r, h=h);
}

// ======================= BASE =======================
module base() {
  difference() {
    union() {
      difference() {
        rbox(outer_l, outer_w, zf + inner_h, corner_r);
        translate([0,0,zf]) rbox(inner_l, inner_w, inner_h+1, 1);
      }
      // corner posts: wide up to the frame shoulder, slim above
      for (p = posts) translate([p[0], p[1], zf-0.01]) {
        cylinder(d=post_low_d, h=frame_z+0.01);
        cylinder(d=post_up_d, h=inner_h+0.01);
      }
      // ADXL375 standoffs
      for (h = adxl_holes) translate([h[0], h[1], zf-0.01]) cylinder(d=6.2, h=adxl_standoff+0.01);
      // XIAO cradle: 4 support pads under the PCB corners
      for (x=[xiao_x0+1.5, xiao_x1-2.5], y=[-1,1])
        translate([x-1.5, y*(xiao_w/2-1.2)-1.5, zf-0.01]) cube([3, 3, xiao_z+0.01]);
      // corner guides that stop sideways movement (gap in the middle for wires)
      for (x=[xiao_x0, xiao_x1-3.5], y=[-1,1])
        translate([x, y>0 ? xiao_w/2+0.2 : -xiao_w/2-0.2-1.2, zf-0.01]) cube([3.5, 1.2, xiao_z+xiao_pcb+1.2]);
      // end stop behind the XIAO (pushes the USB-C into the wall slot)
      translate([xiao_x0-0.2-1.4, -4, zf-0.01]) cube([1.4, 8, xiao_z+xiao_pcb+1.5]);
      // bolt column tube, full height, keeps the hex key away from the electronics
      translate([col_x, 0, zf-0.01]) cylinder(d=col_od, h=inner_h+0.01);
    }
    // bolt column: bore for the socket head down to col_floor, shank hole through the bottom
    translate([col_x, 0, col_floor]) cylinder(d=col_bore, h=zf+inner_h);
    translate([col_x, 0, -1]) cylinder(d=bolt_d, h=zf+2);
    // USB-C slot, rounded
    translate([outer_l/2 - wall - 1, 0, zf + usb_z]) rotate([0,90,0])
      hull() for (s=[-1,1]) translate([0, s*(usb_w/2-usb_hh/2), 0]) cylinder(d=usb_hh, h=wall+2);
    // pilot holes
    // M2 inserts in the post tops (lid screws)
    for (p = posts) translate([p[0], p[1], zf+inner_h-ins_l]) cylinder(d=ins_d, h=ins_l+1);
    for (p = posts) translate([p[0], p[1], zf+inner_h-(screw_l-lid_t)-1]) cylinder(d=m2_clear, h=inner_h);   // countersunk: whole length counts
    // M2 inserts in the ADXL375 bosses; screw tip may go on into the floor
    for (h = adxl_holes) translate([h[0], h[1], zf+adxl_standoff-ins_l]) cylinder(d=ins_d, h=ins_l+1);
    for (h = adxl_holes) translate([h[0], h[1], -1]) cylinder(d=m2_clear, h=zf+adxl_standoff);   // screw tip ends ~0.6 mm above the bottom face
    // grip ribs under the bolt
    for (a=[0:30:150]) translate([bolt_x,0,-0.01]) rotate([0,0,a]) translate([-9,-0.4,0]) cube([18,0.8,0.4]);
  }
}

// ======================= FRAME =======================
// modelled in its installed position (underside at z=0 = frame_z above floor)
module frame() {
  union() {
    difference() {
      rbox(inner_l-0.8, inner_w-0.8, frame_t, 1);
      for (p = posts) translate([p[0], p[1], -1]) cylinder(d=post_up_d+0.7, h=frame_t+2);
      // wire pass-through for battery + sensor wires (next to the XIAO)
      translate([xiao_x0+2, inner_w/2-4, -1]) cube([12, 5, frame_t+2]);
      translate([xiao_x0+2, -inner_w/2-1, -1]) cube([12, 5, frame_t+2]);
      // clearance around the bolt column
      translate([col_x, 0, -1]) cylinder(d=col_od+0.8, h=frame_t+2);
      // window over the ADXL375 chip (it is screwed down, needs no pressure)
      translate([adxl_cx, 0, -1]) rbox(10, 14, frame_t+2, 2);
    }
    // fingers pressing the XIAO PCB corners at the far end
    for (y=[-1,1]) translate([xiao_x0+1.6, y*(xiao_w/2-1.4), -(frame_z-xiao_z-xiao_pcb)])
      cylinder(d=2.4, h=frame_z-xiao_z-xiao_pcb+0.01);
    // finger pressing the USB-C shell
    translate([xiao_x1-4, 0, -(frame_z-xiao_z-xiao_pcb-usb_h_above_pcb)])
      translate([-2.5,-3,0]) cube([4, 6, frame_z-xiao_z-xiao_pcb-usb_h_above_pcb+0.01]);
  }
}

// ======================= LID ART =======================
// "PFEILZÄHLER 3000": an arrow flying into a bullseye; the hex key hole is the bullseye.
art_depth = 1.2;                       // 6 layers at 0.2 mm, enough for a light colour to cover a dark lid
art_font  = "Liberation Sans:style=Bold";
module lid_art_2d() {
  cx = col_x;
  // bullseye rings around the key hole
  for (ring = [[5.8, 6.5], [8.0, 8.7]]) translate([cx, 0]) difference() { circle(r=ring[1], $fn=96); circle(r=ring[0], $fn=96); }
  // arrow: tip stuck in the outer ring, shaft to the right, fletching at the end
  tip_x = cx + 8.5;  tail_x = 27.5;
  translate([tip_x, 0]) polygon([[0,0],[3.4,1.7],[3.4,-1.7]]);                // point
  translate([tip_x+3.3, -0.45]) square([tail_x-tip_x-3.3-4, 0.9]);          // shaft
  for (i=[0:2]) translate([tail_x-4.2+i*1.5, 0])                           // fletching
    for (s=[-1,1]) polygon([[0, s*0.3],[1.1, s*0.3],[2.3, s*2.2],[1.2, s*2.2]]);
  // text
  tx = (tip_x + tail_x)/2 + 0.5;
  translate([tx, 3.4])  text("PFEILZÄHLER", size=4.3, font=art_font, halign="center", valign="bottom");
  translate([tx, -3.4]) text("3000", size=6.0, font=art_font, halign="center", valign="top", spacing=1.08);
}
// The lid prints face down and is flipped over (about the X axis) onto the pod,
// so the art is mirrored in Y here to read correctly on the finished part.
module lid_art() translate([0,0,-0.01]) linear_extrude(art_depth+0.01) mirror([0,1,0]) lid_art_2d();

// ======================= LID =======================
// modelled flat face down (z=0 = outside face); installed it is flipped
// lid() = engraved lid (single colour); lid_inlay() = the art as a separate 2nd-colour part
module lid_inlay() intersection() { lid_plain(); lid_art(); }
module lid() difference() { lid_plain(); lid_art(); }
module lid_plain() {
  difference() {
    union() {
      rbox(outer_l, outer_w, lid_t, corner_r);
      // locating lip
      translate([0,0,lid_t-0.01]) difference() {
        rbox(inner_l-0.6, inner_w-0.6, 2, 1);
        translate([0,0,-1]) rbox(inner_l-3.6, inner_w-3.6, 4, 0.5);
        for (p = posts) translate([p[0], p[1], -1]) cylinder(d=post_up_d+3, h=4);
        translate([col_x, 0, -1]) cylinder(d=col_od+1, h=4);
      }
      // battery pocket walls
      translate([bat_x,0,lid_t-0.01]) difference() {
        rbox(bat_l+2.4, bat_w+2.4, lid_pocket_h, 1.5);
        translate([0,0,-1]) rbox(bat_l, bat_w, lid_pocket_h+2, 0.3);
        translate([-4, bat_w/2-1, -1]) cube([8, 4, lid_pocket_h+2]);   // wire exit
      }

    }
    // hex key access to the mount bolt
    translate([col_x, 0, -1]) cylinder(d=key_hole, h=lid_t+2);
    translate([col_x, 0, -0.01]) cylinder(d1=key_hole+1.6, d2=key_hole, h=0.8);
    for (p = posts) translate([p[0], p[1], -1]) cylinder(d=lid_hole, h=lid_t+2);
    for (p = posts) translate([p[0], p[1], -0.01]) { cylinder(d=head_d, h=0.2); translate([0,0,0.19]) cylinder(d1=head_d, d2=lid_hole, h=csk_h); }   // 90° countersink, head sits flush
  }
}

// ======================= DUMMY BOARDS (preview only) =======================
module adxl_dummy() {
  translate([adxl_cx, 0, zf+adxl_standoff]) {
    color("#1b2a55") difference() {
      translate([-adxl_w/2, -adxl_l/2, 0]) cube([adxl_w, adxl_l, adxl_pcb]);
      for (h = adxl_holes) translate([h[0]-adxl_cx, h[1], -1]) cylinder(d=2.5, h=4);
    }
    color("#222") translate([-1.5,-1.5,adxl_pcb]) cube([3,3,1]);                      // ADXL375 chip
    for (s=[-1,1]) color("#e8e2d0") translate([-3, s*(adxl_l/2-2.6)-2, adxl_pcb]) cube([6,4,2.9]); // JST SH
    for (i=[0:5]) color("#c9a227") translate([adxl_w/2-2.54, -6.35+i*2.54, adxl_pcb]) cylinder(d=1.6,h=0.1);
  }
}
module xiao_dummy() {
  translate([xiao_x0, -xiao_w/2, zf+xiao_z]) {
    color("#1f5f3f") cube([xiao_l, xiao_w, xiao_pcb]);
    color("#b8bcc2") translate([6, 2.5, xiao_pcb]) cube([9, 12, 1.4]);                // shield can
    color("#d8dbe0") translate([xiao_l-7.3+1.0, xiao_w/2-4.5, xiao_pcb]) cube([7.3, 9, usb_h_above_pcb]);  // USB-C
    for (i=[0:6], s=[0,1]) color("#c9a227") translate([2+i*2.54-0.6, s*(xiao_w-1), xiao_pcb]) cube([1.2,1,0.05]);
  }
}
module battery_dummy() {
  color("#c7ccd3") translate([bat_x-35/2, -25/2, zf+frame_z+frame_t]) cube([35, 25, cell_t]);
  color("#f2d6a2") translate([bat_x-35/2, -25/2, zf+frame_z+frame_t+cell_t]) cube([35, 25, foam_t]);    // foam pad
}
module nut_stud_dummy() {   // 5/16"-24 x 5/8" socket head cap screw
  color("#9aa0a6") difference() {
    translate([bolt_x,0,col_floor]) cylinder(d=11.9, h=7.94);
    translate([bolt_x,0,col_floor+3.5]) cylinder(d=6.35/cos(30), h=5, $fn=6);
  }
  color("#8a9096") translate([bolt_x,0,col_floor-15.9]) cylinder(d=7.9, h=15.9);
}

module installed_frame() translate([0,0,zf+frame_z]) frame();
module installed_lid()   translate([0,0,zf+inner_h+lid_t]) rotate([180,0,0]) lid();   // real flip, not a mirror

// ======================= OUTPUT =======================
module insert_test() {
  difference() {
    translate([-14,-5,0]) cube([28,10,7]);
    for (i=[0:3]) translate([-10.5+i*7, 0, 7-ins_l]) cylinder(d=[2.7,2.8,2.9,3.0][i], h=ins_l+1);
    for (i=[0:3]) translate([-10.5+i*7, 0, -1]) cylinder(d=m2_clear, h=8);
    for (i=[0:3]) translate([-10.5+i*7-1.5, -4.6, 7-0.5]) linear_extrude(1) text(str([27,28,29,30][i]), size=2.4, font="Liberation Sans:style=Bold");
  }
}
// ======================= WASHER (2.3) =======================
// Thin TPU ring between the pod and the riser around the bolt: grips both, so the release jolts
// cannot turn the pod on its bolt. Print in TPU (95A), 100 % infill; or use a rubber washer of the
// same size (EPDM / neoprene, M8: 8.4 x 18 x 1 mm).
washer_od = 18;  washer_id = 8.6;  washer_t = 1.0;
module washer() difference() {
  cylinder(d=washer_od, h=washer_t, $fn=96);
  translate([0,0,-1]) cylinder(d=washer_id, h=washer_t+2, $fn=64);
}

if (part == "washer") washer();
if (part == "test")  insert_test();
if (part == "base")  base();
if (part == "frame") translate([0,0,frame_t]) mirror([0,0,1]) frame();   // print fingers up
if (part == "lid")   lid();
if (part == "lid_inlay") lid_inlay();
if (part == "plate") {
  base();
  translate([0, outer_w + 6, frame_t]) mirror([0,0,1]) frame();
  translate([0, -outer_w - 6, 0]) lid();
}
if (part == "assembly") {
  color("#e0a91b", 0.35) base();
  adxl_dummy(); xiao_dummy(); nut_stud_dummy();
  color("#2f7fb8") installed_frame();
  battery_dummy();
}
if (part == "exploded") {
  color("#e0a91b") base();
  adxl_dummy(); xiao_dummy(); nut_stud_dummy();
  translate([0,0,16]) color("#2f7fb8") installed_frame();
  translate([0,0,24]) battery_dummy();
  translate([0,0,44]) color("#e0a91b", 0.9) installed_lid();
}
if (part == "topview") {
  color("#e0a91b") base();
  adxl_dummy(); xiao_dummy();
}
