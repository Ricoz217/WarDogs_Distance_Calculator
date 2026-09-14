# Third-party notices

This application distributes the following third-party components:

- **ONNX Runtime 1.29.0**, Copyright Microsoft Corporation, MIT License. The license and bundled dependency notices are under `licenses/onnxruntime/` in release packages.
- **PP-OCRv6 recognition model**, from PaddleOCR/RapidOCR, Apache License 2.0. The model license is distributed beside the model as `models/LICENSE.PaddleOCR.txt`.
- **Qt 6.8**, dynamically linked under the GNU Lesser General Public License version 3. The LGPL and GPL license texts are under `licenses/qt/` in release packages.
- **Zstandard 1.5.7**, Copyright Meta Platforms, Inc. and contributors, BSD 3-Clause License. Only the decompression and common sources are statically linked. Its license is under `licenses/zstd/` in release packages.

The application loads only the recognition model. It does not distribute or execute RapidOCR's text detection or direction-classification models.

## Optional terrain data

The separately distributed `.wdt` terrain packages are derived from Terrain3D
elevation datasets published by the unofficial community project **WARDOGS
Artillery Calculator**, maintained by Apollyon. Source and rights information is
provided in `TERRAIN_DATA_NOTICE.md` and must accompany separately distributed
terrain packages.
