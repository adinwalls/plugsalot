#!/usr/bin/env python3
"""Copy the upstream DSP files the plugin compiles into plugin/vendor/<source>/.

Files are copied byte-for-byte from the pinned checkout (.cache/upstream), keeping
their upstream relative paths, so every vendored file can be diffed against its
source commit. Framework glue the files expect (DPF, the VST2 SDK) is replaced by
small headers in plugin/shims/, never by editing upstream code.

    python3 tools/fetch_sources.py
    python3 tools/vendor_sources.py
"""

from __future__ import annotations

import json
import shutil
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CACHE = REPO / ".cache" / "upstream"
VENDOR = REPO / "plugin" / "vendor"

# source key -> glob patterns (relative to the upstream root)
FILES = {
    "dusk-audio": [
        "LICENSE",
        "plugins/multi-comp/core/*.hpp",
        "plugins/multi-comp/core/*.cpp",
        "plugins/TapeMachine/core/*.hpp",
        "plugins/TapeMachine/core/*.cpp",
        "plugins/shared-daf/dsp/*.hpp",
        "plugins/shared-daf/dsp/*.h",
        "plugins/shared-daf/dsp/*.cpp",
        "plugins/shared-daf/dsp/*.inc",
        "plugins/4k-eq/daf-plugin/FourKEQBandFrequency.hpp",
        "plugins/multi-comp/HardwareEmulation/*.h",
        "plugins/shared/AnalogEmulation/*.h",
    ],
    "dusk-audio-echo": [
        "LICENSE",
        "plugins/tape-echo/core/TapeEchoDSP.hpp",
        "plugins/tape-echo/core/TapeEchoDSP.cpp",
        "plugins/tape-echo/daf-plugin/TapeEchoParams.hpp",
    ],
    "dragonfly-reverb": [
        "LICENSE",
        "plugins/dragonfly-plate-reverb/DSP.cpp",
        "plugins/dragonfly-plate-reverb/DSP.hpp",
        "plugins/dragonfly-plate-reverb/DistrhoPluginInfo.h",
        "common/AbstractDSP.hpp",
        "common/Param.hpp",
        "common/fv3_config.h",
        "common/freeverb/*",
    ],
    "x42-dpl": [
        "COPYING",
        "src/peaklim.cc",
        "src/peaklim.h",
    ],
    "airwindows": [
        "LICENSE",
        "plugins/LinuxVST/src/DeBess/*",
    ],
    "nam-core": [
        "LICENSE",
        "NAM/*.h",
        "NAM/*.cpp",
        "NAM/wavenet/*",
        "Dependencies/nlohmann/*",
        "example_models/wavenet_a1_standard.nam",
        "example_models/A2.nam",
    ],
    "nam-plugin": [
        "LICENSE",
        "NeuralAmpModeler/ToneStack.h",
        "NeuralAmpModeler/ToneStack.cpp",
    ],
    "audiodsptools": [
        "LICENSE",
        "dsp/dsp.h",
        "dsp/dsp.cpp",
        "dsp/wav.h",
        "dsp/wav.cpp",
        "dsp/NoiseGate.h",
        "dsp/NoiseGate.cpp",
        "dsp/RecursiveLinearFilter.h",
        "dsp/RecursiveLinearFilter.cpp",
        "dsp/ImpulseResponse.h",
        "dsp/ImpulseResponse.cpp",
        "dsp/Resample.h",
    ],
    "eigen": [
        "COPYING.*",
        "Eigen/*",
        "Eigen/src/**/*",
        "unsupported/Eigen/FFT",
        "unsupported/Eigen/src/FFT/*",
    ],
}


def main() -> int:
    sources = json.loads((REPO / "third_party" / "sources.json").read_text())["sources"]
    manifest = {}
    for key, patterns in FILES.items():
        root = CACHE / key
        if not root.exists():
            print(f"{key}: not fetched; run tools/fetch_sources.py", file=sys.stderr)
            return 1
        dest_root = VENDOR / key
        if dest_root.exists():
            shutil.rmtree(dest_root)
        copied = []
        for pat in patterns:
            matches = sorted(p for p in root.glob(pat) if p.is_file())
            if not matches:
                print(f"{key}: no files match {pat}", file=sys.stderr)
                return 1
            for src in matches:
                rel = src.relative_to(root)
                dst = dest_root / rel
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(src, dst)
                copied.append(str(rel))
        manifest[key] = {"commit": sources[key]["commit"], "license": sources[key]["license"], "files": sorted(set(copied))}
        print(f"  {key}: {len(set(copied))} files")
    (VENDOR / "VENDORED.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
