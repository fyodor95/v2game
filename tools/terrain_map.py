"""Сборка атласов, русской легенды и карты по явным типам провинций.

Требуется Pillow. --baseline указывает сохранённый исходный terrain.bmp;
рядом ожидаются terrain.baseline.txt и texturesheet.baseline.tga.
"""
import argparse
import hashlib
import io
import os
import struct
import tempfile
from functools import lru_cache
import json
from pathlib import Path
import re

from PIL import Image, ImageDraw, ImageFont


def categories(text):
    tokens = [t for t in re.findall(r'"(?:\\.|[^"\\])*"|#[^\r\n]*|[{}=]|[^\s{}=#]+', text)
              if not t.startswith('#')]
    start = tokens.index('categories')
    if tokens[start + 1:start + 3] != ['=', '{']:
        raise ValueError('Не найден блок categories')
    names = []
    depth = 1
    for i in range(start + 3, len(tokens)):
        token = tokens[i]
        if depth == 1 and tokens[i + 1:i + 3] == ['=', '{']:
            names.append(token)
        depth += (token == '{') - (token == '}')
        if depth == 0:
            return names
    raise ValueError('Незакрытый блок categories')


def validate_slots(names, slots):
    if not names or len(names) > 64 or len(set(names)) != len(names):
        raise ValueError('Нужен отдельный слот для каждого типа, максимум 64')
    if set(slots) != set(names):
        raise ValueError('Состав категорий не соответствует слотам атласа')
    if any(type(slot) is not int or not 0 <= slot < 64 for slot in slots.values()):
        raise ValueError('Слоты должны быть целыми числами от 0 до 63')
    if len(set(slots.values())) != len(names):
        raise ValueError('Каждому типу нужен уникальный слот')


