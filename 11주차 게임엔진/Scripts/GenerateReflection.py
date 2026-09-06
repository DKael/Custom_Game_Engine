"""
Generate a small reflection registration unit from UPROPERTY annotations.

Supported declaration shape:

    UPROPERTY(EditAnywhere, DisplayName="Blend Alpha", Min=0.0, Max=1.0, Speed=0.01, Animatable)
    float BlendAlpha = 0.0f;

    UPROPERTY(VisibleAnywhere)
    UAnimSequence* Anim = nullptr;

Only EditAnywhere and VisibleAnywhere are accepted as access specifiers.
"""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
PROJECT_DIR = ROOT / "JSEngine"
SOURCE_DIR = PROJECT_DIR / "Source"
OUTPUT_DIR = SOURCE_DIR / "Generated"
OUTPUT_CPP = OUTPUT_DIR / "Reflection.generated.cpp"


UCLASS_RE = re.compile(
    r"UCLASS\s*\(([^)]*)\)\s*(?:\r?\n\s*)*class\s+([A-Za-z_]\w*)"
    r"(?:\s*:\s*public\s+([A-Za-z_]\w*))?"
)
USTRUCT_RE = re.compile(r"USTRUCT\s*\([^)]*\)\s*(?:\r?\n\s*)*struct\s+([A-Za-z_]\w*)")
UENUM_RE = re.compile(r"UENUM\s*\([^)]*\)\s*(?:\r?\n\s*)*enum\s+class\s+([A-Za-z_]\w*)(?:\s*:\s*[A-Za-z_]\w*)?")
GENERATED_BODY_RE = re.compile(r"GENERATED_BODY\s*\([^)]*\)")
DECL_RE = re.compile(
    r"^(?P<type>[A-Za-z_]\w*(?:::[A-Za-z_]\w*)?(?:\s*<[^;{}]+>)?(?:\s*[*&])?)\s+"
    r"(?P<name>[A-Za-z_]\w*)\s*(?:[={].*)?;\s*$"
)


TYPE_MAP = {
    "bool": "EPropertyType::Bool",
    "int": "EPropertyType::Int",
    "int32": "EPropertyType::Int",
    "float": "EPropertyType::Float",
    "FVector": "EPropertyType::Vec3",
    "FVector4": "EPropertyType::Vec4",
    "FColor": "EPropertyType::Color",
    "FString": "EPropertyType::String",
    "FName": "EPropertyType::Name",
}

PROPERTY_CLASS_MAP = {
    "EPropertyType::Bool": "FBoolProperty",
    "EPropertyType::Int": "FIntProperty",
    "EPropertyType::Float": "FFloatProperty",
    "EPropertyType::Vec3": "FVectorProperty",
    "EPropertyType::Vec4": "FVector4Property",
    "EPropertyType::String": "FStringProperty",
    "EPropertyType::Name": "FNameProperty",
    "EPropertyType::Color": "FColorProperty",
    "EPropertyType::ObjectRef": "FObjectProperty",
    "EPropertyType::SceneComponentRef": "FSceneComponentProperty",
    "EPropertyType::Struct": "FStructProperty",
    "EPropertyType::Enum": "FEnumProperty",
    "EPropertyType::Vec3Array": "FArrayProperty",
}


def write_if_changed(path: Path, content: str) -> bool:
    if path.exists():
        current = path.read_text(encoding="utf-8-sig", errors="ignore")
        if current == content:
            return False

    path.write_text(content, encoding="utf-8", newline="\r\n")
    return True


def strip_line_comment(line: str) -> str:
    return line.split("//", 1)[0].strip()


def normalize_type(type_text: str) -> str:
    return re.sub(r"\s+", "", type_text.strip())


def make_file_id(rel_header: str) -> str:
    return re.sub(r"[^A-Za-z0-9_]", "_", rel_header)


def make_generated_header_name(rel_header: str) -> str:
    return f"{Path(rel_header).stem}.generated.h"


