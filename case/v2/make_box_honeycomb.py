"""
OBD Monitor case - COMPACT version (2 printed parts, no screws, snap-fit lid)
  2.4" SPI ILI9341 touch display (PCB 77.18 x 42.72), ESP32 DevKit 30-pin (51.5 x 28.3),
  SN65HVD230 CAN board (23.24 x 10.24), LM2596 buck (43.2 x 21 x 14), 12 mm buzzer,
  7 mm panel push button.

  ASSUMES WIRES ARE SOLDERED directly to the pins (no dupont plugs) - that is what
  makes it this thin. With dupont plugs set INNER_D = 42.

Edit the numbers below, then run:  python3 make_case.py
Outputs: case_back.stl, case_lid.stl      (needs: pip install manifold3d trimesh numpy)
"""
import math
import numpy as np
import trimesh
from manifold3d import Manifold

# ------------------------------------------------------------------ settings (mm)
PCB_W, PCB_H   = 42.72, 77.18   # display PCB (portrait)
PCB_TOL        = 0.6            # clearance around the display PCB
AA_W, AA_H     = 36.72, 48.96   # visible screen area
WIN_MARGIN     = 1.0
WIN_SHIFT_Y    = -5.5           # screen sits ~4 mm away from the pin-header end (header end = +Y, marked by the dot)

WALL           = 1.6
FLOOR          = 1.2
LID_T          = 1.6
LIP_T          = 1.2            # lid rim that goes inside the box and holds the display
LIP_H          = 5.5            # = display thickness (glass + PCB)
FIT            = 0.2            # gap between lid rim and box wall
INNER_D        = 41.8           # inside depth: room for dupont jumper plugs (soldered wires only: 32)
CORNER_R       = 3.0
SEG            = 64

# derived inside size: display + lid rim + fit gap
INNER_W = PCB_W + PCB_TOL + 2 * LIP_T + 2 * FIT
INNER_H = PCB_H + PCB_TOL + 2 * LIP_T + 2 * FIT

ESP_X          = -INNER_W / 2 + 0.3 + 28.3 / 2   # ESP32 against the left wall
USB_W, USB_H, USB_Z0 = 13.0, 10.0, 0.0           # ESP32 lies upside down, USB at the floor
CABLE_D        = 7.0                             # OBD cable, bottom wall, at the CAN terminal
CABLE_X, CABLE_Z = 13.0, 8.0
BUTTON_D       = 7.2                             # 7 mm panel push button, right wall
BUTTON_Y, BUTTON_Z = 4.0, 18.0
BUZZER_POS     = (13.0, 0.0)                     # centred between the two vent groups
SNAP_Y         = (-25.0, 25.0)                   # click bumps on the long sides
SNAP_DOWN      = 2.5                             # distance of the bumps below the lid plate
# ---------------------------------------------------------------------------------

OUT_W, OUT_H = INNER_W + 2 * WALL, INNER_H + 2 * WALL
BOX_D = FLOOR + INNER_D


def rounded_box(w, h, d, r, z0=0.0):
    r = max(r, 0.3)
    pts = [Manifold.cylinder(d, r, r, SEG).translate([sx * (w / 2 - r), sy * (h / 2 - r), z0])
           for sx in (-1, 1) for sy in (-1, 1)]
    return Manifold.batch_hull(pts)


def cyl(d, h, x, y, z=0.0):
    return Manifold.cylinder(h, d / 2, d / 2, SEG).translate([x, y, z])


def box(w, h, d, x=0.0, y=0.0, z=0.0):
    return Manifold.cube([w, h, d], True).translate([x, y, z + d / 2])


def rod_y(r, length, x, z):          # cylinder lying along Y, centred on y=0
    return Manifold.cylinder(length, r, r, SEG, True).rotate([90, 0, 0]).translate([x, 0, z])


# ================================================================== BACK BOX (print open side up)
back = rounded_box(OUT_W, OUT_H, BOX_D, CORNER_R) - rounded_box(INNER_W, INNER_H, INNER_D + 1, CORNER_R - WALL, FLOOR)

yb = -OUT_H / 2
back = back - Manifold.cylinder(WALL * 3, BUTTON_D / 2, BUTTON_D / 2, SEG).rotate([0, 90, 0]).translate([OUT_W / 2 - WALL * 1.5, BUTTON_Y, FLOOR + BUTTON_Z])

# (buzzer sits behind the honeycomb on the back - no separate grille needed)

