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
FLOOR          = 1.6
LID_T          = 1.6
LIP_T          = 1.2            # lid rim that goes inside the box and holds the display
LIP_H          = 5.5            # = display thickness (glass + PCB)
FIT            = 0.2            # gap between lid rim and box wall
INNER_D        = 46.8           # inside depth: room for dupont jumper plugs (soldered wires only: 32)
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
BUZZER_POS     = (13.0, 12.0)                    # grille on the back (clear of the CAN board plugs)
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
back = back - box(USB_W, WALL * 3, USB_H, ESP_X, yb, FLOOR + USB_Z0)
back = back - Manifold.cylinder(WALL * 3, CABLE_D / 2, CABLE_D / 2, SEG).rotate([90, 0, 0]).translate([CABLE_X, yb + WALL * 1.5, FLOOR + CABLE_Z])
back = back - Manifold.cylinder(WALL * 3, BUTTON_D / 2, BUTTON_D / 2, SEG).rotate([0, 90, 0]).translate([OUT_W / 2 - WALL * 1.5, BUTTON_Y, FLOOR + BUTTON_Z])

bx, by = BUZZER_POS
for i in range(-2, 3):
    for j in range(-2, 3):
        hx, hy = bx + i * 3.2 + (j % 2) * 1.6, by + j * 2.8
        if (hx - bx) ** 2 + (hy - by) ** 2 <= 5.6 ** 2:
            back = back - cyl(1.8, FLOOR * 3, hx, hy, -FLOOR)

for k in range(4):                                       # vents in the top wall, above the buck
    back = back - box(6, WALL * 3, 1.8, -12 + k * 8, OUT_H / 2, FLOOR + 19)

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
