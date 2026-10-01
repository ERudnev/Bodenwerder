"""Build dual-scale facies PNG pairs: high (landscape) and low (close)."""

from __future__ import annotations

import json
import shutil
import tempfile
import urllib.request
import zipfile
from pathlib import Path

from PIL import Image, ImageEnhance, ImageOps, ImageStat

ROOT = Path(__file__).resolve().parent
HIGH_DIR = ROOT / "high"
LOW_DIR = ROOT / "low"
SIZE = 1024
USER_AGENT = "DAQL-Eltanin-facies/1.0"

NAMES = [
    "00_Snow",
    "01_Glacier",
    "02_DirtyIce",
    "03_VolatileFrost",
    "04_Hydrate",
    "05_Dunite",
    "06_Peridotite",
    "07_Pyroxenite",
    "08_Komatiite",
    "09_Basalt",
    "10_Gabbro",
    "11_Andesite",
    "12_Granite",
    "13_Rhyolite",
    "14_Obsidian",
    "15_Anorthosite",
    "16_ClayPan",
    "17_Laterite",
    "18_Arenite",
    "19_Evaporite",
    "20_Carbonate",
    "21_Chondrite",
    "22_Tholin",
    "23_Bitumen",
    "24_IronMetal",
    "25_NickelMetal",
    "26_Sulfide",
    "27_SulfurPlains",
    "28_SO2Frost",
    "29_Fumarole",
    "30_Hematite",
    "31_Magnetite",
    "32_DesertVarnish",
    "33_BaseMetal",
    "34_Porphyry",
    "35_PGMLag",
    "36_REELaterite",
    "37_Actinide",
    "38_Pahoehoe",
    "39_Scoria",
    "40_Pumice",
    "41_SilicaSinter",
    "42_RegolithMafic",
    "43_RegolithFelsic",
    "44_Breccia",
    "45_Pegmatite",
    "46_Exotic",
    "47_Caliche",
]

SEED_LOW = {
    "00_Snow",
    "02_DirtyIce",
    "03_VolatileFrost",
    "04_Hydrate",
    "07_Pyroxenite",
    "12_Granite",
    "16_ClayPan",
    "17_Laterite",
    "18_Arenite",
    "19_Evaporite",
    "21_Chondrite",
    "22_Tholin",
    "23_Bitumen",
    "25_NickelMetal",
    "29_Fumarole",
    "30_Hematite",
    "33_BaseMetal",
    "35_PGMLag",
    "36_REELaterite",
    "37_Actinide",
    "42_RegolithMafic",
    "43_RegolithFelsic",
    "45_Pegmatite",
    "46_Exotic",
    "47_Caliche",
}

SEED_HIGH = set(NAMES) - SEED_LOW

# Missing high (landscape) counterparts for seed-low slots.
FETCH_HIGH = {
    "02_DirtyIce": ("Ice003", {"mul": (0.72, 0.76, 0.70)}),
    "03_VolatileFrost": ("Ice001", {"mul": (0.88, 0.94, 1.05), "saturation": 0.35}),
    "04_Hydrate": ("Ice003", {"mul": (0.78, 0.82, 0.80)}),
    "07_Pyroxenite": ("Rock028", {"mul": (0.42, 0.38, 0.36)}),
    "12_Granite": ("Rock003", {}),
    "16_ClayPan": ("Ground033", {"mul": (0.86, 0.62, 0.42)}),
    "17_Laterite": ("Ground033", {"mul": (1.15, 0.55, 0.32)}),
    "18_Arenite": ("Ground054", {}),
    "19_Evaporite": ("Travertine001", {"brightness": 1.12}),
    "21_Chondrite": ("Rock022", {"mul": (0.22, 0.20, 0.18), "brightness": 0.45}),
    "22_Tholin": ("Rock029", {"mul": (1.15, 0.42, 0.22), "saturation": 1.25}),
    "23_Bitumen": ("Rock022", {"mul": (0.22, 0.18, 0.16), "brightness": 0.45, "rough": 0.55}),
    "25_NickelMetal": ("Rock020", {"mul": (0.72, 0.70, 0.64)}),
    "29_Fumarole": ("Ground033", {"mul": (1.1, 0.95, 0.35)}),
    "30_Hematite": ("Rock029", {"mul": (1.05, 0.42, 0.22)}),
    "33_BaseMetal": ("Rock036", {}),
    "35_PGMLag": ("Rock020", {"mul": (0.62, 0.60, 0.58)}),
    "36_REELaterite": ("Ground033", {"mul": (0.85, 0.62, 0.40)}),
    "37_Actinide": ("Rock023", {"mul": (0.42, 0.48, 0.32)}),
    "42_RegolithMafic": ("Ground054", {"mul": (0.55, 0.52, 0.48)}),
    "43_RegolithFelsic": ("Ground054", {"mul": (0.85, 0.82, 0.76)}),
    "45_Pegmatite": ("Rock003", {"contrast": 1.12}),
    "46_Exotic": ("Rock013", {"mul": (0.72, 0.42, 0.95)}),
    "47_Caliche": ("Ground054", {"mul": (0.95, 0.90, 0.78), "brightness": 1.05}),
}

