"""Создать или проверить относительный симлинк корневых инструкций."""
import argparse
import os
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
link = root / 'AGENTS.md'
target = Path('AGENTS') / 'AGENTS.md'
if args.check:
    assert link.is_symlink() and not Path(os.readlink(link)).is_absolute()
    assert link.resolve() == (root / target).resolve()
else:
    if link.is_symlink():
        if link.resolve() != (root / target).resolve():
            raise RuntimeError('Существующая ссылка ведёт к другому файлу')
        link.unlink()
    elif link.exists():
        raise RuntimeError('Существующий обычный файл не заменяется')
    os.symlink(str(target), str(link))
print('AGENTS.md → AGENTS/AGENTS.md')
