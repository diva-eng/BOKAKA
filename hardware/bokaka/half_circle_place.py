# KiCad pcbnew scripting console script:
# Place selected footprints on an arc (typically half circle) with:
# - Natural sorting by silkscreen reference (GetReference): D1, D2, D10...
# - LEFT = smallest reference, RIGHT = largest
# - Optional tangential rotation
# - Optional "padding" at the start/end so it doesn't occupy the full 180°

import pcbnew
import math
import re

board = pcbnew.GetBoard()

# -------------------- PARAMETERS --------------------
center_x_mm = 145.0      # arc center X (mm)
center_y_mm = 123.0      # arc center Y (mm)
radius_mm   = 51.0      # radius (mm)

# Base arc (a half circle is 0->180)
base_start_deg = 0.0
base_end_deg   = 180.0

# Padding at each end of the base arc (degrees).
# Example: 8° means you leave 8° empty at the start and 8° empty at the end.
pad_start_deg = 8.0
pad_end_deg   = 8.0

# Geometry / direction controls
flip_y = True           # True fixes common "upside down" placement (invert Y)
clockwise = False       # If True, reverse the angle progression direction

# Orientation controls
tangent_rotate = True   # True: rotate footprints tangentially along the arc
tangent_sign = +1       # +1 => angle+90,  -1 => angle-90 (flip tangent direction)
rot_offset_deg = 0.0    # extra rotation tweak if your footprint's 0° is different

# Sorting / mapping controls
left_smallest = True    # True: leftmost position gets smallest reference (D1)
# ----------------------------------------------------


def mm_to_nm(mm: float) -> int:
    return int(round(mm * 1_000_000))

def natural_key(s: str):
    return [int(t) if t.isdigit() else t.lower() for t in re.split(r"(\d+)", s)]

# Collect selected footprints
selected = [fp for fp in board.GetFootprints() if fp.IsSelected()]
if len(selected) < 2:
    raise RuntimeError("Select at least 2 footprints before running this script.")

# Sort by silkscreen reference (usually the visible ref text)
selected.sort(key=lambda f: natural_key(f.GetReference()))

n = len(selected)

# Compute the effective arc after padding
start_deg = base_start_deg + pad_start_deg
end_deg   = base_end_deg   - pad_end_deg

if end_deg <= start_deg:
    raise ValueError(
        f"Padding too large: effective arc is {end_deg - start_deg}°. "
        "Reduce pad_start_deg / pad_end_deg."
    )

# Distribute footprints across the effective arc
angles = [start_deg + (end_deg - start_deg) * i / (n - 1) for i in range(n)]

# Apply direction controls on the angle sequence
if clockwise:
    angles = list(reversed(angles))

# Map references to left->right visual order
# If your mapping ends up flipped, toggling left_smallest fixes it.
if left_smallest:
    angles = list(reversed(angles))

cx = mm_to_nm(center_x_mm)
cy = mm_to_nm(center_y_mm)
r  = mm_to_nm(radius_mm)

for fp, deg in zip(selected, angles):
    rad = math.radians(deg)

    # Position on arc
    x = cx + int(round(r * math.cos(rad)))
    s = math.sin(rad)
    if flip_y:
        s = -s
    y = cy + int(round(r * s))

    fp.SetPosition(pcbnew.VECTOR2I(x, y))

    # Orientation
    if tangent_rotate:
        rot = deg + tangent_sign * 90.0 + rot_offset_deg
        fp.SetOrientationDegrees(rot)
    else:
        fp.SetOrientationDegrees(rot_offset_deg)

pcbnew.Refresh()
print(
    f"Placed {n} footprints on effective arc {start_deg}°→{end_deg}° "
    f"(base {base_start_deg}°→{base_end_deg}°, padding {pad_start_deg}°/{pad_end_deg}°), "
    f"radius {radius_mm}mm."
)
