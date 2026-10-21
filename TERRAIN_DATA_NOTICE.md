# Terrain data notice

The optional `bakurani.wdt`, `ozeti.wdt`, and `zestafona.wdt` files contain
quantized elevation samples derived from WARDOGS game data. The underlying game
assets and terrain data are the property of BULKHEAD.

The source Terrain3D snapshot was extracted and processed by Apollyon for
**WARDOGS Artillery Calculator**, an unofficial community project:

- Project: https://wardogs-artillery.com/

The snapshot was supplied directly by Apollyon as a one-time exception and
includes the Season 2 Bakurani update. War Dogs Distance Calculator retained the
2 metre horizontal grid, quantized height to 0.1 metre units, and re-encoded the
samples in its own chunked `.wdt` format with Zstandard compression. The `.wdt`
files contain elevation samples and package metadata; they do not contain map
imagery.

The MIT License in the War Dogs Distance Calculator repository applies to the
project's source code and other original material covered by that license. It
does **not** apply to these `.wdt` files, their source terrain data, or any other
WARDOGS or BULKHEAD asset, and it grants no rights in those materials. War Dogs
Distance Calculator does not authorize their publication, redistribution,
sublicensing, or sale.

These terrain datasets are not published through the project's source-code
repository or GitHub Releases and are not offered as unrestricted public
downloads. This project does not sell the terrain data.

War Dogs Distance Calculator is an unofficial fan-made utility and is not
affiliated with, endorsed by, or officially associated with BULKHEAD or the
WARDOGS development team. Use of the data remains the user's responsibility.

This notice must remain with every copy of the `.wdt` files. War Dogs Distance
Calculator does not grant permission to redistribute the underlying terrain
data.
