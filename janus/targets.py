"""Registry of Janus's generation targets. `generate()` (janus/cli.py)
writes every entry here on every run, each into its own subtree of
`target_dir`/`scaffold_src` — it does not accept a `--target` selector
itself. Picking which target a given project actually wants installed is
`scripts/janus.sh --target <name>`'s job, not this module's or
`generate()`'s — see the desktop_target initiative's
multi_target_pipeline epic for why.

A `Target` with `runtime_dir=None` is a reserved name with no generator
yet (`desktop`, `android`): `generate()` skips it entirely rather than
writing an empty directory for it.
"""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

_REPO_ROOT = Path(__file__).resolve().parent.parent


@dataclass(frozen=True)
class Target:
    name: str
    # Directory to vendor the fixed runtime library from, in scaffold
    # mode. None means "reserved, not implemented yet" — generate()
    # skips writing anything for this target at all.
    runtime_dir: Path | None = None
    vendored_subdirs: tuple[str, ...] = ()
    vendored_root_files: tuple[str, ...] = ()


TARGETS: dict[str, Target] = {
    "embedded_c": Target(
        name="embedded_c",
        runtime_dir=_REPO_ROOT / "runtime" / "embedded_c",
        vendored_subdirs=("include", "src", "tests", "host_mock"),
        vendored_root_files=("CMakeLists.txt", "janus_img.ld"),
    ),
    # Reserved — desktop_sdl2_runtime/desktop_scaffold populate this.
    "desktop": Target(name="desktop"),
    # Reserved — a future, separate initiative populates this.
    "android": Target(name="android"),
}


def implemented_targets() -> list[Target]:
    """Targets `generate()` should actually produce output for — every
    registry entry with a real runtime to vendor."""
    return [t for t in TARGETS.values() if t.runtime_dir is not None]
