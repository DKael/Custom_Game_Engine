from __future__ import annotations

import argparse
import json
import sys
from copy import deepcopy
from pathlib import Path
from typing import Any


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
ENGINE_ROOT = REPO_ROOT / "NipsEngine"
DEFAULT_OUTPUT_DIR = ENGINE_ROOT / "Asset" / "Scene"
SCENE_EXTENSION = ".Scene"

DEFAULT_CAMERA = {
    "location": [158.421021, 76.895691, 205.454651],
    "rotation": [-50.752869, -156.246597, 18.819605],
    "fov": 90.0,
    "near_clip": 0.1,
    "far_clip": 2000.0,
}

DEFAULT_FOG = {
    "enabled": False,
    "actor_visible": True,
    "visible": True,
    "location": [0.0, 0.0, 0.0],
    "rotation": [0.0, 0.0, 0.0],
    "scale": [1.0, 1.0, 1.0],
    "inscattering_color": [0.5, 0.6, 0.7, 1.0],
    "fog_density": 0.1,
    "height_falloff": 0.2,
    "start_distance": 0.0,
    "cutoff_distance": 0.0,
    "max_opacity": 1.0,
}

DEFAULT_TEXT = {
    "enabled": True,
    "visible": False,
    "outline": False,
    "font": "Default",
    "font_size": 1.0,
    "location": [0.0, 0.0, 1.0],
    "rotation": [0.0, 0.0, 0.0],
    "scale": [1.0, 1.0, 1.0],
}

DEFAULT_BILLBOARD = {
    "enabled": True,
    "visible": True,
    "outline": False,
    "sprite": "Asset\\Texture\\S_LightPoint.png",
    "width": 1.0,
    "height": 1.0,
    "play_rate": 30.0,
    "loop": True,
    "cylindrical": False,
    "use_rotation": False,
    "distance_fade": False,
    "fade_start_distance": 100.0,
    "fade_end_distance": 1000.0,
    "location": [0.0, 0.0, 0.0],
    "rotation": [0.0, 0.0, 0.0],
    "scale": [1.0, 1.0, 1.0],
}

DEFAULT_MESH_GROUP = {
    "enabled": True,
    "actor_visible": True,
    "visible": True,
    "outline": True,
    "counts": [1, 1, 1],
    "spacing": [120.0, 120.0, 120.0],
    "origin": [0.0, 0.0, 0.0],
    "location_offset": [0.0, 0.0, 0.0],
    "center_grid": True,
    "static_mesh": "Asset/Mesh/Dice/Dice.obj",
    "materials": [],
    "rotation": [0.0, 0.0, 0.0],
    "scale": [1.0, 1.0, 1.0],
    "scroll_uv": [0.0, 0.0],
    "text": deepcopy(DEFAULT_TEXT),
}

DEFAULT_LIGHT_GROUP = {
    "enabled": True,
    "actor_visible": True,
    "counts": [1, 1, 1],
    "spacing": [120.0, 120.0, 120.0],
    "origin": [0.0, 0.0, 31.459732],
    "location_offset": [0.0, 0.0, 0.0],
    "center_grid": True,
    "root_rotation": [0.0, 0.0, 0.0],
    "root_scale": [1.0, 1.0, 1.0],
    "light": {
        "color": [1.0, 1.0, 1.0, 1.0],
        "intensity": 12.4,
        "radius": 85.0,
        "location": [0.0, 0.0, 0.0],
        "rotation": [0.0, 0.0, 0.0],
        "scale": [1.0, 1.0, 1.0],
    },
    "billboard": deepcopy(DEFAULT_BILLBOARD),
}

DEFAULT_CONFIG = {
    "scene_name": "GeneratedScene",
    "world_type": "Editor",
    "camera": deepcopy(DEFAULT_CAMERA),
    "fog": deepcopy(DEFAULT_FOG),
    "mesh_groups": [],
    "point_light_groups": [],
}


def merge_dicts(base: Any, override: Any) -> Any:
    if isinstance(base, dict) and isinstance(override, dict):
        merged = deepcopy(base)
        for key, value in override.items():
            if key in merged:
                merged[key] = merge_dicts(merged[key], value)
            else:
                merged[key] = deepcopy(value)
        return merged
    return deepcopy(override)