BLUR_HIGH = {"00_Snow"}

# Missing low (close grain) counterparts for seed-high slots.
FETCH_LOW = {
    "01_Glacier": ("Ice004", {"contrast": 1.08, "brightness": 0.92}),
    "05_Dunite": ("Rock023", {"mul": (0.72, 0.82, 0.55)}),
    "06_Peridotite": ("Rock035", {"mul": (0.72, 0.82, 0.55)}),
    "08_Komatiite": ("Rock028", {"mul": (0.55, 0.62, 0.42)}),
    "09_Basalt": ("Rock034", {"mul": (0.55, 0.52, 0.50), "brightness": 0.72}),
    "10_Gabbro": ("Rock036", {"contrast": 1.12}),
    "11_Andesite": ("Rock030", {"mul": (0.78, 0.74, 0.70)}),
    "13_Rhyolite": ("Rock050", {"mul": (0.92, 0.88, 0.82), "brightness": 1.08}),
    "14_Obsidian": ("Rock034", {"mul": (0.18, 0.16, 0.20), "brightness": 0.45, "rough": 0.22}),
    "15_Anorthosite": ("Marble006", {"mul": (0.95, 0.93, 0.90), "brightness": 1.12}),
    "20_Carbonate": ("Travertine001", {"mul": (0.96, 0.94, 0.88)}),
    "24_IronMetal": ("Metal007", {}),
    "26_Sulfide": ("Rock006", {"mul": (0.82, 0.68, 0.28)}),
    "27_SulfurPlains": ("Ground032", {"mul": (1.35, 1.18, 0.22), "saturation": 1.4}),
    "28_SO2Frost": ("Snow001", {"mul": (0.95, 0.98, 1.05), "brightness": 1.15, "saturation": 0.2, "rough": 0.35}),
    "31_Magnetite": ("Rock034", {"mul": (0.28, 0.28, 0.30), "brightness": 0.5}),
    "32_DesertVarnish": ("Ground042", {"mul": (0.42, 0.32, 0.22), "brightness": 0.62}),
    "34_Porphyry": ("Rock020", {}),
    "38_Pahoehoe": ("Rock034", {}),
    "39_Scoria": ("Rock034", {"brightness": 0.7}),
    "40_Pumice": ("Rock003", {"mul": (0.92, 0.90, 0.84), "brightness": 1.1}),
    "41_SilicaSinter": ("Travertine001", {"brightness": 1.08}),
    "44_Breccia": ("Rock036", {"contrast": 1.12}),
}

