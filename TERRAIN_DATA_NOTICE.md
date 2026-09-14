# Terrain data source notice

The optional `bakurani.wdt`, `ozeti.wdt`, and `zestafona.wdt` terrain packages
are derived from Terrain3D elevation datasets published by **WARDOGS Artillery
Calculator**, an unofficial community project maintained by Apollyon:

- Project: https://wardogs-artillery.com/
- Source repository: https://github.com/apollyon-sys/wardogs-calculator
- Published Terrain3D data: https://assets.wardogs-artillery.com/releases/assets-v1/data/terrain/

This project records the SHA-256 digest of each upstream manifest, verifies each
source chunk against the digest listed in that manifest, retains the 2 metre
horizontal grid, quantizes height to 0.1 metre units, and re-encodes each chunk
with Zstandard. The resulting `.wdt` files contain elevation samples and package
metadata; they do not contain map imagery.

According to the upstream project's
[legal notice](https://github.com/apollyon-sys/wardogs-calculator/blob/main/docs/legal.md),
its MIT License applies to original source code and does not cover WARDOGS game
assets or other third-party material. Those materials remain the property of
their respective rights holders. This project's MIT License likewise does not
relicense the source terrain data or any WARDOGS intellectual property.

War Dogs Distance Calculator is an unofficial fan-made utility and is not
affiliated with, endorsed by, or officially associated with BULKHEAD or the
WARDOGS development team.

When distributing any `.wdt` package separately from the application, include
this notice with the package.