# --- openings on 3 sides (landscape: lower edge = -X wall, left/right ends = +Y / -Y walls) ---
# every side gets one USB-C slot + one round OBD hole
HOLE_Z   = FLOOR + INNER_D * 0.55          # height in the wall
OBD_D    = 7.0                             # OBD cable hole
USB_W2, USB_H2 = 13.0, 7.5                 # USB-C plug opening (fits the plug's plastic body)
def usb_slot_end(x, ywall):                # slot in a +Y/-Y end wall, long axis along X
    return Manifold.batch_hull([Manifold.cylinder(WALL * 4, USB_H2 / 2, USB_H2 / 2, SEG, True).rotate([90, 0, 0]).translate([x + sx * (USB_W2 - USB_H2) / 2, ywall, HOLE_Z]) for sx in (-1, 1)])
def usb_slot_side(y):                      # slot in the -X wall, long axis along Y
    return Manifold.batch_hull([Manifold.cylinder(WALL * 4, USB_H2 / 2, USB_H2 / 2, SEG, True).rotate([0, 90, 0]).translate([-OUT_W / 2, y + sy * (USB_W2 - USB_H2) / 2, HOLE_Z]) for sy in (-1, 1)])
END_USB_X, END_OBD_X = -INNER_W / 2 + 8.5, -INNER_W / 2 + 21.0     # USB-C lowest, OBD hole above it
for sy in (-1, 1):
    yw = sy * (OUT_H / 2 - WALL / 2)
    back = back - Manifold.cylinder(WALL * 4, OBD_D / 2, OBD_D / 2, SEG, True).rotate([90, 0, 0]).translate([END_OBD_X, yw, HOLE_Z])
    back = back - usb_slot_end(END_USB_X, yw)
back = back - usb_slot_side(-9.0)                                                     # bottom edge, centre pair
back = back - Manifold.cylinder(WALL * 4, OBD_D / 2, OBD_D / 2, SEG, True).rotate([0, 90, 0]).translate([-OUT_W / 2 + WALL / 2, 9.0, HOLE_Z])
# back openings (through the floor), in the lower half when held landscape
BACK_X = -INNER_W / 2 + 11.0
back = back - Manifold.batch_hull([cyl(USB_H2, FLOOR * 4, BACK_X, -9.0 + sy * (USB_W2 - USB_H2) / 2, -FLOOR) for sy in (-1, 1)])
back = back - cyl(OBD_D, FLOOR * 4, BACK_X, 9.0, -FLOOR)
# ---------------- honeycomb (saves filament + ventilation) ----------------
HEX_AP, HEX_RIB = 5.0, 1.3                  # hex apothem (half width) and rib between hexes
def rrect(w, h, r, t):
    r = max(0.3, min(r, w / 2 - 0.01, h / 2 - 0.01))
    return Manifold.batch_hull([Manifold.cylinder(t, r, r, SEG, True).translate([sx * (w / 2 - r), sy * (h / 2 - r), 0]) for sx in (-1, 1) for sy in (-1, 1)])

def honeycomb(width, height, keepouts, margin=2.4):
    """honeycomb PANEL: rounded frame, full cells inside, edge cells clipped cleanly, slivers dropped"""
    a = 2 * HEX_AP + HEX_RIB
    R = HEX_AP / math.cos(math.pi / 6)
    region = rrect(width - 2 * margin, height - 2 * margin, 3.0, WALL * 8)
    for (u, v, hu, hv) in keepouts:      # rounded solid islands around openings
        region = region - rrect(2 * hu, 2 * hv, min(hu, hv), WALL * 10).translate([u, v, 0])
    full = Manifold.cylinder(WALL * 6, R, R, 6, True).rotate([0, 0, 30])
    fv = full.volume()
    cells = []
    rows = int(height / (a * 0.866)) + 2
    cols = int(width / a) + 2
    for j_ in range(-rows, rows + 1):
        for i_ in range(-cols, cols + 1):
            u = i_ * a + (j_ % 2) * a / 2
            v = j_ * a * math.sqrt(3) / 2
            if abs(u) > width / 2 + a or abs(v) > height / 2 + a: continue
            c = full.translate([u, v, 0]) ^ region
            if c.volume() >= 0.5 * fv:      # drop slivers -> clean, intentional edge
                cells.append(c)
    return Manifold.compose(cells) if cells else Manifold()

def place_end(m, sy):      # local u->x, v->z, w->y  on the +/-Y end walls
    return m.rotate([90, 0, 0]).translate([0, sy * (OUT_H / 2 - WALL / 2), 0])
def place_long(m, sx):     # local u->y, v->z, w->x  on the +/-X long walls
    return m.rotate([90, 0, 0]).rotate([0, 0, 90]).translate([sx * (OUT_W / 2 - WALL / 2), 0, 0])

