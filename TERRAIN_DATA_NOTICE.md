# Terrain data notice

The optional `bakurani.wdt`, `ozeti.wdt`, and `zestafona.wdt` files contain
quantized elevation samples independently parsed and converted by War Dogs
Distance Calculator from WARDOGS terrain data for research and calibration.
The converter retains a 2 metre horizontal grid, quantizes height to 0.1 metre
units, and stores the samples in a chunked `.wdt` format with Zstandard
compression. The files contain elevation samples and package metadata; they do
not contain map imagery.

The underlying game assets and terrain data are the property of BULKHEAD. War
Dogs Distance Calculator does not claim ownership of them.

The MIT License in the War Dogs Distance Calculator repository applies to the
project's source code and other original material covered by that license. It
does **not** apply to these `.wdt` files, their source terrain data, or any other
WARDOGS or BULKHEAD asset, and it grants no rights in those materials. War Dogs
Distance Calculator does not authorize their publication, redistribution,
sublicensing, or sale.

These terrain datasets are not published through the project's source-code
repository or GitHub Releases and are not offered as unrestricted public
downloads. This project does not sell the terrain data.

## Software attribution

War Dogs Distance Calculator uses algorithms and source code from **WARDOGS
Artillery Calculator**, an unofficial community project by Apollyon:

- Project: https://wardogs-artillery.com/

This attribution applies to the software work. It does not identify WARDOGS
Artillery Calculator or its author as the source, publisher, or rights holder of
the `.wdt` terrain data in this package.

War Dogs Distance Calculator is an unofficial fan-made utility and is not
affiliated with, endorsed by, or officially associated with BULKHEAD or the
WARDOGS development team. Use of the data remains the user's responsibility.

This notice must remain with every copy of the `.wdt` files. War Dogs Distance
Calculator does not grant permission to redistribute the underlying terrain
data.
