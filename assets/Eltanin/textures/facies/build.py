"""Build 48 RGBA facies PNGs: legacy crust pairs + ambientCG Color/Roughness."""

from __future__ import annotations

import json
import shutil
import tempfile
import urllib.request
import zipfile
from pathlib import Path

from PIL import Image, ImageEnhance, ImageOps

ROOT = Path(__file__).resolve().parent
CRUST = ROOT.parent / "crust"
SIZE = 1024
USER_AGENT = "DAQL-Eltanin-facies/1.0"

LEGACY = {
    "00_Snow": "00_ice_close",
    "05_Dunite": "01_olivine_close",
    "07_Pyroxenite": "02_pyroxene_close",
    "12_Granite": "03_feldspar_close",
    "16_ClayPan": "04_clay_close",
    "19_Evaporite": "14_salts_close",
    "21_Chondrite": "05_carbonaceous_close",
    "24_IronMetal": "06_iron_close",
    "25_NickelMetal": "07_nickel_close",
    "26_Sulfide": "08_sulfides_close",
    "30_Hematite": "09_oxides_close",
    "33_BaseMetal": "10_basemetals_close",
    "35_PGMLag": "11_pgm_close",
    "36_REELaterite": "12_rareearths_close",
    "37_Actinide": "13_actinides_close",
    "46_Exotic": "15_exotic_close",
}

# asset, optional grade kwargs
AMBIENT = {
    "01_Glacier": ("Ice002", {"contrast": 1.08, "brightness": 0.92}),
    "02_DirtyIce": ("Snow003", {"mul": (0.72, 0.76, 0.70), "brightness": 0.82}),
    "03_VolatileFrost": ("Snow001", {"mul": (0.88, 0.94, 1.05), "saturation": 0.35}),
    "04_Hydrate": ("Ice004", {"mul": (0.78, 0.82, 0.80), "brightness": 0.78}),
    "06_Peridotite": ("Rock023", {"mul": (0.72, 0.82, 0.55)}),
    "08_Komatiite": ("Rock035", {"mul": (0.55, 0.62, 0.42)}),
    "09_Basalt": ("Rock028", {"mul": (0.55, 0.52, 0.50), "brightness": 0.72}),
    "10_Gabbro": ("Rock020", {"contrast": 1.15}),
    "11_Andesite": ("Rock025", {"mul": (0.78, 0.74, 0.70)}),
    "13_Rhyolite": ("Rock032", {"mul": (0.92, 0.88, 0.82), "brightness": 1.08}),
    "14_Obsidian": ("Rock022", {"mul": (0.18, 0.16, 0.20), "brightness": 0.45, "rough": 0.22}),
    "15_Anorthosite": ("Rock008", {"mul": (0.95, 0.93, 0.90), "brightness": 1.12}),
    "17_Laterite": ("Ground037", {"mul": (1.15, 0.55, 0.32)}),
    "18_Arenite": ("Ground054", {}),
    "20_Carbonate": ("Marble001", {"mul": (0.96, 0.94, 0.88)}),
    "22_Tholin": ("Ground013", {"mul": (1.15, 0.42, 0.22), "saturation": 1.25}),
    "23_Bitumen": ("Asphalt012", {"mul": (0.22, 0.18, 0.16), "brightness": 0.45, "rough": 0.55}),
    "27_SulfurPlains": ("Rock006", {"mul": (1.35, 1.18, 0.22), "saturation": 1.4}),
    "28_SO2Frost": ("Snow006", {"mul": (0.95, 0.98, 1.05), "brightness": 1.15, "saturation": 0.2, "rough": 0.35}),
    "29_Fumarole": ("Ground032", {"mul": (1.1, 0.95, 0.35)}),
    "31_Magnetite": ("Rock022", {"mul": (0.28, 0.28, 0.30), "brightness": 0.5}),
    "32_DesertVarnish": ("Rock029", {"mul": (0.42, 0.32, 0.22), "brightness": 0.62}),
    "34_Porphyry": ("Rock036", {}),
    "38_Pahoehoe": ("Lava001", {}),
    "39_Scoria": ("Lava002", {"brightness": 0.7}),
    "40_Pumice": ("Rock013", {"mul": (0.92, 0.90, 0.84), "brightness": 1.1}),
    "41_SilicaSinter": ("Travertine002", {"brightness": 1.08}),
    "42_RegolithMafic": ("Ground042", {"mul": (0.55, 0.52, 0.48)}),
    "43_RegolithFelsic": ("Ground048", {"mul": (0.85, 0.82, 0.76)}),
    "44_Breccia": ("Rock029", {"contrast": 1.12}),
    "45_Pegmatite": ("Granite002A", {"contrast": 1.2}),
    "47_Caliche": ("Ground033", {"mul": (0.95, 0.90, 0.78), "brightness": 1.05}),
}

