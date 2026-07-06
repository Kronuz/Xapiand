# Geospatial debugging tools

## `google_map_plotter.py` — plot trixels and geospatial ranges on a map

A small utility for *seeing* what the geospatial / HTM (Hierarchical Triangular
Mesh) subsystem produces. It renders a standalone Google Maps HTML page with
points, paths, and polygons, so you can eyeball trixels, convex hulls, and range
coverings instead of reading raw term lists.

It is a vendored copy of [**gmplot**](https://github.com/vgm64/gmplot) by Michael
Woods (MIT). Nothing here is Xapiand-specific — the value is the workflow: take
the trixels a query or an indexed shape expands to, feed their corner
coordinates in as polygons, and open the result in a browser. You can also just
`pip install gmplot` and use the upstream package; this copy is kept so the
trixel-plotting recipe below doesn't get lost.

### Usage

```python
from google_map_plotter import GoogleMapPlotter

# Center the map and pick a zoom level.
gmap = GoogleMapPlotter(37.428, -122.145, 11)

# Each trixel is a triangle: draw it as a polygon from its three corner
# (lat, lng) pairs. Loop over the trixels a shape or query expands to.
for trixel in trixels:
    lats = [c.lat for c in trixel.corners]
    lngs = [c.lng for c in trixel.corners]
    gmap.polygon(lats, lngs, color="#3366cc")

# Scatter the original points / draw the source shape on top for comparison.
gmap.scatter(point_lats, point_lngs, color="#cc0000", marker=True)

# Write the page and open it in a browser.
gmap.draw("trixels.html")
```

`GoogleMapPlotter` also offers `plot` (polylines), `heatmap`, `circle`, and
`grid`. A live Google Maps page needs a Maps JavaScript API key; for quick local
inspection the generated HTML is usually enough.

### Requirements

Python 3 with `requests` (used only by the optional geocoding helper).