def ensure_length(name: str, values: list[Any], expected: int) -> list[Any]:
    if len(values) != expected:
        raise ValueError(f"{name} must contain exactly {expected} values, got {len(values)}")
    return values


def vec3(name: str, values: Any, default: list[float] | None = None) -> list[float]:
    if values is None:
        if default is None:
            raise ValueError(f"{name} is required")
        return [float(v) for v in default]
    items = ensure_length(name, list(values), 3)
    return [float(v) for v in items]


def vec4(name: str, values: Any, default: list[float] | None = None) -> list[float]:
    if values is None:
        if default is None:
            raise ValueError(f"{name} is required")
        return [float(v) for v in default]
    items = ensure_length(name, list(values), 4)
    return [float(v) for v in items]


def vec2(name: str, values: Any, default: list[float] | None = None) -> list[float]:
    if values is None:
        if default is None:
            raise ValueError(f"{name} is required")
        return [float(v) for v in default]
    items = ensure_length(name, list(values), 2)
    return [float(v) for v in items]


def int3(name: str, values: Any, default: list[int] | None = None) -> list[int]:
    if values is None:
        if default is None:
            raise ValueError(f"{name} is required")
        return [int(v) for v in default]
    items = ensure_length(name, list(values), 3)
    result = [int(v) for v in items]
    if any(v < 0 for v in result):
        raise ValueError(f"{name} cannot contain negative values: {result}")
    return result


def format_text(template: str, flat_index: int, grid_index: list[int]) -> str:
    return template.format(
        index=flat_index,
        x=grid_index[0],
        y=grid_index[1],
        z=grid_index[2],
    )


def grid_instances(group: dict[str, Any]) -> list[dict[str, Any]]:
    counts = int3("counts", group.get("counts"), [1, 1, 1])
    if any(count == 0 for count in counts):
        return []

    spacing = vec3("spacing", group.get("spacing"), [0.0, 0.0, 0.0])
    origin = vec3("origin", group.get("origin"), [0.0, 0.0, 0.0])
    location_offset = vec3("location_offset", group.get("location_offset"), [0.0, 0.0, 0.0])
    center_grid = bool(group.get("center_grid", True))

    instances: list[dict[str, Any]] = []
    flat_index = 0

    for ix in range(counts[0]):
        for iy in range(counts[1]):
            for iz in range(counts[2]):
                location = []
                for axis_index, grid_index in enumerate((ix, iy, iz)):
                    if center_grid:
                        axis_offset = (grid_index - (counts[axis_index] - 1) * 0.5) * spacing[axis_index]
                    else:
                        axis_offset = grid_index * spacing[axis_index]
                    location.append(origin[axis_index] + location_offset[axis_index] + axis_offset)

                instances.append(
                    {
                        "flat_index": flat_index,
                        "grid_index": [ix, iy, iz],
                        "location": location,
                    }
                )
                flat_index += 1

    return instances


def build_text_component(text_config: dict[str, Any], flat_index: int, grid_index: list[int]) -> dict[str, Any] | None:
    if not text_config.get("enabled", True):
        return None

    props: dict[str, Any] = {
        "Font": str(text_config.get("font", "Default")),
        "Font Size": float(text_config.get("font_size", 1.0)),
        "Location": vec3("text.location", text_config.get("location"), DEFAULT_TEXT["location"]),
        "Outline": bool(text_config.get("outline", False)),
        "Rotation": vec3("text.rotation", text_config.get("rotation"), DEFAULT_TEXT["rotation"]),
        "Scale": vec3("text.scale", text_config.get("scale"), DEFAULT_TEXT["scale"]),
        "Visible": bool(text_config.get("visible", False)),
    }

    template = text_config.get("text_template")
    if template:
        props["Text"] = format_text(str(template), flat_index, grid_index)

    return {
        "ClassName": "UTextRenderComponent",
        "Properties": props,
    }