def tile_box(slot, size=256):
    return (slot % 8 * size, slot // 8 * size,
            slot % 8 * size + size, slot // 8 * size + size)


def split_variants(source):
    if source.width != source.height or source.width % 2 or source.width < 512:
        raise ValueError("Исходник вариантов должен быть квадратным с чётной стороной не меньше 512")
    half = source.width // 2
    return [source.crop((column * half, row * half, (column + 1) * half, (row + 1) * half))
            .convert("RGB").resize((256, 256), Image.Resampling.LANCZOS)
            for row in range(2) for column in range(2)]


def terrain_hash(x, y):
    import numpy as np
    n = y - np.floor(y / 289.0) * 289.0
    n = (n * 34.0 + 1.0) * n
    n -= np.floor(n / 289.0) * 289.0
    n += x
    n -= np.floor(n / 289.0) * 289.0
    n = (n * 34.0 + 1.0) * n
    n -= np.floor(n / 289.0) * 289.0
    return n / 289.0


def terrain_value_noise(x, y):
    import numpy as np
    cell_x, cell_y = np.floor(x), np.floor(y)
    tx, ty = x - cell_x, y - cell_y
    tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
    a = terrain_hash(cell_x, cell_y)
    b = terrain_hash(cell_x + 1, cell_y)
    c = terrain_hash(cell_x, cell_y + 1)
    d = terrain_hash(cell_x + 1, cell_y + 1)
    return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty


@lru_cache(maxsize=2)
def shader_coordinates(size=768, extent=128):
    import numpy as np
    # float32 соответствует арифметике shader model3; карты общие для всех типов.
    axis = (np.arange(size, dtype=np.float32) + np.float32(0.5)) / size * extent / 16.0
    x, y = np.meshgrid(axis, axis)
    warp_x = terrain_value_noise(x * 0.21 + 3.7, y * 0.21 + 9.2)
    warp_y = terrain_value_noise(x * 0.173 + 21.3, y * 0.173 + 6.4)
    x, y = x + (warp_x - 0.5) * 0.65, y + (warp_y - 0.5) * 0.65
    transforms = [
        (0.956304756, -0.292371705, 0.292371705, 0.956304756, 0.91, 0.13, 1.71),
        (0.292371705, -0.956304756, 0.956304756, 0.292371705, 1.07, 2.23, -0.41),
        (-0.731353702, -0.681998360, 0.681998360, -0.731353702, 1.19, -1.37, 3.11),
        (-0.754709580, 0.656059029, -0.656059029, -0.754709580, 0.87, 4.73, 2.61),
    ]
    coordinates = []
    for a, b, c, d, scale, dx, dy in transforms:
        u, v = (a * x + b * y) * scale + dx, (c * x + d * y) * scale + dy
        u, v = 1 - np.abs(np.mod(u * 0.5, 1) * 2 - 1), 1 - np.abs(np.mod(v * 0.5, 1) * 2 - 1)
        # Выборка одного256-пиксельного квадранта с точным полутексельным clamp.
        coordinates.append((np.clip(u * 256, 0.5, 255.5) - 0.5,
                            np.clip(v * 256, 0.5, 255.5) - 0.5))
    wx = terrain_value_noise(x * 0.137 + 9.3, y * 0.137 + 4.7)
    wy = terrain_value_noise(x * 0.119 + 31.1, y * 0.119 + 17.9)
    wx, wy = np.clip((wx - 0.5) * 1.8 + 0.5, 0, 1), np.clip((wy - 0.5) * 1.8 + 0.5, 0, 1)
    wx, wy = wx * wx * (3 - 2 * wx), wy * wy * (3 - 2 * wy)
    return coordinates, wx[..., None], wy[..., None]


def shader_preview(variants, size=768, extent=128):
    import numpy as np
    coordinates, wx, wy = shader_coordinates(size, extent)
    sampled = []
    for image, (u, v) in zip(variants, coordinates):
        pixels = np.asarray(image, dtype=np.float32)
        x0, y0 = np.floor(u).astype(np.int32), np.floor(v).astype(np.int32)
        x1, y1 = np.minimum(x0 + 1, 255), np.minimum(y0 + 1, 255)
        tx, ty = (u - x0.astype(np.float32))[..., None], (v - y0.astype(np.float32))[..., None]
        top = pixels[y0, x0] + (pixels[y0, x1] - pixels[y0, x0]) * tx
        bottom = pixels[y1, x0] + (pixels[y1, x1] - pixels[y1, x0]) * tx
        sampled.append(top + (bottom - top) * ty)
    a, b, c, d = sampled
    upper, lower = a + (b - a) * wx, c + (d - c) * wx
    result = np.clip(np.rint(upper + (lower - upper) * wy), 0, 255).astype(np.uint8)
    return Image.fromarray(result)


def raw_edge_difference(image):
    pixels = image.load()
    width, height = image.size
    horizontal = sum(abs(pixels[0, y][c] - pixels[width - 1, y][c])
                     for y in range(height) for c in range(3)) / (height * 3)
    vertical = sum(abs(pixels[x, 0][c] - pixels[x, height - 1][c])
                   for x in range(width) for c in range(3)) / (width * 3)
    return {'left_right_mean_absolute_rgb': horizontal, 'top_bottom_mean_absolute_rgb': vertical}


def encode_dds(atlas):
    if atlas.size != (4096, 4096) or atlas.mode != 'RGB':
        raise ValueError('DDS-атлас должен быть RGB4096×4096')
    stream = io.BytesIO()
    atlas.save(stream, format='DDS', pixel_format='DXT1')
    data = bytearray(stream.getvalue())
    linear_size = ((atlas.width + 3) // 4) * ((atlas.height + 3) // 4) * 8
    if data[:4] != b'DDS ' or data[84:88] != b'DXT1' or len(data) != 128 + linear_size:
        raise ValueError('Некорректный DXT1-контейнер')
    # Pillow пишет строчный pitch; Clausewitz ожидает полный размер DXT1-поверхности.
    struct.pack_into('<I', data, 20, linear_size)
    flags = struct.unpack_from('<I', data, 8)[0]
    struct.pack_into('<I', data, 8, (flags | 0x80000) & ~0x8)
    with Image.open(io.BytesIO(data)) as source:
        if source.size != atlas.size or source.pixel_format != 'DXT1':
            raise ValueError('DDS не прошёл проверку декодирования')
        decoded = source.convert('RGB')
    return bytes(data), decoded


def publish_dds(root, data):
    directory = (root / 'map/terrain').resolve(strict=True)
    if not directory.is_relative_to(root.resolve()):
        raise ValueError('Каталог атласов выходит за пределы root')
    destinations = [directory / name for name in ('texturesheet.dds', 'texturesheet2.dds')]
    legacy = [directory / name for name in ('texturesheet.tga', 'texturesheet2.tga')]
    for path in destinations + legacy:
        if path.is_symlink() or not path.resolve().is_relative_to(directory) or (path.exists() and not path.is_file()):
            raise ValueError(f'Недопустимая цель атласа: {path}')
    staged = []
    try:
        for destination in destinations:
            with tempfile.NamedTemporaryFile(dir=directory, prefix=destination.name + '.', suffix='.tmp', delete=False) as stream:
                temporary = Path(stream.name)
                staged.append((temporary, destination))
                stream.write(data)
                stream.flush()
                os.fsync(stream.fileno())
            if temporary.read_bytes() != data:
                raise ValueError('Промежуточный DDS записан не полностью')
        for temporary, destination in staged:
            os.replace(temporary, destination)
        if any(destination.read_bytes() != data for destination in destinations):
            raise ValueError('Опубликованные DDS не прошли проверку')
        # Удаляются только два устаревших производных атласа после успешного DDS.
        for path in legacy:
            path.unlink(missing_ok=True)
    finally:
        for temporary, _ in staged:
            temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--atlas-only', action='store_true')
    parser.add_argument('--check-only', action='store_true')
    args = parser.parse_args()
    root = args.root.resolve()
    terrain_file = root / 'map/terrain.txt'
    text = terrain_file.read_bytes().decode('cp1251')
    shader_hash = hashlib.sha256((root / 'gfx/FX/terrain_2_0.fx').read_bytes()).hexdigest()
    names = [n for n in categories(text) if n != 'ocean']
    slots = json.loads((root / 'assets/terrain/tiles.json').read_text(encoding='utf-8'))
    validate_slots(names, slots)
    baseline_path = args.baseline.resolve()
    if baseline_path == (root / 'map/terrain.bmp').resolve():
        raise ValueError('Baseline должен храниться отдельно от результата')
    old_text = baseline_path.with_name('terrain.baseline.txt').read_bytes().decode('cp1251')
    old_slots = {}
    old_entries = re.findall(r'\btext_(\d+)\s*=\s*\{\s*type\s*=\s*(\w+)', old_text)
    if sorted(int(number) for number, _ in old_entries) != list(range(64)):
        raise ValueError('Исходная таблица должна содержать ровно 64 графические ячейки')
    for number, name in old_entries:
        old_slots.setdefault(name, int(number))
    old_atlas = Image.open(baseline_path.with_name('texturesheet.baseline.tga')).convert('RGB')
    if old_atlas.size != (2048, 2048):
        raise ValueError('Размер исходного атласа должен быть 2048×2048')
    # Неиспользуемые ячейки сохраняют исходную графику в увеличенном размере.
    atlas = Image.new('RGB', (4096, 4096))
    for slot in range(64):
        atlas.paste(old_atlas.crop(tile_box(slot)).resize((512, 512), Image.Resampling.LANCZOS),
                    tile_box(slot, 512)[:2])
    tiles = {}
    source_hashes = {}
    hashes = set()
    for name, slot in slots.items():
        path = root / 'assets/terrain/variants' / f'{slot:02}.png'
        source_hashes[name] = hashlib.sha256(path.read_bytes()).hexdigest()
        with Image.open(path) as source:
            variants = split_variants(source)
        for variant, tile in enumerate(variants):
            digest = hashlib.sha256(tile.tobytes()).hexdigest()
            if digest in hashes:
                raise ValueError(f'Повтор текстуры: {name}, вариант {variant}')
            hashes.add(digest)
            x, y = tile_box(slot, 512)[:2]
            atlas.paste(tile, (x + variant % 2 * 256, y + variant // 2 * 256))
        tiles[name] = variants
    labels = {}
    for path in (root / 'localisation').glob('*.csv'):
        for line in path.read_bytes().decode('cp1251').splitlines():
            parts = line.split(';')
            if len(parts) > 1 and parts[0] in slots:
                labels[parts[0]] = parts[1]
    if any(not labels.get(name) for name in names):
        raise ValueError('Отсутствует русская локализация типа местности')
    inverse = {slot: name for name, slot in slots.items()}
    for index in range(64):
        pattern = rf'(\btext_{index}\s*=\s*\{{\s*type\s*=\s*)\w+'
        text, count = re.subn(pattern, lambda m: m[1] + inverse.get(index, 'plains'), text)
        if count != 1:
            raise ValueError(f'Не найден единственный графический слот {index}')
    assignments = {}
    seen = set()
    changed = 0
    result = None
    if not args.atlas_only:
        for path in (root / 'history/provinces').rglob('*.txt'):
            values = re.findall(r'^\s*terrain\s*=\s*(\w+)', path.read_bytes().decode('cp1251'), re.M)
            if len(set(values)) != 1 or not values or values[0] not in slots:
                raise ValueError(f'Неоднозначный тип: {path}')
            ident = int(path.name.split(' ', 1)[0])
            if ident in assignments:
                raise ValueError(f'Повтор номера провинции: {ident}')
            assignments[ident] = values[0]
        if not assignments:
            raise ValueError('Не найдены провинции')
        colours = {}
        for line in (root / 'map/definition.csv').read_bytes().decode('cp1251').splitlines():
            parts = line.split(';')
            if len(parts) >= 4 and parts[0].isdigit():
                colours[tuple(int(float(n)) for n in parts[1:4])] = int(parts[0])
        province = Image.open(root / 'map/provinces.bmp').convert('RGB')
        baseline = Image.open(baseline_path)
        if baseline.mode != 'P' or baseline.size != province.size:
            raise ValueError('Ожидается исходный индексированный BMP размера карты')
        old = baseline.tobytes()
        output = bytearray(old)
        rgb = province.tobytes()
        for offset, original in enumerate(old):
            if original == 254:
                continue
            ident = colours.get(tuple(rgb[offset * 3:offset * 3 + 3]))
            if ident in assignments:
                output[offset] = slots[assignments[ident]]
                seen.add(ident)
        result = Image.frombytes('P', baseline.size, bytes(output))
        result.putpalette(baseline.getpalette())
        changed = sum(a != b for a, b in zip(old, output))
    summary = {'categories': len(names), 'provinces': len(assignments),
               'painted_provinces': len(seen), 'changed_pixels': changed,
               'atlas_size': 4096, 'variants_per_terrain': 4, 'unique_variants': len(hashes)}
    dds, decoded = encode_dds(atlas)
    decoded_hashes = set()
    decoded_variant_hashes = {}
    for name, slot in slots.items():
        x, y = tile_box(slot, 512)[:2]
        for variant in range(4):
            left, top = x + variant % 2 * 256, y + variant // 2 * 256
            image = decoded.crop((left, top, left + 256, top + 256))
            value = hashlib.sha256(image.tobytes()).hexdigest()
            decoded_hashes.add(value)
            decoded_variant_hashes[name, variant] = value
    if len(decoded_hashes) != len(names) * 4:
        raise ValueError('DXT1 потерял уникальность вариантов')
    summary.update({'dds_bytes': len(dds), 'dds_linear_size': 8388608,
                    'dds_sha256': hashlib.sha256(dds).hexdigest(),
                    'decoded_sha256': hashlib.sha256(decoded.tobytes()).hexdigest(),
                    'decoded_unique_variants': len(decoded_hashes), 'compression': 'DXT1 lossy'})
    # Все входные данные и DDS проверены; до этой точки выходные файлы не меняются.
    if args.check_only:
        print(json.dumps(summary))
        return
    font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 13)
    legend = Image.new('RGB', (1280, ((len(names) + 3) // 4) * 345), '#f1e6ce')
    draw = ImageDraw.Draw(legend)
    comparison = Image.new('RGB', (1200, ((len(names) + 3) // 4) * 175), '#f1e6ce')
    compare_draw = ImageDraw.Draw(comparison)
    repeats = root / '.work/terrain/repeats'
    repeats.mkdir(parents=True, exist_ok=True)
    raw_repeats = repeats / 'raw-diagnostic'
    raw_repeats.mkdir(exist_ok=True)
    records = []
    for i, name in enumerate(names):
        variants = tiles[name]
        tile = variants[0]
        x, y = i % 4 * 320, i // 4 * 345
        for variant, image in enumerate(variants):
            legend.paste(image.resize((150, 150)),
                         (x + variant % 2 * 155 + 2, y + variant // 2 * 155))
        draw.text((x + 3, y + 310), labels[name], fill='#281e11', font=font)
        draw.text((x + 3, y + 327), name, fill='#281e11', font=font)
        x, y = i % 4 * 300, i // 4 * 175
        previous = old_slots.get(name)
        if previous is not None:
            comparison.paste(old_atlas.crop(tile_box(previous)).resize((140, 128)), (x, y))
        else:
            compare_draw.text((x + 3, y + 40), 'Нет старой текстуры', fill='#281e11', font=font)
        composite = shader_preview(variants)
        comparison.paste(composite.resize((140, 128)), (x + 150, y))
        composite.save(repeats / f'{slots[name]:02}-{name}-shader-composite.png')
        compare_draw.text((x + 3, y + 130), labels[name], fill='#281e11', font=font)
        compare_draw.text((x + 3, y + 147), f'{name}: {previous} → {slots[name]}', fill='#281e11', font=font)
        variant_records = []
        for variant, image in enumerate(variants):
            repeated = Image.new('RGB', (768, 768))
            for row in range(3):
                for column in range(3):
                    repeated.paste(image, (column * 256, row * 256))
            repeated.save(raw_repeats / f'{slots[name]:02}-{name}-v{variant}.png')
            shader_preview([image] * 4).save(repeats / f'{slots[name]:02}-{name}-v{variant}-mirrored.png')
            variant_records.append({'variant': variant, 'sha256': hashlib.sha256(image.tobytes()).hexdigest(),
                                    'raw_edges': raw_edge_difference(image),
                                    'dxt1_decoded_sha256': decoded_variant_hashes[name, variant]})
        records.append({'terrain': name, 'label': labels[name], 'old_slot': previous,
                        'slot': slots[name], 'variants': variant_records,
                        'source': f'assets/terrain/variants/{slots[name]:02}.png',
                        'source_sha256': source_hashes[name]})
    publish_dds(root, dds)
    legend.save(root / 'assets/terrain/legend.png')
    comparison.save(root / '.work/terrain/comparison.png')
    (root / '.work/terrain/texture-validation.json').write_text(
        json.dumps({'summary': summary, 'tiles': records,
                    'baseline_atlas_sha256': hashlib.sha256(baseline_path.with_name('texturesheet.baseline.tga').read_bytes()).hexdigest(),
                    'baseline_terrain_sha256': hashlib.sha256(old_text.encode('cp1251')).hexdigest(),
                    'preview': {'shader_sha256': shader_hash,
                                'world_extent': 128, 'pixels': 768, 'addressing': 'mirrored per rotated/scaled/warped variant with half-texel clamp',
                                'blend': 'permute289 value noise, domain warp, per-variant rotations/scales, noise smoothstep',
                                'note': 'CPU-превью не включает освещение, colormap, переходы типов и сезон; rawgrid не доказывает бесшовность'}}, ensure_ascii=False, indent=2), encoding='utf-8')
    if result is not None:
        result.save(root / 'map/terrain.bmp')
        terrain_file.write_bytes(text.encode('cp1251'))
    print(json.dumps(summary))


if __name__ == '__main__':
    main()