ALTERNATES = {
    "04_Hydrate": "Ice003",
    "11_Andesite": "Rock030",
    "13_Rhyolite": "Rock050",
    "15_Anorthosite": "Marble006",
    "17_Laterite": "Ground048",
    "18_Arenite": "Ground003",
    "20_Carbonate": "Travertine001",
    "22_Tholin": "Ground037",
    "23_Bitumen": "Rubber002",
    "27_SulfurPlains": "Rock008",
    "29_Fumarole": "Ground037",
    "34_Porphyry": "Rock020",
    "39_Scoria": "Rock034",
    "40_Pumice": "Rock003",
    "41_SilicaSinter": "Travertine001",
    "45_Pegmatite": "Rock003",
    "47_Caliche": "Ground054",
}


def open_url(url: str):
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    return urllib.request.urlopen(request, timeout=120)


def resize_square(image: Image.Image) -> Image.Image:
    if image.size != (SIZE, SIZE):
        image = image.resize((SIZE, SIZE), Image.Resampling.LANCZOS)
    return image


def as_rgb(image: Image.Image) -> Image.Image:
    return resize_square(image.convert("RGB"))


def as_gray(image: Image.Image) -> Image.Image:
    return resize_square(ImageOps.grayscale(image))


def apply_grade(color: Image.Image, rough: Image.Image, grade: dict) -> tuple[Image.Image, Image.Image]:
    pixels = color.load()
    mul = grade.get("mul", (1.0, 1.0, 1.0))
    for y in range(color.height):
        for x in range(color.width):
            r, g, b = pixels[x, y]
            pixels[x, y] = (
                max(0, min(255, int(r * mul[0] + 0.5))),
                max(0, min(255, int(g * mul[1] + 0.5))),
                max(0, min(255, int(b * mul[2] + 0.5))),
            )
    if "brightness" in grade:
        color = ImageEnhance.Brightness(color).enhance(grade["brightness"])
    if "contrast" in grade:
        color = ImageEnhance.Contrast(color).enhance(grade["contrast"])
    if "saturation" in grade:
        color = ImageEnhance.Color(color).enhance(grade["saturation"])
    if "rough" in grade:
        target = int(grade["rough"] * 255 + 0.5)
        rough = Image.blend(rough, Image.new("L", rough.size, target), 0.65)
    return color, rough


def save_rgba(name: str, color: Image.Image, rough: Image.Image) -> Path:
    color = as_rgb(color)
    rough = as_gray(rough)
    out = color.convert("RGBA")
    out.putalpha(rough)
    path = ROOT / f"{name}.png"
    out.save(path, "PNG", optimize=True)
    return path


def merge_legacy() -> dict[str, str]:
    sources = {}
    for name, stem in LEGACY.items():
        existing = ROOT / f"{name}.png"
        if existing.exists():
            sources[name] = f"legacy crust/{stem}"
            print(f"skip {name}", flush=True)
            continue
        albedo = Image.open(CRUST / "albedo" / f"{stem}.bmp")
        roughness = Image.open(CRUST / "roughness" / f"{stem}.bmp")
        save_rgba(name, albedo, roughness)
        sources[name] = f"legacy crust/{stem}"
        print(f"legacy {name}", flush=True)
    return sources


def pick_zip_name(folders: dict) -> str | None:
    downloads = []
    default = folders.get("default") or next(iter(folders.values()))
    categories = default.get("downloadFiletypeCategories", {})
    zips = categories.get("zip", {}).get("downloads", [])
    for item in zips:
        downloads.append(item.get("fileName") or "")
    for wanted in ("1K-PNG.zip", "1K-JPG.zip", "2K-JPG.zip"):
        for name in downloads:
            if name.endswith(wanted):
                return name
    return downloads[0] if downloads else None


