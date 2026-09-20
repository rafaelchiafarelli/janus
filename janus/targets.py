"""Registry of Janus's generation targets. `generate()` (janus/cli.py)
writes every entry here on every run, each into its own subtree of
`target_dir`/`scaffold_src` — it does not accept a `--target` selector
itself. Picking which target a given project actually wants installed is
`scripts/janus.sh --target <name>`'s job, not this module's or
`generate()`'s — see the desktop_target initiative's
multi_target_pipeline epic for why.

A `Target` with an empty `vendor_from` is a reserved name with no
generator yet (`android`): `generate()` skips it entirely rather than
writing an empty directory for it.
"""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

_REPO_ROOT = Path(__file__).resolve().parent.parent


@dataclass(frozen=True)
class Target:
    name: str
    # Source roots to vendor the fixed runtime library from, in scaffold
    # mode, in order — a later root's file of the same relative path
    # overwrites an earlier root's copy at the destination. `desktop`
    # uses this to reuse `embedded_c`'s genuinely platform-agnostic core
    # (already proven on x86 via host_mock) unchanged, adding only its
    # own SDL2 driver + CMakeLists.txt on top, rather than duplicating
    # the core into a second tree (desktop_sdl2_runtime epic decision 1).
    # An empty tuple means "reserved, not implemented yet" — generate()
    # skips writing anything for this target at all.
    vendor_from: tuple[Path, ...] = ()
    vendored_subdirs: tuple[str, ...] = ()
    vendored_root_files: tuple[str, ...] = ()


TARGETS: dict[str, Target] = {
    "embedded_c": Target(
        name="embedded_c",
        vendor_from=(_REPO_ROOT / "runtime" / "embedded_c",),
        vendored_subdirs=("include", "src", "tests", "host_mock"),
        vendored_root_files=("CMakeLists.txt", "janus_img.ld"),
    ),
    "desktop": Target(
        name="desktop",
        vendor_from=(
            _REPO_ROOT / "runtime" / "embedded_c",
            _REPO_ROOT / "runtime" / "desktop",
        ),
        # "driver" only exists under runtime/desktop (the real SDL2
        # driver, deliberately its own CMake library — see that
        # CMakeLists.txt for why it can't be compiled into janus_runtime
        # itself); _vendor_runtime skips a subdir a given root lacks, so
        # listing it here is harmless for the embedded_c root.
        vendored_subdirs=("include", "src", "tests", "host_mock", "driver"),
        vendored_root_files=("CMakeLists.txt",),
    ),
    # Reserved — a future, separate initiative populates this.
    "android": Target(name="android"),
}


def implemented_targets() -> list[Target]:
    """Targets `generate()` should actually produce output for — every
    registry entry with at least one real source root to vendor from."""
    return [t for t in TARGETS.values() if t.vendor_from]
