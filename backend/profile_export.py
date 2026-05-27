import json
from pathlib import Path
from typing import List, Tuple

from lru_sim import ReuseProfile


def export_path_for(path: Path, export_arg: str, file_count: int, mode: str) -> Path:
    export_path = Path(export_arg)
    if file_count == 1 and export_path.suffix:
        export_path.parent.mkdir(parents=True, exist_ok=True)
        return export_path
    export_path.mkdir(parents=True, exist_ok=True)
    return export_path / f"{path.stem}_{mode}_rdh.json"


def profile_json(profile: ReuseProfile) -> dict:
    return {
        "histogram": {str(rd): count for rd, count in sorted(profile.histogram.items())},
        "cold_misses": len(profile.cold_misses),
        "total_reuses": sum(profile.histogram.values()),
    }


def export_profile(path: Path, mode: str, profile: ReuseProfile,
                   blocks: List[Tuple[str, ReuseProfile]], export_path: Path,
                   granularity: str = "element", cache_line_size: int = 32) -> None:
    payload = {
        "file": str(path),
        "mode": mode,
        "granularity": granularity,
        "cache_line_size": cache_line_size if granularity == "cache-line" else None,
        "program": profile_json(profile),
        "blocks": [
            {"name": name, "profile": profile_json(block_profile)}
            for name, block_profile in blocks
        ],
    }
    export_path.write_text(json.dumps(payload, indent=2, sort_keys=True), encoding="utf-8")
    print(f"  JSON 저장 → {export_path}")