def find_map(extracted: Path, kind: str) -> Path | None:
    kind = kind.lower()
    matches = []
    for path in extracted.rglob("*"):
        if not path.is_file():
            continue
        lower = path.name.lower()
        if kind not in lower:
            continue
        if path.suffix.lower() not in {".png", ".jpg", ".jpeg", ".tga", ".bmp"}:
            continue
        if "preview" in lower or "thumb" in lower:
            continue
        matches.append(path)
    if not matches:
        return None
    matches.sort(key=lambda p: (0 if kind in p.stem.lower() else 1, len(p.name)))
    return matches[0]


def fetch_asset(asset_id: str, work: Path) -> tuple[Image.Image, Image.Image]:
    api = f"https://ambientcg.com/api/v2/full_json?id={asset_id}&include=downloadData"
    with open_url(api) as response:
        payload = json.loads(response.read().decode("utf-8"))
    assets = payload.get("foundAssets") or []
    if not assets:
        raise RuntimeError(f"ambientCG unknown id {asset_id}")
    zip_name = pick_zip_name(assets[0].get("downloadFolders") or {})
    if not zip_name:
        raise RuntimeError(f"ambientCG {asset_id} has no zip")
    zip_path = work / zip_name
    print(f"download {zip_name}")
    with open_url(f"https://ambientcg.com/get?file={zip_name}") as response, zip_path.open("wb") as out:
        shutil.copyfileobj(response, out)
    extracted = work / f"{asset_id}_out"
    extracted.mkdir(exist_ok=True)
    with zipfile.ZipFile(zip_path) as archive:
        archive.extractall(extracted)
    color_path = find_map(extracted, "color") or find_map(extracted, "albedo") or find_map(extracted, "diffuse")
    rough_path = find_map(extracted, "roughness")
    if not color_path:
        raise RuntimeError(f"ambientCG {asset_id} missing color map")
    color = Image.open(color_path)
    rough = Image.open(rough_path) if rough_path else Image.new("L", color.size, 180)
    return color, rough


def fetch_ambient(sources: dict[str, str]) -> None:
    with tempfile.TemporaryDirectory(prefix="facies-acg-") as tmp:
        work = Path(tmp)
        for name, (asset_id, grade) in AMBIENT.items():
            tried = [asset_id]
            if name in ALTERNATES:
                tried.append(ALTERNATES[name])
            existing = ROOT / f"{name}.png"
            if existing.exists():
                sources[name] = f"ambientCG {asset_id} (cached)"
                print(f"skip {name}", flush=True)
                continue
            last_error = None
            for candidate in tried:
                try:
                    color, rough = fetch_asset(candidate, work)
                    color, rough = apply_grade(as_rgb(color), as_gray(rough), grade)
                    save_rgba(name, color, rough)
                    note = f"ambientCG {candidate}"
                    if grade:
                        note += " + grade"
                    sources[name] = note
                    print(f"ok {name} ← {candidate}", flush=True)
                    last_error = None
                    break
                except Exception as error:
                    last_error = error
                    print(f"fail {name} {candidate}: {error}")
            if last_error:
                raise last_error


def write_sources(sources: dict[str, str]) -> None:
    lines = ["# Facies texpack sources (CC0 ambientCG + legacy crust)", ""]
    for index in range(48):
        name = None
        for key in list(LEGACY) + list(AMBIENT):
            if key.startswith(f"{index:02d}_"):
                name = key
                break
        if name is None:
            continue
        lines.append(f"{name}.png\t{sources.get(name, '?')}")
    (ROOT / "sources.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    ROOT.mkdir(parents=True, exist_ok=True)
    sources = merge_legacy()
    fetch_ambient(sources)
    write_sources(sources)
    missing = [f"{index:02d}" for index in range(48) if not any(ROOT.glob(f"{index:02d}_*.png"))]
    if missing:
        raise SystemExit(f"missing slots: {missing}")
    print(f"wrote {len(list(ROOT.glob('*.png')))} png")


if __name__ == "__main__":
    main()