ALTERNATES = {
    "00_Snow": "Snow005",
    "01_Glacier": "Ice003",
    "02_DirtyIce": "Ice001",
    "03_VolatileFrost": "Snow005",
    "04_Hydrate": "Ice001",
    "05_Dunite": "Rock035",
    "07_Pyroxenite": "Rock022",
    "09_Basalt": "Rock022",
    "12_Granite": "Rock008",
    "14_Obsidian": "Rock022",
    "15_Anorthosite": "Marble001",
    "16_ClayPan": "Ground054",
    "17_Laterite": "Ground037",
    "18_Arenite": "Ground054",
    "19_Evaporite": "Marble001",
    "20_Carbonate": "Marble001",
    "21_Chondrite": "Rock028",
    "22_Tholin": "Ground013",
    "23_Bitumen": "Asphalt012",
    "24_IronMetal": "Metal032",
    "25_NickelMetal": "Metal032",
    "26_Sulfide": "Rock036",
    "27_SulfurPlains": "Rock006",
    "28_SO2Frost": "Snow006",
    "30_Hematite": "Ground037",
    "31_Magnetite": "Rock022",
    "32_DesertVarnish": "Rock029",
    "36_REELaterite": "Ground037",
    "38_Pahoehoe": "Lava002",
    "39_Scoria": "Lava001",
    "40_Pumice": "Rock013",
    "41_SilicaSinter": "Travertine002",
    "42_RegolithMafic": "Ground042",
    "43_RegolithFelsic": "Ground048",
    "45_Pegmatite": "Rock008",
    "46_Exotic": "Rock003",
    "47_Caliche": "Ground033",
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


def _srgb_tables() -> tuple[list[float], bytearray]:
    to_lin = []
    for i in range(256):
        u = i / 255.0
        to_lin.append(u / 12.92 if u <= 0.04045 else ((u + 0.055) / 1.055) ** 2.4)
    to_srgb = bytearray(65536)
    inv = 1.0 / 2.4
    for i in range(65536):
        x = i / 65535.0
        u = x * 12.92 if x <= 0.0031308 else 1.055 * (x ** inv) - 0.055
        to_srgb[i] = int(max(0.0, min(1.0, u)) * 255.0 + 0.5)
    return to_lin, to_srgb


TO_LIN, TO_SRGB = _srgb_tables()


def encode_lin(value: float) -> int:
    if value <= 0.0:
        return 0
    if value >= 1.0:
        return 255
    return TO_SRGB[int(value * 65535.0 + 0.5)]


def linear_luma_mean(color: Image.Image) -> float:
    pix = as_rgb(color).tobytes()
    total = 0.0
    lin = TO_LIN
    for i in range(0, len(pix), 3):
        total += 0.2126 * lin[pix[i]] + 0.7152 * lin[pix[i + 1]] + 0.0722 * lin[pix[i + 2]]
    return total / max(len(pix) // 3, 1)


def apply_linear_luma(color: Image.Image, factor: float, lift: float) -> Image.Image:
    color = as_rgb(color)
    pix = bytearray(color.tobytes())
    lin = TO_LIN
    for i in range(0, len(pix), 3):
        pix[i] = encode_lin(lin[pix[i]] * factor + lift)
        pix[i + 1] = encode_lin(lin[pix[i + 1]] * factor + lift)
        pix[i + 2] = encode_lin(lin[pix[i + 2]] * factor + lift)
    return Image.frombytes("RGB", color.size, bytes(pix))


def match_color_luma(color: Image.Image, target: Image.Image) -> Image.Image:
    color = as_rgb(color)
    src_y = linear_luma_mean(color)
    dst_y = linear_luma_mean(target)
    factor = 1.0 if src_y < 1.0e-6 else dst_y / src_y
    scaled = apply_linear_luma(color, factor, 0.0)
    lift = dst_y - linear_luma_mean(scaled)
    if abs(lift) < 0.003:
        return scaled
    return apply_linear_luma(scaled, 1.0, lift)


def match_roughness(rough: Image.Image, target: Image.Image) -> Image.Image:
    src = as_gray(rough)
    dst = as_gray(target)
    src_stat = ImageStat.Stat(src)
    dst_stat = ImageStat.Stat(dst)
    sm, ss = src_stat.mean[0], src_stat.stddev[0]
    tm, ts = dst_stat.mean[0], dst_stat.stddev[0]
    if ss < 3.0 and ts >= 8.0:
        return dst
    if sm < 1.0:
        return Image.new("L", src.size, int(tm + 0.5))
    k = tm / sm
    lut = [max(0, min(255, int(i * k + 0.5))) for i in range(256)]
    return src.point(lut)


def match_to_sibling(color: Image.Image, rough: Image.Image, sibling: Path) -> tuple[Image.Image, Image.Image]:
    if not sibling.exists():
        return color, rough
    target = Image.open(sibling).convert("RGBA")
    target_rough = target.split()[3] if "A" in target.getbands() else Image.new("L", target.size, 180)
    return match_color_luma(color, target.convert("RGB")), match_roughness(rough, target_rough)


def calibrate_pairs() -> None:
    for name in NAMES:
        high_path = HIGH_DIR / f"{name}.png"
        low_path = LOW_DIR / f"{name}.png"
        if name in SEED_LOW:
            seed_path, other_path, other_dir = low_path, high_path, HIGH_DIR
        else:
            seed_path, other_path, other_dir = high_path, low_path, LOW_DIR
        other = Image.open(other_path).convert("RGBA")
        seed = Image.open(seed_path).convert("RGBA")
        color = other.convert("RGB")
        rough = other.split()[3]
        seed_color = seed.convert("RGB")
        seed_rough = seed.split()[3]
        src_y = linear_luma_mean(color)
        dst_y = linear_luma_mean(seed_color)
        src_a = ImageStat.Stat(rough).mean[0]
        dst_a = ImageStat.Stat(seed_rough).mean[0]
        color = match_color_luma(color, seed_color)
        rough = match_roughness(rough, seed_rough)
        save_rgba(other_dir, name, color, rough)
        new_y = linear_luma_mean(color)
        new_a = ImageStat.Stat(as_gray(rough)).mean[0]
        y0 = src_y / dst_y if dst_y > 1.0e-6 else 0.0
        y1 = new_y / dst_y if dst_y > 1.0e-6 else 0.0
        print(f"calibrate {other_dir.name}/{name} Y {y0:.2f}→{y1:.2f} a {src_a:.0f}→{new_a:.0f} seed {dst_a:.0f}", flush=True)


def save_rgba(directory: Path, name: str, color: Image.Image, rough: Image.Image) -> Path:
    color = as_rgb(color)
    rough = as_gray(rough)
    out = color.convert("RGBA")
    out.putalpha(rough)
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / f"{name}.png"
    out.save(path, "PNG", optimize=True)
    return path


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
    if not zip_path.exists():
        print(f"download {zip_name}", flush=True)
        with open_url(f"https://ambientcg.com/get?file={zip_name}") as response, zip_path.open("wb") as out:
            shutil.copyfileobj(response, out)
    extracted = work / f"{asset_id}_out"
    if not extracted.exists():
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


def blur_to_landscape(sibling: Path) -> Image.Image:
    image = Image.open(sibling).convert("RGBA")
    small = image.resize((48, 48), Image.Resampling.LANCZOS)
    return small.resize((SIZE, SIZE), Image.Resampling.LANCZOS)


def fetch_one(name: str, asset_id: str, grade: dict, directory: Path, sibling: Path, work: Path, sources: dict[str, str]) -> None:
    tried = [asset_id]
    if name in ALTERNATES:
        tried.append(ALTERNATES[name])
    last_error = None
    for candidate in tried:
        try:
            color, rough = fetch_asset(candidate, work)
            color, rough = apply_grade(as_rgb(color), as_gray(rough), grade)
            color, rough = match_to_sibling(as_rgb(color), as_gray(rough), sibling)
            save_rgba(directory, name, color, rough)
            note = f"ambientCG {candidate}"
            if grade:
                note += " + grade"
            if sibling.exists():
                note += " + match"
            sources[name] = note
            print(f"ok {directory.name}/{name} ← {candidate}", flush=True)
            return
        except Exception as error:
            last_error = error
            print(f"fail {name} {candidate}: {error}", flush=True)
    raise last_error


def write_sources(directory: Path, sources: dict[str, str], seed: set[str]) -> None:
    lines = ["# Facies texpack sources (CC0 ambientCG + existing seed). RGB albedo + A roughness.", ""]
    for name in NAMES:
        if name in seed:
            origin = "seed (existing)"
        elif name in BLUR_HIGH and directory == HIGH_DIR:
            origin = sources.get(name, "blurred low sibling")
        else:
            origin = sources.get(name, "?")
        lines.append(f"{name}.png\t{origin}")
    (directory / "sources.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    HIGH_DIR.mkdir(parents=True, exist_ok=True)
    LOW_DIR.mkdir(parents=True, exist_ok=True)
    high_sources: dict[str, str] = {}
    low_sources: dict[str, str] = {}
    with tempfile.TemporaryDirectory(prefix="facies-acg-") as tmp:
        work = Path(tmp)
        for name in BLUR_HIGH:
            path = HIGH_DIR / f"{name}.png"
            sibling = LOW_DIR / f"{name}.png"
            if path.exists():
                high_sources[name] = "cached"
                print(f"skip high/{name}", flush=True)
                continue
            if not sibling.exists():
                raise SystemExit(f"blur high/{name} needs low sibling")
            blurred = blur_to_landscape(sibling)
            color = blurred.convert("RGB")
            rough = blurred.split()[3] if blurred.mode == "RGBA" else Image.new("L", blurred.size, 180)
            save_rgba(HIGH_DIR, name, color, rough)
            high_sources[name] = "blurred low sibling"
            print(f"ok high/{name} ← blur {sibling.name}", flush=True)
        for name, (asset_id, grade) in FETCH_HIGH.items():
            path = HIGH_DIR / f"{name}.png"
            if path.exists():
                high_sources[name] = "cached"
                print(f"skip high/{name}", flush=True)
                continue
            fetch_one(name, asset_id, grade, HIGH_DIR, LOW_DIR / f"{name}.png", work, high_sources)
        for name, (asset_id, grade) in FETCH_LOW.items():
            path = LOW_DIR / f"{name}.png"
            if path.exists():
                low_sources[name] = "cached"
                print(f"skip low/{name}", flush=True)
                continue
            fetch_one(name, asset_id, grade, LOW_DIR, HIGH_DIR / f"{name}.png", work, low_sources)
    calibrate_pairs()
    if any(origin != "cached" for origin in list(high_sources.values()) + list(low_sources.values())):
        write_sources(HIGH_DIR, high_sources, SEED_HIGH)
        write_sources(LOW_DIR, low_sources, SEED_LOW)
    missing_high = [name for name in NAMES if not (HIGH_DIR / f"{name}.png").exists()]
    missing_low = [name for name in NAMES if not (LOW_DIR / f"{name}.png").exists()]
    if missing_high or missing_low:
        raise SystemExit(f"missing high={missing_high} low={missing_low}")
    print(f"wrote high={len(list(HIGH_DIR.glob('*.png')))} low={len(list(LOW_DIR.glob('*.png')))}", flush=True)


if __name__ == "__main__":
    main()
