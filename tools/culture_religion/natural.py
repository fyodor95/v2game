"""Естественное срабатывание новой первой корки через существующий нативный стенд."""
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from tools.culture_religion.engine import install_adapters
from tools.chronology_import.natural_spawn import main
if __name__=='__main__':
    install_adapters()
    raise SystemExit(main())
