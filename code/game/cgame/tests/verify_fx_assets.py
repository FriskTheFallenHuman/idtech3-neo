"""Validate every Doom effect/media dependency and reject Hexen gameplay content."""

from pathlib import Path
import re
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[3]


def main():
    base = ROOT / "base"
    definitions = sorted((base / "effects/doom").glob("*.efx"))
    if len(definitions) != 72:
        raise RuntimeError(f"Expected 72 effect definitions, found {len(definitions)}")
    shaders = (base / "scripts/doom_effects.shader").read_text()
    declared = set(re.findall(r"^(doom/fx/\w+)\s*$", shaders, re.MULTILINE))
    packed = set()
    for pack in base.glob("*.pk3"):
        with ZipFile(pack) as archive:
            packed.update(archive.namelist())
    for asset in re.findall(r"\bmap\s+(\S+)", shaders):
        if asset != "$whiteimage" and not (base / asset).is_file() and asset not in packed:
            raise RuntimeError(f"Missing effect texture: {asset}")
    for definition in definitions:
        text = definition.read_text()
        if re.search(r"\bh2/|magic|rune|artifact|spell|tome", text, re.IGNORECASE):
            raise RuntimeError(f"Hexen gameplay reference in {definition}")
        for shader in re.findall(r"doom/fx/\w+", text):
            if shader not in declared:
                raise RuntimeError(f"Undeclared effect shader {shader} in {definition}")
        for reference in re.findall(r"\b(?:impactfx|deathfx|emitfx)\s+(\S+)", text):
            if not (base / "effects" / f"{reference}.efx").is_file():
                raise RuntimeError(f"Missing child effect {reference} in {definition}")
        liquid = any(definition.stem.endswith(f"_{name}") for name in ("water", "slime", "lava"))
        if liquid and re.search(r"^decal\b", text, re.MULTILINE):
            raise RuntimeError(f"Liquid hit incorrectly leaves a solid mark: {definition}")
    for path in (ROOT / "code/renderer").glob("tr_*.cpp"):
        if path.name not in ("tr_vfx.cpp", "tr_decals.cpp", "tr_hbao.cpp"):
            continue
        for shader in re.findall(r'"(glprogs/[^\"]+\.(?:vert|frag))"', path.read_text()):
            if not (base / shader).is_file():
                raise RuntimeError(f"Missing GLSL program: {shader}")
    print(
        "Doom FX assets: 72 definitions, child effects, physical media and GLSL dependencies passed."
    )


if __name__ == "__main__":
    main()