def build_static_mesh_actor(group: dict[str, Any], instance: dict[str, Any]) -> dict[str, Any]:
    scroll_uv = vec2("mesh.scroll_uv", group.get("scroll_uv"), [0.0, 0.0])
    props: dict[str, Any] = {
        "Location": instance["location"],
        "Outline": bool(group.get("outline", True)),
        "Rotation": vec3("mesh.rotation", group.get("rotation"), [0.0, 0.0, 0.0]),
        "Scale": vec3("mesh.scale", group.get("scale"), [1.0, 1.0, 1.0]),
        "Scroll U": float(scroll_uv[0]),
        "Scroll V": float(scroll_uv[1]),
        "StaticMesh": str(group.get("static_mesh", DEFAULT_MESH_GROUP["static_mesh"])),
        "Visible": bool(group.get("visible", True)),
    }

    for slot_index, material_name in enumerate(group.get("materials", [])):
        if material_name:
            props[f"Material {slot_index}"] = str(material_name)

    root_component: dict[str, Any] = {
        "ClassName": "StaticMeshComp",
        "Properties": props,
    }

    text_config = merge_dicts(DEFAULT_TEXT, group.get("text", {}))
    text_component = build_text_component(text_config, instance["flat_index"], instance["grid_index"])
    if text_component is not None:
        root_component["Children"] = [text_component]

    return {
        "ClassName": "AStaticMeshActor",
        "RootComponent": root_component,
        "Visible": bool(group.get("actor_visible", True)),
    }


def build_light_billboard(billboard: dict[str, Any]) -> dict[str, Any] | None:
    if not billboard.get("enabled", True):
        return None

    return {
        "ClassName": "UBillboardComponent",
        "Properties": {
            "Distance Fade": bool(billboard.get("distance_fade", False)),
            "Fade End Distance": float(billboard.get("fade_end_distance", 1000.0)),
            "Fade Start Distance": float(billboard.get("fade_start_distance", 100.0)),
            "Height": float(billboard.get("height", 1.0)),
            "Location": vec3("billboard.location", billboard.get("location"), [0.0, 0.0, 0.0]),
            "Outline": bool(billboard.get("outline", False)),
            "Play Rate": float(billboard.get("play_rate", 30.0)),
            "Rotation": vec3("billboard.rotation", billboard.get("rotation"), [0.0, 0.0, 0.0]),
            "Scale": vec3("billboard.scale", billboard.get("scale"), [1.0, 1.0, 1.0]),
            "Sprite": str(billboard.get("sprite", DEFAULT_BILLBOARD["sprite"])),
            "Visible": bool(billboard.get("visible", True)),
            "Width": float(billboard.get("width", 1.0)),
            "bCylindrical": bool(billboard.get("cylindrical", False)),
            "bLoop": bool(billboard.get("loop", True)),
            "bUseRotation": bool(billboard.get("use_rotation", False)),
        },
    }


def build_point_light_actor(group: dict[str, Any], instance: dict[str, Any]) -> dict[str, Any]:
    light = group.get("light", {})
    billboard = merge_dicts(DEFAULT_BILLBOARD, group.get("billboard", {}))

    children: list[dict[str, Any]] = [
        {
            "ClassName": "UPointLightComponent",
            "Properties": {
                "Color": vec4("light.color", light.get("color"), [1.0, 1.0, 1.0, 1.0]),
                "Intensity": float(light.get("intensity", 12.4)),
                "Location": vec3("light.location", light.get("location"), [0.0, 0.0, 0.0]),
                "Radius": float(light.get("radius", 85.0)),
                "Rotation": vec3("light.rotation", light.get("rotation"), [0.0, 0.0, 0.0]),
                "Scale": vec3("light.scale", light.get("scale"), [1.0, 1.0, 1.0]),
            },
        }
    ]

    billboard_component = build_light_billboard(billboard)
    if billboard_component is not None:
        children.append(billboard_component)

    return {
        "ClassName": "APointLightActor",
        "RootComponent": {
            "ClassName": "USceneComponent",
            "Children": children,
            "Properties": {
                "Location": instance["location"],
                "Rotation": vec3("light.root_rotation", group.get("root_rotation"), [0.0, 0.0, 0.0]),
                "Scale": vec3("light.root_scale", group.get("root_scale"), [1.0, 1.0, 1.0]),
            },
        },
        "Visible": bool(group.get("actor_visible", True)),
    }


