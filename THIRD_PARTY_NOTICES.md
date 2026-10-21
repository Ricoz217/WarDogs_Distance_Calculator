# Third-party notices

This application distributes the following third-party components:

- **ONNX Runtime 1.29.0**, Copyright Microsoft Corporation, MIT License. The license and bundled dependency notices are under `licenses/onnxruntime/` in release packages.
- **PP-OCRv6 recognition model**, from PaddleOCR/RapidOCR, Apache License 2.0. The model license is distributed beside the model as `models/LICENSE.PaddleOCR.txt`.
- **Qt 6.8**, dynamically linked under the GNU Lesser General Public License version 3. The LGPL and GPL license texts are under `licenses/qt/` in release packages.
- **Zstandard 1.5.7**, Copyright Meta Platforms, Inc. and contributors, BSD 3-Clause License. Only the decompression and common sources are statically linked. Its license is under `licenses/zstd/` in release packages.
- **WARDOGS Artillery Calculator SPH-2 fitted trajectory model**, Copyright (c) 2026 Apollyon, MIT License. The C++ port is derived from `js/workers/terrain-height-solver.js` at upstream commit `f43183c0747afa3dc33ec9af93a153a53766bf7f`. Source: <https://github.com/apollyon-sys/wardogs-calculator>. Its license is under `licenses/wardogs-calculator/` in release packages.

The application loads only the recognition model. It does not distribute or execute RapidOCR's text detection or direction-classification models.

## Optional terrain data

The optional `.wdt` terrain packages are independently parsed and
converted by this project from WARDOGS terrain data for research and
calibration. The underlying game assets and terrain data are the property of
BULKHEAD; this project does not claim ownership of them. The repository's MIT
License does not cover the `.wdt` files, their source terrain data, or other
WARDOGS/BULKHEAD assets. Processing details and rights information are provided
in `TERRAIN_DATA_NOTICE.md`, which must remain with every copy of the terrain
data.