V0, V1 = FLOOR + 0.8, BOX_D - 5.0           # keep the rim + click grooves solid
VC, VH = (V0 + V1) / 2, V1 - V0
K = 2.0                                     # solid border around each opening
end_keep = [(END_USB_X, HOLE_Z - VC, USB_W2 / 2 + K, USB_H2 / 2 + K), (END_OBD_X, HOLE_Z - VC, OBD_D / 2 + K, OBD_D / 2 + K)]
for sy in (-1, 1):
    back = back - place_end(honeycomb(INNER_W - 2, VH, end_keep).translate([0, VC, 0]), sy)
bot_keep = [(-9.0, HOLE_Z - VC, USB_W2 / 2 + K, USB_H2 / 2 + K), (9.0, HOLE_Z - VC, OBD_D / 2 + K, OBD_D / 2 + K)]
top_keep = [(BUTTON_Y, FLOOR + BUTTON_Z - VC, BUTTON_D / 2 + 3, BUTTON_D / 2 + 3)]
back = back - place_long(honeycomb(INNER_H - 2, VH, bot_keep).translate([0, VC, 0]), -1)
back = back - place_long(honeycomb(INNER_H - 2, VH, top_keep).translate([0, VC, 0]), 1)
back_keep = [(BACK_X, -9.0, USB_W2 / 2 + K, USB_H2 / 2 + K), (BACK_X, 9.0, OBD_D / 2 + K, OBD_D / 2 + K)]
back = back - honeycomb(OUT_W - 2, OUT_H - 2, back_keep, margin=3.0).translate([0, 0, FLOOR / 2])

for sy in SNAP_Y:                                        # grooves for the lid click bumps
    for sx in (-1, 1):
        back = back - rod_y(0.95, 9.0, sx * INNER_W / 2, BOX_D - SNAP_DOWN).translate([0, sy, 0])

# ================================================================== LID (print face down)
lid = rounded_box(OUT_W, OUT_H, LID_T, CORNER_R)
lid = lid - box(AA_W + 2 * WIN_MARGIN, AA_H + 2 * WIN_MARGIN, LID_T * 3, 0, WIN_SHIFT_Y, -LID_T)
bevel = Manifold.batch_hull([
    box(AA_W + 2 * WIN_MARGIN, AA_H + 2 * WIN_MARGIN, 0.01, 0, WIN_SHIFT_Y, LID_T * 0.6),
    box(AA_W + 2 * WIN_MARGIN + 2.0, AA_H + 2 * WIN_MARGIN + 2.0, 0.01, 0, WIN_SHIFT_Y, -0.01),
])
lid = lid - bevel

lip_w, lip_h = INNER_W - 2 * FIT, INNER_H - 2 * FIT     # rim: fits inside the box, holds the display
rim = rounded_box(lip_w, lip_h, LIP_H, CORNER_R - WALL - FIT, LID_T) - box(PCB_W + PCB_TOL, PCB_H + PCB_TOL, LIP_H + 1, 0, 0, LID_T - 0.5)
for sy in (-1, 1):                                       # gaps in the short ends for display wires
    rim = rim - box(20, 4, LIP_H + 1, 0, sy * lip_h / 2, LID_T + 2.0)
lid = lid + rim
# orientation mark: raised dot on the inside of the lid at the PIN-HEADER end
lid = lid + cyl(3.0, 0.8, -(PCB_W / 2 - 4), (PCB_H + PCB_TOL) / 2 - 2.5, LID_T)   # beyond the glass, under the bare PCB end
for sy in SNAP_Y:                                        # click bumps
    for sx in (-1, 1):
        lid = lid + rod_y(0.7, 8.0, sx * lip_w / 2, LID_T + SNAP_DOWN).translate([0, sy, 0])


def save(m, name):
    mesh = m.to_mesh()
    t = trimesh.Trimesh(vertices=np.asarray(mesh.vert_properties)[:, :3], faces=np.asarray(mesh.tri_verts))
    t.export(name)
    ext = t.bounds[1] - t.bounds[0]
    print(f"{name}: {ext[0]:.1f} x {ext[1]:.1f} x {ext[2]:.1f} mm, plastic {t.volume / 1000:.1f} cm3, watertight={t.is_watertight}")
    return t


if __name__ == "__main__":
    print(f"inside: {INNER_W:.1f} x {INNER_H:.1f} x {INNER_D:.1f} mm")
    save(back, "case_back.stl")
    save(lid, "case_lid.stl")
