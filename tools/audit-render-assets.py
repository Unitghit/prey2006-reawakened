"""Read-only inventory of retail renderer features; never extracts game assets."""
import argparse
import collections
import json
import re
import zipfile
from pathlib import Path


def declarations(text):
    # Preserve quoted strings while removing comments and tracking braces.
    text = re.sub(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*.*?\*/',
                  lambda m: m[0] if m[0].startswith('"') else " ", text, flags=re.S)
    tokens = re.findall(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*.*?\*/|[{}]|[^\s{}]+', text, re.S)
    depth, name, body = 0, [], []
    for token in tokens:
        if token.startswith(("//", "/*")):
            continue
        if token == "{":
            if depth == 0:
                body = []
            depth += 1
        elif token == "}":
            depth -= 1
            if depth == 0:
                yield " ".join(name), " ".join(body)
                name = []
                continue
        if depth:
            body.append(token)
        elif token not in ("{", "}"):
            name.append(token)


def inventory(base):
    archives = sorted(base.glob("*.pk4"), key=lambda p: p.name.lower())
    files = {}
    for archive in archives:
        with zipfile.ZipFile(archive) as z:
            for entry in z.infolist():
                if not entry.is_dir():
                    files[entry.filename.lower()] = (archive, entry.filename)
    features = collections.defaultdict(list)
    programs = collections.Counter()
    extensions = collections.Counter()
    material_files = materials = 0
    opened = {}
    try:
        for name, (archive, entry) in sorted(files.items()):
            if name.startswith("models/"):
                extension = Path(name).suffix
                if extension in (".ase", ".lwo", ".md5mesh", ".ma", ".flt"):
                    extensions[extension] += 1
            if not name.endswith(".mtr"):
                continue
            if archive not in opened:
                opened[archive] = zipfile.ZipFile(archive)
            material_files += 1
            text = opened[archive].read(entry).decode("latin1")
            for material, body in declarations(text):
                if material.lower().startswith("table "):
                    continue
                materials += 1
                for keyword in ("shaderFallback1", "shaderFallback2", "shaderFallback3",
                                "shaderLevel1", "shaderLevel2", "shaderLevel3",
                                "highres", "shuttleView",
                                "spiritWalk", "notSpiritWalk", "scopeView"):
                    if re.search(r"\b" + keyword + r"\b", body, re.I):
                        features[keyword].append(material)
                for keyword in ("growIn", "growOut"):
                    if re.search(r"\b" + keyword + r"\s*\[", body, re.I):
                        features[keyword + " table reference"].append(material)
                    if re.search(r"\b" + keyword + r"\b(?!\s*\[)", body, re.I):
                        features[keyword + " non-table occurrence"].append(material)
                for kind in re.findall(r"\bdeform\s+(\w+)", body, re.I):
                    features["deform " + kind.lower()].append(material)
                if re.search(r"\bblend\s+shader\b", body, re.I):
                    features["blend shader"].append(material)
                    if not re.search(r"\b(?:diffusemap|specularmap)\b", body, re.I):
                        features["custom shader without lit fallback"].append(material)
                    programs.update(re.findall(r"\bprogram\s+([^ {}]+)", body, re.I))
    finally:
        for z in opened.values():
            z.close()
    return {"archive_count": len(archives), "material_files": material_files,
            "material_declarations": materials, "model_files": dict(extensions),
            "custom_interaction_program_occurrences": dict(programs),
            "features": {k: {"count": len(v), "materials": v}
                         for k, v in sorted(features.items())}}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base", type=Path, help="Folder containing retail pk4 archives")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = json.dumps(inventory(args.base), indent=2)
    if args.output:
        args.output.write_text(result + "\n", encoding="utf-8")
    else:
        print(result)
