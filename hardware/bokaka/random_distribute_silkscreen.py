import pcbnew
import random

board = pcbnew.GetBoard()

# ---------------- PARAMETERS ----------------
rect_center_x_mm = 146.0
rect_center_y_mm = 87.0
rect_width_mm    = 110.0
rect_height_mm   = 70.0

seed = None  # set int for repeatability, e.g. 123

# How "random" within each grid cell (0.0 = dead center, 0.5 = up to half cell)
jitter_frac = 0.35

# Minimum extra spacing between footprints (mm), on top of footprint size
margin_mm = 0.8

# Placement attempts per footprint before giving up
attempts_per_item = 80

# Rotation
enable_rotation = True
snap_rotation   = True
snap_step_deg   = 15.0     # 15/30/45/90 etc
rot_min_deg     = 0.0
rot_max_deg     = 360.0
# -------------------------------------------

def mm_to_nm(mm: float) -> int:
    return int(round(mm * 1_000_000))

def nm_to_mm(nm: int) -> float:
    return nm / 1_000_000.0

def make_angle_deg(deg: float):
    # KiCad 9 expects EDA_ANGLE
    return pcbnew.EDA_ANGLE(deg)

def random_rotation_deg():
    if not enable_rotation:
        return None
    if snap_rotation:
        steps = int(round(360.0 / snap_step_deg))
        return random.randint(0, steps - 1) * snap_step_deg
    return random.uniform(rot_min_deg, rot_max_deg)

def approx_radius_nm(fp) -> int:
    """
    Approximate footprint as a circle radius based on its current bbox diagonal/2.
    This is conservative enough to keep spacing.
    """
    bb = fp.GetBoundingBox()  # EDA_RECT, in nm
    w = bb.GetWidth()
    h = bb.GetHeight()
    diag = int(round(math.sqrt(w*w + h*h)))
    return diag // 2

# --- Collect selected footprints ---
fps = [fp for fp in board.GetFootprints() if fp.IsSelected()]
if not fps:
    raise RuntimeError("No footprints selected. Select footprints first.")

n = len(fps)

if seed is not None:
    random.seed(seed)

# --- Rectangle bounds in nm ---
cx = mm_to_nm(rect_center_x_mm)
cy = mm_to_nm(rect_center_y_mm)
half_w = mm_to_nm(rect_width_mm / 2.0)
half_h = mm_to_nm(rect_height_mm / 2.0)

xmin, xmax = cx - half_w, cx + half_w
ymin, ymax = cy - half_h, cy + half_h

rect_w_nm = xmax - xmin
rect_h_nm = ymax - ymin

# --- Build a grid of cell centers ---
# Choose grid dimensions close to square while fitting n cells.
cols = max(1, int(round(math.sqrt(n * (rect_w_nm / max(1, rect_h_nm))))))
rows = int(math.ceil(n / cols))
# if too many rows, adjust
while rows * cols < n:
    rows += 1

cell_w = rect_w_nm / cols
cell_h = rect_h_nm / rows

# Create candidate cell centers (more than needed if rows*cols > n)
cells = []
for r in range(rows):
    for c in range(cols):
        x0 = xmin + int(round((c + 0.5) * cell_w))
        y0 = ymin + int(round((r + 0.5) * cell_h))
        cells.append((x0, y0))

# Shuffle cell order for randomness
random.shuffle(cells)
cells = cells[:n]  # take first n cells

# Shuffle footprints too (so refs don't bias spatial pattern)
random.shuffle(fps)

# --- Precompute spacing radii ---
margin_nm = mm_to_nm(margin_mm)
radii = [approx_radius_nm(fp) + margin_nm for fp in fps]

# --- Helper: validate spacing against already placed points ---
placed = []  # list of tuples: (x, y, r_nm)
def ok_spacing(x, y, r_nm):
    for (px, py, pr) in placed:
        dx = x - px
        dy = y - py
        # Compare squared distances
        min_d = r_nm + pr
        if dx*dx + dy*dy < min_d*min_d:
            return False
    return True

# --- Place footprints ---
failed = 0
for i, (fp, (cx_cell, cy_cell), r_nm) in enumerate(zip(fps, cells, radii)):
    # jitter range inside cell
    jx = int(round(cell_w * jitter_frac))
    jy = int(round(cell_h * jitter_frac))

    success = False
    for _ in range(attempts_per_item):
        x = cx_cell + random.randint(-jx, jx)
        y = cy_cell + random.randint(-jy, jy)

        # Clamp to rectangle bounds (keep anchors inside)
        x = max(xmin, min(xmax, x))
        y = max(ymin, min(ymax, y))

        if ok_spacing(x, y, r_nm):
            new_pos = pcbnew.VECTOR2I(int(x), int(y))

            # Move footprint anchor
            old_pos = fp.GetPosition()
            fp.Move(pcbnew.VECTOR2I(new_pos.x - old_pos.x, new_pos.y - old_pos.y))

            # Rotate around its new anchor
            deg = random_rotation_deg()
            if deg is not None:
                fp.Rotate(new_pos, make_angle_deg(deg))

            placed.append((new_pos.x, new_pos.y, r_nm))
            success = True
            break

    if not success:
        failed += 1

pcbnew.Refresh()
print(f"Placed {n - failed}/{n} footprints with spacing. Failed: {failed}.")
if failed:
    print("Tip: increase rect size, reduce margin_mm, reduce jitter_frac, or increase attempts_per_item.")