# Third-party notices

This application distributes the following third-party components:

- **ONNX Runtime 1.29.0**, Copyright Microsoft Corporation, MIT License. The license and bundled dependency notices are under `licenses/onnxruntime/` in release packages.
- **PP-OCRv6 recognition model**, from PaddleOCR/RapidOCR, Apache License 2.0. The model license is distributed beside the model as `models/LICENSE.PaddleOCR.txt`.
- **Qt 6.8**, dynamically linked under the GNU Lesser General Public License version 3. The LGPL and GPL license texts are under `licenses/qt/` in release packages.

The application loads only the recognition model. It does not distribute or execute RapidOCR's text detection or direction-classification models.
