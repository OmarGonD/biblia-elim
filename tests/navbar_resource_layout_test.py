"""Check the layout contract in the UI resource actually shipped in the app.

A vertically expanding lookup entry propagates expansion into the navbar.
With centre alignment this leaves large empty strips above and below it.
"""

import subprocess
import sys
import xml.etree.ElementTree as ET


resource = subprocess.check_output([
    sys.argv[1], "extract", sys.argv[2],
    "/org/xiphos/ui/navbar_versekey.gtkbuilder",
])
tree = ET.fromstring(resource)


def properties(widget_id):
    widget = tree.find(f".//object[@id='{widget_id}']")
    assert widget is not None, f"Missing widget: {widget_id}"
    return {p.attrib["name"]: p.text.strip().lower()
            for p in widget.findall("property")}


bar = properties("navbar")
entry = properties("entry_lookup")
assert bar.get("vexpand") == "false", "Navbar must not consume reading height"
assert bar.get("valign") == "start", "Navbar must remain next to passage tabs"
assert entry.get("vexpand", "false") == "false", "Lookup must not expand vertically"
assert bar.get("hexpand") == "true", "Navbar background must span the row"
assert bar.get("halign") == "fill", "Navbar background must not stop after the controls"
assert entry.get("hexpand") == "false", "Lookup must not stretch across the window"
assert entry.get("width-chars") == entry.get("max-width-chars") == "20"
for widget_id in ("label_chapter", "label_verse"):
    label = properties(widget_id)
    assert label.get("hexpand") == "false", f"{widget_id} must not stretch"
    assert label.get("width-chars") == "3", f"{widget_id} must fit Psalm 119:176"
# Both labels must fill the button height. A vertical box packs the verse
# label at the top, whereas a horizontal box gives it the full cross axis.
for widget_id in ("hbox6", "hbox7"):
    assert properties(widget_id).get("orientation", "horizontal") == "horizontal", \
        f"{widget_id} must vertically centre its number like the chapter selector"
print("navbar_resource_layout_failures=0")