def build_fog_actor(fog: dict[str, Any]) -> dict[str, Any] | None:
    if not fog.get("enabled", False):
        return None

    return {
        "ClassName": "AExponentialHeightFog",
        "RootComponent": {
            "ClassName": "UHeightFogComponent",
            "Properties": {
                "Cutoff Distance": float(fog.get("cutoff_distance", 0.0)),
                "Fog Density": float(fog.get("fog_density", 0.1)),
                "Height Falloff": float(fog.get("height_falloff", 0.2)),
                "Inscattering Color": vec4(
                    "fog.inscattering_color",
                    fog.get("inscattering_color"),
                    [0.5, 0.6, 0.7, 1.0],
                ),
                "Location": vec3("fog.location", fog.get("location"), [0.0, 0.0, 0.0]),
                "Max Opacity": float(fog.get("max_opacity", 1.0)),
                "Rotation": vec3("fog.rotation", fog.get("rotation"), [0.0, 0.0, 0.0]),
                "Scale": vec3("fog.scale", fog.get("scale"), [1.0, 1.0, 1.0]),
                "Start Distance": float(fog.get("start_distance", 0.0)),
                "Visible": bool(fog.get("visible", True)),
            },
        },
        "Visible": bool(fog.get("actor_visible", True)),
    }


def build_camera(camera: dict[str, Any]) -> dict[str, Any]:
    return {
        "FOV": [float(camera.get("fov", DEFAULT_CAMERA["fov"]))],
        "FarClip": [float(camera.get("far_clip", DEFAULT_CAMERA["far_clip"]))],
        "Location": vec3("camera.location", camera.get("location"), DEFAULT_CAMERA["location"]),
        "NearClip": [float(camera.get("near_clip", DEFAULT_CAMERA["near_clip"]))],
        "Rotation": vec3("camera.rotation", camera.get("rotation"), DEFAULT_CAMERA["rotation"]),
    }


def build_scene(config: dict[str, Any]) -> tuple[dict[str, Any], dict[str, int]]:
    scene_name = str(config.get("scene_name") or "GeneratedScene")
    world_type = str(config.get("world_type") or "Editor")
    if world_type not in {"Editor", "Game", "PIE"}:
        raise ValueError(f"world_type must be one of Editor/Game/PIE, got {world_type!r}")

    actors: list[dict[str, Any]] = []

    fog_config = merge_dicts(DEFAULT_FOG, config.get("fog", {}))
    fog_actor = build_fog_actor(fog_config)
    if fog_actor is not None:
        actors.append(fog_actor)

    mesh_count = 0
    for group_override in config.get("mesh_groups", []):
        group = merge_dicts(DEFAULT_MESH_GROUP, group_override)
        if not group.get("enabled", True):
            continue
        for instance in grid_instances(group):
            actors.append(build_static_mesh_actor(group, instance))
            mesh_count += 1

    light_count = 0
    for group_override in config.get("point_light_groups", []):
        group = merge_dicts(DEFAULT_LIGHT_GROUP, group_override)
        if not group.get("enabled", True):
            continue
        for instance in grid_instances(group):
            actors.append(build_point_light_actor(group, instance))
            light_count += 1

    scene = {
        "Actors": actors,
        "ClassName": "UWorld",
        "Name": scene_name,
        "PerspectiveCamera": build_camera(merge_dicts(DEFAULT_CAMERA, config.get("camera", {}))),
        "Version": 3,
        "WorldType": world_type,
    }

    summary = {
        "fog_actors": 1 if fog_actor is not None else 0,
        "static_mesh_actors": mesh_count,
        "point_light_actors": light_count,
        "total_actors": len(actors),
    }
    return scene, summary


def load_config(config_path: Path) -> dict[str, Any]:
    with config_path.open("r", encoding="utf-8") as file:
        return json.load(file)


def resolve_output_path(output_arg: str | None, scene_name: str) -> Path:
    if output_arg:
        output_path = Path(output_arg)
    else:
        output_path = DEFAULT_OUTPUT_DIR / scene_name

    if output_path.suffix.lower() != SCENE_EXTENSION.lower():
        output_path = output_path.with_suffix(SCENE_EXTENSION)

    if output_path.is_absolute():
        return output_path

    first_part = output_path.parts[0] if output_path.parts else ""
    if first_part == "Asset":
        return (ENGINE_ROOT / output_path).resolve()
    return (REPO_ROOT / output_path).resolve()