def get_line_number(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def get_property_type(
    type_text: str,
    reflected_structs: set[str] | None = None,
    reflected_enums: set[str] | None = None,
) -> tuple[str, str]:
    reflected_structs = reflected_structs or set()
    reflected_enums = reflected_enums or set()
    normalized = normalize_type(type_text)
    if normalized == "TArray<FVector>":
        return "EPropertyType::Vec3Array", "nullptr"

    if normalized.endswith("*"):
        object_type = normalized[:-1]
        return "EPropertyType::ObjectRef", f"{object_type}::StaticClass()"

    if normalized in TYPE_MAP:
        return TYPE_MAP[normalized], "nullptr"

    if normalized in reflected_structs:
        return "EPropertyType::Struct", f"{normalized}::StaticStruct()"

    if normalized in reflected_enums:
        return "EPropertyType::Enum", f"Z_Construct_UEnum_{normalized}()"

    raise ValueError(f"Unsupported UPROPERTY type: {type_text}")


def split_metadata_args(args_text: str) -> list[str]:
    args: list[str] = []
    current: list[str] = []
    in_string = False
    escaped = False

    for char in args_text:
        if escaped:
            current.append(char)
            escaped = False
            continue

        if char == "\\" and in_string:
            current.append(char)
            escaped = True
            continue

        if char == '"':
            in_string = not in_string
            current.append(char)
            continue

        if char == "," and not in_string:
            arg = "".join(current).strip()
            if arg:
                args.append(arg)
            current = []
            continue

        current.append(char)

    arg = "".join(current).strip()
    if arg:
        args.append(arg)

    return args


def find_uproperty_args(line: str) -> str | None:
    marker = "UPROPERTY"
    start = line.find(marker)
    if start < 0:
        return None

    open_index = line.find("(", start + len(marker))
    if open_index < 0:
        return None

    depth = 0
    in_string = False
    escaped = False
    for index in range(open_index, len(line)):
        char = line[index]
        if escaped:
            escaped = False
            continue
        if char == "\\" and in_string:
            escaped = True
            continue
        if char == '"':
            in_string = not in_string
            continue
        if in_string:
            continue
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                return line[open_index + 1:index]

    return None


def parse_metadata(args_text: str, path: Path, line_number: int) -> dict[str, str]:
    args = split_metadata_args(args_text)
    if not args:
        raise ValueError(f"{path}:{line_number}: UPROPERTY requires EditAnywhere or VisibleAnywhere")

    access = args[0]
    if access not in {"EditAnywhere", "VisibleAnywhere"}:
        raise ValueError(
            f"{path}:{line_number}: only UPROPERTY(EditAnywhere) and UPROPERTY(VisibleAnywhere) are supported"
        )

    metadata = {
        "access": access,
        "display_name": "nullptr",
        "serialize_name": "nullptr",
        "min": "0.0f",
        "max": "0.0f",
        "speed": "0.1f",
        "usage_flags": "EPropertyUsageFlags::None",
        "type_override": "",
    }

    usage_flags: list[str] = []
    for arg in args[1:]:
        if arg == "Animatable":
            usage_flags.append("EPropertyUsageFlags::Animatable")
            continue
        if arg in {"Transient", "NonSerialized"}:
            usage_flags.append("EPropertyUsageFlags::NonSerialized")
            continue

        if "=" not in arg:
            raise ValueError(f"{path}:{line_number}: unsupported UPROPERTY metadata '{arg}'")

        key, value = [part.strip() for part in arg.split("=", 1)]
        if key == "DisplayName":
            if not (value.startswith('"') and value.endswith('"')):
                raise ValueError(f"{path}:{line_number}: DisplayName must be a quoted string")
            metadata["display_name"] = value
        elif key == "SerializeName":
            if not (value.startswith('"') and value.endswith('"')):
                raise ValueError(f"{path}:{line_number}: SerializeName must be a quoted string")
            metadata["serialize_name"] = value
        elif key == "Min":
            metadata["min"] = value
        elif key == "Max":
            metadata["max"] = value
        elif key == "Speed":
            metadata["speed"] = value
        elif key == "Type":
            if value != "SceneComponentRef":
                raise ValueError(f"{path}:{line_number}: unsupported Type override '{value}'")
            metadata["type_override"] = "EPropertyType::SceneComponentRef"
        else:
            raise ValueError(f"{path}:{line_number}: unsupported UPROPERTY metadata key '{key}'")

    if usage_flags:
        metadata["usage_flags"] = " | ".join(usage_flags)

    return metadata


def make_float_literal(value: str) -> str:
    value = value.strip()
    if value.endswith(("f", "F")):
        return value

    if re.fullmatch(r"[+-]?(?:\d+(?:\.\d*)?|\.\d+)", value):
        return f"{value}f"

    return value


def get_property_class(property_type: str, path: Path, line_number: int) -> str:
    property_class = PROPERTY_CLASS_MAP.get(property_type)
    if not property_class:
        raise ValueError(f"{path}:{line_number}: unsupported generated property type '{property_type}'")
    return property_class


def has_class_metadata(args_text: str, name: str) -> bool:
    return any(arg.strip() == name for arg in split_metadata_args(args_text))


def find_reflected_class_ranges(text: str, path: Path) -> list[tuple[str, int, int, int, str | None, str]]:
    ranges: list[tuple[str, int, int, int, str | None, str]] = []
    for match in UCLASS_RE.finditer(text):
        metadata = match.group(1)
        class_name = match.group(2)
        parent_name = match.group(3)
        brace_start = text.find("{", match.end())
        if brace_start < 0:
            continue

        depth = 0
        end = -1
        for index in range(brace_start, len(text)):
            char = text[index]
            if char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0:
                    end = index
                    break

        if end >= 0:
            generated_body = GENERATED_BODY_RE.search(text, brace_start, end)
            if not generated_body:
                line_number = get_line_number(text, match.start())
                raise ValueError(f"{path}:{line_number}: UCLASS requires GENERATED_BODY()")
            ranges.append((class_name, brace_start, end, get_line_number(text, generated_body.start()), parent_name, metadata))

    return ranges


def find_reflected_struct_ranges(text: str, path: Path) -> list[tuple[str, int, int, int]]:
    ranges: list[tuple[str, int, int, int]] = []
    for match in USTRUCT_RE.finditer(text):
        struct_name = match.group(1)
        brace_start = text.find("{", match.end())
        if brace_start < 0:
            continue

        depth = 0
        end = -1
        for index in range(brace_start, len(text)):
            char = text[index]
            if char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0:
                    end = index
                    break

        if end >= 0:
            generated_body = GENERATED_BODY_RE.search(text, brace_start, end)
            if not generated_body:
                line_number = get_line_number(text, match.start())
                raise ValueError(f"{path}:{line_number}: USTRUCT requires GENERATED_BODY()")
            ranges.append((struct_name, brace_start, end, get_line_number(text, generated_body.start())))

    return ranges


def find_reflected_enum_ranges(text: str) -> list[tuple[str, int, int]]:
    ranges: list[tuple[str, int, int]] = []
    for match in UENUM_RE.finditer(text):
        enum_name = match.group(1)
        brace_start = text.find("{", match.end())
        if brace_start < 0:
            continue

        depth = 0
        end = -1
        for index in range(brace_start, len(text)):
            char = text[index]
            if char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0:
                    end = index
                    break

        if end >= 0:
            ranges.append((enum_name, brace_start, end))

    return ranges


def parse_enum_values(text: str, start: int, end: int) -> list[dict[str, str]]:
    body = text[start + 1:end]
    values: list[dict[str, str]] = []
    for raw_entry in body.split(","):
        entry = raw_entry.split("//", 1)[0].strip()
        if not entry:
            continue
        display_match = re.search(r'UMETA\s*\(\s*DisplayName\s*=\s*"([^"]+)"\s*\)', entry)
        display_name = display_match.group(1) if display_match else None
        entry = re.sub(r"UMETA\s*\([^)]*\)", "", entry).strip()
        name = entry.split("=", 1)[0].strip()
        if not re.fullmatch(r"[A-Za-z_]\w*", name):
            continue
        if name in {"MAX", "Max", "Count", "COUNT", "Num", "NUM"}:
            continue
        values.append({"name": name, "display_name": display_name or name})
    return values


def parse_properties_for_ranges(
    path: Path,
    ranges: list[tuple],
    reflected_structs: set[str],
    reflected_enums: set[str],
) -> dict[str, list[dict[str, str]]]:
    text = path.read_text(encoding="utf-8-sig", errors="ignore")
    lines = text.splitlines()
    line_offsets: list[int] = []
    cursor = 0
    for line in lines:
        line_offsets.append(cursor)
        cursor += len(line) + 1

    if not ranges:
        return {}

    properties_by_owner: dict[str, list[dict[str, str]]] = {}
    pending_metadata: dict[str, str] | None = None
    pending_line = 0

    for line_index, line in enumerate(lines):
        offset = line_offsets[line_index]
        active_owner = None
        for owner_name, start, end, *_ in ranges:
            if start <= offset <= end:
                active_owner = owner_name
                break

        clean = strip_line_comment(line)
        if not active_owner:
            pending_metadata = None
            continue

        property_args = find_uproperty_args(clean)
        if property_args is not None:
            pending_metadata = parse_metadata(property_args, path, line_index + 1)
            pending_line = line_index + 1
            continue

        if pending_metadata is None:
            continue

        if not clean or clean in {"public:", "protected:", "private:"}:
            continue

        declaration = DECL_RE.match(clean)
        if not declaration:
            raise ValueError(
                f"{path}:{pending_line}: UPROPERTY must be followed by a supported single-line member declaration"
            )

        type_text = declaration.group("type")
        member_name = declaration.group("name")
        property_type, object_type = get_property_type(type_text, reflected_structs, reflected_enums)
        if pending_metadata["type_override"]:
            property_type = pending_metadata["type_override"]
            object_type = "nullptr"

        properties_by_owner.setdefault(active_owner, []).append(
            {
                "access": pending_metadata["access"],
                "name": member_name,
                "property_type": property_type,
                "property_class": get_property_class(property_type, path, pending_line),
                "object_type": object_type,
                "display_name": pending_metadata["display_name"],
                "serialize_name": pending_metadata["serialize_name"],
                "min": pending_metadata["min"],
                "max": pending_metadata["max"],
                "speed": pending_metadata["speed"],
                "usage_flags": pending_metadata["usage_flags"],
            }
        )
        pending_metadata = None

    return properties_by_owner


def main() -> None:
    reflected: dict[str, dict[str, object]] = {}
    reflected_structs: dict[str, dict[str, object]] = {}
    reflected_enums: dict[str, dict[str, object]] = {}

    headers = sorted(SOURCE_DIR.rglob("*.h"))

    for header in headers:
        text = header.read_text(encoding="utf-8-sig", errors="ignore")
        rel_header = header.relative_to(SOURCE_DIR).as_posix()

        for class_name, _, _, generated_body_line, parent_name, metadata in find_reflected_class_ranges(text, header):
            reflected[class_name] = {
                "header": rel_header,
                "generated_header": make_generated_header_name(rel_header),
                "file_id": make_file_id(rel_header),
                "generated_body_line": generated_body_line,
                "parent": parent_name,
                "factory_excluded": has_class_metadata(metadata, "Abstract"),
                "properties": [],
            }

        for struct_name, _, _, generated_body_line in find_reflected_struct_ranges(text, header):
            reflected_structs[struct_name] = {
                "header": rel_header,
                "generated_header": make_generated_header_name(rel_header),
                "file_id": make_file_id(rel_header),
                "generated_body_line": generated_body_line,
                "properties": [],
            }

        for enum_name, start, end in find_reflected_enum_ranges(text):
            reflected_enums[enum_name] = {
                "header": rel_header,
                "values": parse_enum_values(text, start, end),
            }

    reflected_struct_names = set(reflected_structs.keys())
    reflected_enum_names = set(reflected_enums.keys())

    for header in headers:
        text = header.read_text(encoding="utf-8-sig", errors="ignore")
        rel_header = header.relative_to(SOURCE_DIR).as_posix()

        parsed_structs = parse_properties_for_ranges(
            header,
            find_reflected_struct_ranges(text, header),
            reflected_struct_names,
            reflected_enum_names,
        )
        for struct_name, properties in parsed_structs.items():
            if struct_name in reflected_structs:
                reflected_structs[struct_name]["properties"] = properties

        parsed = parse_properties_for_ranges(
            header,
            find_reflected_class_ranges(text, header),
            reflected_struct_names,
            reflected_enum_names,
        )
        for class_name, properties in parsed.items():
            if class_name in reflected:
                reflected[class_name]["properties"] = properties

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    generated_headers_by_source: dict[str, list[dict[str, object]]] = {}
    for class_name, data in reflected.items():
        header = data["header"]  # type: ignore[assignment]
        generated_headers_by_source.setdefault(header, []).append(
            {
                "kind": "class",
                "name": class_name,
                "parent": data["parent"],
                "generated_body_line": data["generated_body_line"],
                "file_id": data["file_id"],
            }
        )

    for struct_name, data in reflected_structs.items():
        header = data["header"]  # type: ignore[assignment]
        generated_headers_by_source.setdefault(header, []).append(
            {
                "kind": "struct",
                "name": struct_name,
                "generated_body_line": data["generated_body_line"],
                "file_id": data["file_id"],
            }
        )

    for header, entries in generated_headers_by_source.items():
        generated_header = make_generated_header_name(header)
        file_id = make_file_id(header)
        header_lines = [
            "// Auto-generated by Scripts/GenerateReflection.py. Do not edit manually.",
            "#pragma once",
            "",
            "#undef CURRENT_FILE_ID",
            f"#define CURRENT_FILE_ID {file_id}",
            "",
        ]
        for entry in entries:
            generated_body_line = entry["generated_body_line"]
            header_lines.append(f"#define {file_id}_{generated_body_line}_GENERATED_BODY \\")
            if entry["kind"] == "class":
                class_name = entry["name"]
                parent_name = entry["parent"]
                header_lines.append("public: \\")
                header_lines.append(f"    friend const UClass* Z_Construct_UClass_{class_name}(); \\")
                header_lines.append(f"    using ThisClass = {class_name}; \\")
                if parent_name:
                    header_lines.append(f"    using Super = {parent_name}; \\")
                    header_lines.append("    static const UClass* StaticClass(); \\")
                    header_lines.append("    const UClass* GetClass() const override;")
                else:
                    header_lines.append("    static const UClass* StaticClass(); \\")
                    header_lines.append("    virtual const UClass* GetClass() const;")
            else:
                header_lines.append("public: \\")
                header_lines.append("    static const UScriptStruct* StaticStruct();")
            header_lines.append("")

        write_if_changed(OUTPUT_DIR / generated_header, "\n".join(header_lines))

    lines: list[str] = [
        "// Auto-generated by Scripts/GenerateReflection.py. Do not edit manually.",
        "",
        "#include <cstddef>",
        "#include \"Object/ObjectFactory.h\"",
        "#include \"Reflection/Reflection.h\"",
        "",
    ]

    include_headers = []
    for collection in (reflected_structs, reflected_enums, reflected):
        for data in collection.values():
            header = data["header"]
            if header not in include_headers:
                include_headers.append(header)

    for header in include_headers:
        lines.append(f"#include \"{header}\"")

    lines.append("")
    for class_name in reflected:
        lines.append(f"const UClass* Z_Construct_UClass_{class_name}();")
    for struct_name in reflected_structs:
        lines.append(f"const UScriptStruct* Z_Construct_UScriptStruct_{struct_name}();")
    for enum_name in reflected_enums:
        lines.append(f"const UEnum* Z_Construct_UEnum_{enum_name}();")

    lines.append("")
    for class_name in reflected:
        lines.append(f"const UClass* {class_name}::StaticClass()")
        lines.append("{")
        lines.append(f"    return Z_Construct_UClass_{class_name}();")
        lines.append("}")
        lines.append("")
        lines.append(f"const UClass* {class_name}::GetClass() const")
        lines.append("{")
        lines.append(f"    return {class_name}::StaticClass();")
        lines.append("}")
        lines.append("")

    for struct_name in reflected_structs:
        lines.append(f"const UScriptStruct* {struct_name}::StaticStruct()")
        lines.append("{")
        lines.append(f"    return Z_Construct_UScriptStruct_{struct_name}();")
        lines.append("}")
        lines.append("")

    if reflected:
        lines.append("namespace")
        lines.append("{")
        for class_name, data in reflected.items():
            if data["factory_excluded"]:
                continue
            lines.append(f"struct FAutoRegisterFactory_{class_name}")
            lines.append("{")
            lines.append(f"    FAutoRegisterFactory_{class_name}()")
            lines.append("    {")
            lines.append("        FObjectFactory::Get().Register(")
            lines.append(f"            \"{class_name}\",")
            lines.append(f"            []() -> UObject* {{ return UObjectManager::Get().CreateObject<{class_name}>(); }},")
            lines.append(f"            []() -> const UClass* {{ return {class_name}::StaticClass(); }});")
            lines.append("    }")
            lines.append("};")
            lines.append("")
            lines.append(f"FAutoRegisterFactory_{class_name} GAutoRegisterFactory_{class_name};")
            lines.append("")
        lines.append("}")
        lines.append("")

    def emit_property_declarations(owner_name: str, properties: list[dict[str, str]]) -> None:
        for index, prop in enumerate(properties):
            property_var = f"Property_{prop['name']}_{index}"
            common_args = (
                f"\"{prop['name']}\", {prop['display_name']}, {prop['serialize_name']}, "
                f"offsetof({owner_name}, {prop['name']}), "
                f"EPropertyAccess::{prop['access']}"
            )
            trailing_args = (
                f"{make_float_literal(prop['min'])}, "
                f"{make_float_literal(prop['max'])}, "
                f"{make_float_literal(prop['speed'])}, "
                f"{prop['usage_flags']}"
            )
            if prop["property_class"] in {"FObjectProperty", "FStructProperty", "FEnumProperty"}:
                lines.append(
                    f"    static const {prop['property_class']} {property_var}("
                    f"{common_args}, {prop['object_type']}, {trailing_args});"
                )
            else:
                lines.append(
                    f"    static const {prop['property_class']} {property_var}("
                    f"{common_args}, {trailing_args});"
                )

    def emit_property_array(properties: list[dict[str, str]]) -> None:
        if not properties:
            lines.append("    static const FProperty* const* Properties = nullptr;")
            lines.append("    static const uint32 PropertyCount = 0;")
            return

        lines.append("    static const FProperty* Properties[] =")
        lines.append("    {")
        for index, prop in enumerate(properties):
            lines.append(f"        &Property_{prop['name']}_{index},")
        lines.append("    };")
        lines.append("    static const uint32 PropertyCount = static_cast<uint32>(sizeof(Properties) / sizeof(Properties[0]));")

    for enum_name, data in reflected_enums.items():
        values = data["values"]  # type: ignore[assignment]
        lines.append(f"const UEnum* Z_Construct_UEnum_{enum_name}()")
        lines.append("{")
        lines.append("    static const char* Names[] =")
        lines.append("    {")
        for value in values:
            lines.append(f"        \"{value['display_name']}\",")
        lines.append("    };")
        lines.append("")
        lines.append("    static const FEnumValue Values[] =")
        lines.append("    {")
        for value in values:
            lines.append(f"        {{ \"{value['name']}\", static_cast<int64>({enum_name}::{value['name']}) }},")
        lines.append("    };")
        lines.append("")
        lines.append("    static UEnum* EnumInfo = nullptr;")
        lines.append("    if (!EnumInfo)")
        lines.append("    {")
        lines.append(f"        EnumInfo = new UEnum(\"{enum_name}\", Names, Values, static_cast<uint32>(sizeof(Values) / sizeof(Values[0])));")
        lines.append("    }")
        lines.append("    return EnumInfo;")
        lines.append("}")
        lines.append("")

    for struct_name, data in reflected_structs.items():
        properties = data["properties"]  # type: ignore[assignment]
        lines.append(f"const UScriptStruct* Z_Construct_UScriptStruct_{struct_name}()")
        lines.append("{")
        emit_property_declarations(struct_name, properties)
        emit_property_array(properties)
        lines.append("")
        lines.append("    static UScriptStruct* StructInfo = nullptr;")
        lines.append("    if (!StructInfo)")
        lines.append("    {")
        lines.append(f"        StructInfo = new UScriptStruct(\"{struct_name}\", sizeof({struct_name}), Properties, PropertyCount);")
        lines.append("    }")
        lines.append("    return StructInfo;")
        lines.append("}")
        lines.append("")

    for class_name, data in reflected.items():
        properties = data["properties"]
        parent_name = data["parent"]
        super_class = f"{parent_name}::StaticClass()" if parent_name else "nullptr"
        lines.append(f"const UClass* Z_Construct_UClass_{class_name}()")
        lines.append("{")
        emit_property_declarations(class_name, properties)  # type: ignore[arg-type]
        emit_property_array(properties)  # type: ignore[arg-type]
        lines.append("")
        lines.append("    static UClass* ClassInfo = nullptr;")
        lines.append("    if (!ClassInfo)")
        lines.append("    {")
        lines.append(f"        ClassInfo = new UClass(\"{class_name}\", {super_class}, sizeof({class_name}), Properties, PropertyCount);")
        lines.append("    }")
        lines.append("    return ClassInfo;")
        lines.append("}")
        lines.append("")

    write_if_changed(OUTPUT_CPP, "\n".join(lines))
    print(
        f"Generated {OUTPUT_CPP.relative_to(ROOT)} with "
        f"{len(reflected)} reflected classes, "
        f"{len(reflected_structs)} reflected structs, and "
        f"{len(reflected_enums)} reflected enums."
    )

if __name__ == "__main__":
    main()
