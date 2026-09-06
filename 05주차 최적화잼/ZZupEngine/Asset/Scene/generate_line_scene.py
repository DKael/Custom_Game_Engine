import argparse
import json
import os
from copy import deepcopy

DEFAULT_TEMPLATE = os.path.join("ZZupEngine", "Asset", "Scene", "Default.scene")

DEFAULT_CAMERA = {
    "FOV": [60.0],
    "FarClip": [100.0],
    "Location": [-28.107929, -27.869390, 25.837776],
    "NearClip": [0.1],
    "Rotation": [0.0, 0.926283, 0.77],
}

DEFAULT_ASSET_ODD = "Data/apple_mid.obj"
DEFAULT_ASSET_EVEN = "Data/bitten_apple_mid.obj"


def load_template(path: str) -> dict:
    if not path:
        return {"NextUUID": 1, "PerspectiveCamera": deepcopy(DEFAULT_CAMERA), "Primitives": {}}
    if not os.path.exists(path):
        raise FileNotFoundError(f"Template scene not found: {path}")
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def parse_vec3(values, name):
    if len(values) != 3:
        raise ValueError(f"{name} must have exactly 3 numbers")
    return [float(v) for v in values]


def main():
    parser = argparse.ArgumentParser(
        description="Generate a .scene file with meshes aligned in a straight line or 3D grid.")
    parser.add_argument("--count", type=int, default=None, help="Number of meshes to create (line). Required unless --grid3 is used")
    parser.add_argument("--out", required=True, help="Output .scene path")
    parser.add_argument("--template", default=DEFAULT_TEMPLATE, help="Template .scene path")
    parser.add_argument("--asset", default=None, help="ObjStaticMeshAsset path (sets both odd/even)")
    parser.add_argument("--asset-odd", default=None, help="Asset for odd-indexed meshes (1st, 3rd, ...)")
    parser.add_argument("--asset-even", default=None, help="Asset for even-indexed meshes (2nd, 4th, ...)")
    parser.add_argument("--start", nargs=3, default=[0, 0, 0], help="Start location (x y z)")
    parser.add_argument("--axis", choices=["x", "y", "z"], default="x", help="Axis to align along")
    parser.add_argument("--spacing", type=float, default=1.0, help="Spacing between meshes")
    parser.add_argument("--grid3", nargs=3, type=int, metavar=("NX", "NY", "NZ"), help="Create a 3D grid (x y z counts)")
    parser.add_argument("--spacing-x", type=float, default=None, help="Grid spacing on x axis")
    parser.add_argument("--spacing-y", type=float, default=None, help="Grid spacing on y axis")
    parser.add_argument("--spacing-z", type=float, default=None, help="Grid spacing on z axis")
    parser.add_argument("--id-start", type=int, default=3, help="Starting UUID for primitives")
    parser.add_argument("--rotation", nargs=3, default=[0, 0, 0], help="Rotation (x y z)")
    parser.add_argument("--scale", nargs=3, default=[1, 1, 1], help="Scale (x y z)")
    parser.add_argument("--type", default="StaticMeshComp", help="Primitive Type")

    args = parser.parse_args()
    if args.count is not None and args.count < 0:
        raise ValueError("--count must be >= 0")
    if args.spacing < 0:
        raise ValueError("--spacing must be >= 0")
    if args.spacing_x is not None and args.spacing_x < 0:
        raise ValueError("--spacing-x must be >= 0")
    if args.spacing_y is not None and args.spacing_y < 0:
        raise ValueError("--spacing-y must be >= 0")
    if args.spacing_z is not None and args.spacing_z < 0:
        raise ValueError("--spacing-z must be >= 0")

    scene = load_template(args.template)
    camera = scene.get("PerspectiveCamera", deepcopy(DEFAULT_CAMERA))

    start = parse_vec3(args.start, "--start")
    rotation = parse_vec3(args.rotation, "--rotation")
    scale = parse_vec3(args.scale, "--scale")

    asset_odd = args.asset_odd or DEFAULT_ASSET_ODD
    asset_even = args.asset_even or DEFAULT_ASSET_EVEN
    if args.asset:
        asset_odd = args.asset
        asset_even = args.asset

    primitives = {}
    if args.grid3:
        nx, ny, nz = args.grid3
        if nx <= 0 or ny <= 0 or nz <= 0:
            raise ValueError("--grid3 values must be >= 1")

        sx = args.spacing if args.spacing_x is None else args.spacing_x
        sy = args.spacing if args.spacing_y is None else args.spacing_y
        sz = args.spacing if args.spacing_z is None else args.spacing_z

        total = nx * ny * nz
        count = total if args.count is None else min(args.count, total)

        for i in range(count):
            z = i // (nx * ny)
            rem = i % (nx * ny)
            y = rem // nx
            x = rem % nx

            loc = list(start)
            loc[0] = loc[0] + (x * sx)
            loc[1] = loc[1] + (y * sy)
            loc[2] = loc[2] + (z * sz)

            prim_id = str(args.id_start + i)
            asset_path = asset_odd if (i % 2 == 0) else asset_even
            primitives[prim_id] = {
                "Location": [float(loc[0]), float(loc[1]), float(loc[2])],
                "ObjStaticMeshAsset": asset_path,
                "Rotation": [float(rotation[0]), float(rotation[1]), float(rotation[2])],
                "Scale": [float(scale[0]), float(scale[1]), float(scale[2])],
                "Type": args.type,
            }
    else:
        if args.count is None:
            raise ValueError("--count is required unless --grid3 is used")
        axis_index = {"x": 0, "y": 1, "z": 2}[args.axis]
        count = args.count
        for i in range(count):
            loc = list(start)
            loc[axis_index] = loc[axis_index] + (i * args.spacing)
            prim_id = str(args.id_start + i)
            asset_path = asset_odd if (i % 2 == 0) else asset_even
            primitives[prim_id] = {
                "Location": [float(loc[0]), float(loc[1]), float(loc[2])],
                "ObjStaticMeshAsset": asset_path,
                "Rotation": [float(rotation[0]), float(rotation[1]), float(rotation[2])],
                "Scale": [float(scale[0]), float(scale[1]), float(scale[2])],
                "Type": args.type,
            }

    out_scene = {
        "NextUUID": args.id_start + count,
        "PerspectiveCamera": camera,
        "Primitives": primitives,
    }

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(out_scene, f, ensure_ascii=False, indent=2)


if __name__ == "__main__":
    main()