def build_config_from_cli(args: argparse.Namespace) -> dict[str, Any]:
    mesh_group = merge_dicts(
        DEFAULT_MESH_GROUP,
        {
            "counts": args.mesh_count,
            "spacing": args.mesh_spacing,
            "origin": args.mesh_origin,
            "location_offset": args.mesh_location_offset,
            "center_grid": args.mesh_center_grid,
            "static_mesh": args.mesh_path,
            "materials": args.mesh_material or [],
            "rotation": args.mesh_rotation,
            "scale": args.mesh_scale,
            "scroll_uv": args.mesh_scroll_uv,
            "visible": args.mesh_visible,
            "outline": args.mesh_outline,
            "text": {
                "enabled": args.mesh_text_enabled,
                "visible": args.mesh_text_visible,
                "outline": False,
                "font": args.mesh_text_font,
                "font_size": args.mesh_text_font_size,
                "location": args.mesh_text_location,
                "rotation": [0.0, 0.0, 0.0],
                "scale": [1.0, 1.0, 1.0],
                "text_template": args.mesh_text_template,
            },
        },
    )

    light_group = merge_dicts(
        DEFAULT_LIGHT_GROUP,
        {
            "counts": args.light_count,
            "spacing": args.light_spacing,
            "origin": args.light_origin,
            "location_offset": args.light_location_offset,
            "center_grid": args.light_center_grid,
            "root_rotation": args.light_root_rotation,
            "root_scale": args.light_root_scale,
            "light": {
                "color": args.light_color,
                "intensity": args.light_intensity,
                "radius": args.light_radius,
                "location": args.light_component_location,
                "rotation": args.light_component_rotation,
                "scale": args.light_component_scale,
            },
            "billboard": {
                "enabled": args.light_billboard_enabled,
                "visible": args.light_billboard_visible,
                "outline": False,
                "sprite": args.light_billboard_sprite,
                "width": args.light_billboard_width,
                "height": args.light_billboard_height,
                "play_rate": args.light_billboard_play_rate,
                "loop": args.light_billboard_loop,
                "cylindrical": args.light_billboard_cylindrical,
                "use_rotation": args.light_billboard_use_rotation,
                "distance_fade": args.light_billboard_distance_fade,
                "fade_start_distance": args.light_billboard_fade_start,
                "fade_end_distance": args.light_billboard_fade_end,
                "location": args.light_billboard_location,
                "rotation": args.light_billboard_rotation,
                "scale": args.light_billboard_scale,
            },
        },
    )

    return {
        "scene_name": args.scene_name,
        "world_type": args.world_type,
        "camera": {
            "location": args.camera_location,
            "rotation": args.camera_rotation,
            "fov": args.camera_fov,
            "near_clip": args.camera_near,
            "far_clip": args.camera_far,
        },
        "fog": {
            "enabled": args.include_fog,
            "actor_visible": True,
            "visible": True,
            "location": args.fog_location,
            "rotation": args.fog_rotation,
            "scale": args.fog_scale,
            "inscattering_color": args.fog_color,
            "fog_density": args.fog_density,
            "height_falloff": args.fog_height_falloff,
            "start_distance": args.fog_start_distance,
            "cutoff_distance": args.fog_cutoff_distance,
            "max_opacity": args.fog_max_opacity,
        },
        "mesh_groups": [mesh_group],
        "point_light_groups": [light_group],
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate a NipsEngine .Scene file with StaticMeshActor and PointLightActor grids."
    )
    parser.add_argument("--config", type=Path, help="JSON config file for advanced multi-group scene generation.")
    parser.add_argument("--scene-name", default="GeneratedScene", help="Root scene name written into the .Scene file.")
    parser.add_argument("--output", help="Output path. Relative Asset/... paths are resolved under NipsEngine.")
    parser.add_argument("--world-type", default="Editor", choices=["Editor", "Game", "PIE"])
    parser.add_argument("--dry-run", action="store_true", help="Print JSON to stdout instead of writing a file.")
    parser.add_argument("--overwrite", action="store_true", help="Overwrite an existing output file.")

    camera = parser.add_argument_group("camera")
    camera.add_argument("--camera-location", nargs=3, type=float, default=DEFAULT_CAMERA["location"])
    camera.add_argument("--camera-rotation", nargs=3, type=float, default=DEFAULT_CAMERA["rotation"])
    camera.add_argument("--camera-fov", type=float, default=DEFAULT_CAMERA["fov"])
    camera.add_argument("--camera-near", type=float, default=DEFAULT_CAMERA["near_clip"])
    camera.add_argument("--camera-far", type=float, default=DEFAULT_CAMERA["far_clip"])

    fog = parser.add_argument_group("fog")
    fog.add_argument("--include-fog", action=argparse.BooleanOptionalAction, default=False)
    fog.add_argument("--fog-location", nargs=3, type=float, default=DEFAULT_FOG["location"])
    fog.add_argument("--fog-rotation", nargs=3, type=float, default=DEFAULT_FOG["rotation"])
    fog.add_argument("--fog-scale", nargs=3, type=float, default=DEFAULT_FOG["scale"])
    fog.add_argument("--fog-color", nargs=4, type=float, default=DEFAULT_FOG["inscattering_color"])
    fog.add_argument("--fog-density", type=float, default=DEFAULT_FOG["fog_density"])
    fog.add_argument("--fog-height-falloff", type=float, default=DEFAULT_FOG["height_falloff"])
    fog.add_argument("--fog-start-distance", type=float, default=DEFAULT_FOG["start_distance"])
    fog.add_argument("--fog-cutoff-distance", type=float, default=DEFAULT_FOG["cutoff_distance"])
    fog.add_argument("--fog-max-opacity", type=float, default=DEFAULT_FOG["max_opacity"])

    mesh = parser.add_argument_group("static mesh grid")
    mesh.add_argument("--mesh-count", nargs=3, type=int, default=DEFAULT_MESH_GROUP["counts"])
    mesh.add_argument("--mesh-spacing", nargs=3, type=float, default=DEFAULT_MESH_GROUP["spacing"])
    mesh.add_argument("--mesh-origin", nargs=3, type=float, default=DEFAULT_MESH_GROUP["origin"])
    mesh.add_argument("--mesh-location-offset", nargs=3, type=float, default=DEFAULT_MESH_GROUP["location_offset"])
    mesh.add_argument("--mesh-center-grid", action=argparse.BooleanOptionalAction, default=True)
    mesh.add_argument("--mesh-path", default=DEFAULT_MESH_GROUP["static_mesh"])
    mesh.add_argument("--mesh-material", action="append", help="Repeat to fill Material 0, Material 1, ...")
    mesh.add_argument("--mesh-rotation", nargs=3, type=float, default=DEFAULT_MESH_GROUP["rotation"])
    mesh.add_argument("--mesh-scale", nargs=3, type=float, default=DEFAULT_MESH_GROUP["scale"])
    mesh.add_argument("--mesh-scroll-uv", nargs=2, type=float, default=DEFAULT_MESH_GROUP["scroll_uv"])
    mesh.add_argument("--mesh-visible", action=argparse.BooleanOptionalAction, default=True)
    mesh.add_argument("--mesh-outline", action=argparse.BooleanOptionalAction, default=True)
    mesh.add_argument("--mesh-text-enabled", action=argparse.BooleanOptionalAction, default=True)
    mesh.add_argument("--mesh-text-visible", action=argparse.BooleanOptionalAction, default=False)
    mesh.add_argument("--mesh-text-font", default=DEFAULT_TEXT["font"])
    mesh.add_argument("--mesh-text-font-size", type=float, default=DEFAULT_TEXT["font_size"])
    mesh.add_argument("--mesh-text-location", nargs=3, type=float, default=DEFAULT_TEXT["location"])
    mesh.add_argument(
        "--mesh-text-template",
        help="Optional text override. Supports {index}, {x}, {y}, {z}. Omit to preserve the actor's UUID text.",
    )

    light = parser.add_argument_group("point light grid")
    light.add_argument("--light-count", nargs=3, type=int, default=DEFAULT_LIGHT_GROUP["counts"])
    light.add_argument("--light-spacing", nargs=3, type=float, default=DEFAULT_LIGHT_GROUP["spacing"])
    light.add_argument("--light-origin", nargs=3, type=float, default=DEFAULT_LIGHT_GROUP["origin"])
    light.add_argument("--light-location-offset", nargs=3, type=float, default=DEFAULT_LIGHT_GROUP["location_offset"])
    light.add_argument("--light-center-grid", action=argparse.BooleanOptionalAction, default=True)
    light.add_argument("--light-root-rotation", nargs=3, type=float, default=DEFAULT_LIGHT_GROUP["root_rotation"])
    light.add_argument("--light-root-scale", nargs=3, type=float, default=DEFAULT_LIGHT_GROUP["root_scale"])
    light.add_argument("--light-color", nargs=4, type=float, default=DEFAULT_LIGHT_GROUP["light"]["color"])
    light.add_argument("--light-intensity", type=float, default=DEFAULT_LIGHT_GROUP["light"]["intensity"])
    light.add_argument("--light-radius", type=float, default=DEFAULT_LIGHT_GROUP["light"]["radius"])
    light.add_argument(
        "--light-component-location",
        nargs=3,
        type=float,
        default=DEFAULT_LIGHT_GROUP["light"]["location"],
    )
    light.add_argument(
        "--light-component-rotation",
        nargs=3,
        type=float,
        default=DEFAULT_LIGHT_GROUP["light"]["rotation"],
    )
    light.add_argument(
        "--light-component-scale",
        nargs=3,
        type=float,
        default=DEFAULT_LIGHT_GROUP["light"]["scale"],
    )
    light.add_argument("--light-billboard-enabled", action=argparse.BooleanOptionalAction, default=True)
    light.add_argument("--light-billboard-visible", action=argparse.BooleanOptionalAction, default=True)
    light.add_argument("--light-billboard-sprite", default=DEFAULT_BILLBOARD["sprite"])
    light.add_argument("--light-billboard-width", type=float, default=DEFAULT_BILLBOARD["width"])
    light.add_argument("--light-billboard-height", type=float, default=DEFAULT_BILLBOARD["height"])
    light.add_argument("--light-billboard-play-rate", type=float, default=DEFAULT_BILLBOARD["play_rate"])
    light.add_argument("--light-billboard-loop", action=argparse.BooleanOptionalAction, default=True)
    light.add_argument("--light-billboard-cylindrical", action=argparse.BooleanOptionalAction, default=False)
    light.add_argument("--light-billboard-use-rotation", action=argparse.BooleanOptionalAction, default=False)
    light.add_argument("--light-billboard-distance-fade", action=argparse.BooleanOptionalAction, default=False)
    light.add_argument("--light-billboard-fade-start", type=float, default=DEFAULT_BILLBOARD["fade_start_distance"])
    light.add_argument("--light-billboard-fade-end", type=float, default=DEFAULT_BILLBOARD["fade_end_distance"])
    light.add_argument("--light-billboard-location", nargs=3, type=float, default=DEFAULT_BILLBOARD["location"])
    light.add_argument("--light-billboard-rotation", nargs=3, type=float, default=DEFAULT_BILLBOARD["rotation"])
    light.add_argument("--light-billboard-scale", nargs=3, type=float, default=DEFAULT_BILLBOARD["scale"])

    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if args.config:
        user_config = load_config(args.config)
        config = merge_dicts(DEFAULT_CONFIG, user_config)
        if "--scene-name" in sys.argv:
            config["scene_name"] = args.scene_name
        if "--world-type" in sys.argv:
            config["world_type"] = args.world_type
    else:
        config = merge_dicts(DEFAULT_CONFIG, build_config_from_cli(args))

    scene_name = str(config.get("scene_name") or "GeneratedScene")
    output_path = resolve_output_path(args.output, scene_name)

    if not config.get("scene_name"):
        config["scene_name"] = output_path.stem
        scene_name = output_path.stem

    scene, summary = build_scene(config)

    if args.dry_run:
        json.dump(scene, sys.stdout, indent=2, ensure_ascii=False)
        sys.stdout.write("\n")
        return 0

    output_path.parent.mkdir(parents=True, exist_ok=True)
    if output_path.exists() and not args.overwrite:
        raise FileExistsError(f"{output_path} already exists. Use --overwrite to replace it.")

    with output_path.open("w", encoding="utf-8", newline="\n") as file:
        json.dump(scene, file, indent=2, ensure_ascii=False)
        file.write("\n")

    print(f"Wrote scene to {output_path}")
    print(
        "Actors:"
        f" fog={summary['fog_actors']},"
        f" static_mesh={summary['static_mesh_actors']},"
        f" point_lights={summary['point_light_actors']},"
        f" total={summary['total_actors']}"
    )
    return 0

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        raise SystemExit(1)
