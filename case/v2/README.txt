OBD Monitor case - v2   (saved 2026-10-13 15:14, Israel time)
================================================================
Print ONE box + the lid.

lid_shift5.5.stl        Lid with screen window shifted 5.5 mm away from the pin-header end.
                        TESTED - display fits and window lines up. Print face down.
box_original_vents.stl  4.5 cm box with slot vents (approved design).
                        49.3 x 83.8 x 43.4 mm, about 9.1 m filament.
box_honeycomb.stl       Same 4.5 cm box with honeycomb walls (light version).
                        49.3 x 83.8 x 43.0 mm, about 5.1 m filament.

Both boxes: USB-C (low) + OBD cable hole (above) on the left and right ends,
USB-C + OBD pair centred in the bottom edge, USB-C + OBD pair in the back, buzzer grille.
Room for dupont jumper plugs (10 cm jumpers, not cut). Lid snaps on, no screws.

Scripts (python3, needs: pip install manifold3d trimesh numpy):
  make_lid.py            -> lid   (WIN_SHIFT_Y = -5.5)
  make_box_vents.py      -> vents box
  make_box_honeycomb.py  -> honeycomb box
