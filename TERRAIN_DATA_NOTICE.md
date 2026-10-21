# Terrain data source notice

The optional `bakurani.wdt`, `ozeti.wdt`, and `zestafona.wdt` terrain packages
are derived from Terrain3D elevation data extracted and processed by Apollyon
for **WARDOGS Artillery Calculator**, an unofficial community project:

- Project: https://wardogs-artillery.com/
- Source repository: https://github.com/apollyon-sys/wardogs-calculator

The current snapshot was supplied directly by Apollyon as a one-time exception.
It includes the Season 2 Bakurani update. No future updates or compatibility
support are implied.

This project records the SHA-256 digest of each upstream manifest, verifies each
source chunk against the digest listed in that manifest, retains the 2 metre
horizontal grid, quantizes height to 0.1 metre units, and re-encodes each chunk
with Zstandard. The resulting `.wdt` files contain elevation samples and package
metadata; they do not contain map imagery.

The underlying WARDOGS game assets and height data belong to BULKHEAD. Apollyon
cannot and does not grant permission to use or redistribute them on BULKHEAD's
behalf. Any use, conversion, hosting, or distribution is undertaken by this
project independently and remains its responsibility. The MIT licenses of the
two projects do not relicense the terrain data or any WARDOGS intellectual
property.

The supplied archive and converted terrain datasets are not published through
this project's GitHub repository or GitHub Releases and are not offered as
unrestricted public downloads. This project does not sell the terrain data. If
network delivery is used, it must use privately controlled infrastructure with
access controls and measures against bulk downloading.

War Dogs Distance Calculator is an unofficial fan-made utility and is not
affiliated with, endorsed by, or officially associated with BULKHEAD or the
WARDOGS development team.

This notice must accompany every separately distributed `.wdt` package. This
project does not grant permission to redistribute the underlying terrain data.
