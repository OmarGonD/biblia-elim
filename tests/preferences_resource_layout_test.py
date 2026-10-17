"""Guard the preferences layout shipped in the executable.

GTK boxes default to horizontal: the missing root orientation placed the Close
button beside the form and stretched it over the entire window height.
"""
import subprocess
import sys
import xml.etree.ElementTree as ET

resource = subprocess.check_output([
    sys.argv[1], "extract", sys.argv[2],
    "/org/bibliaelim/ui/prefs.gtkbuilder",
])
tree = ET.fromstring(resource)


def widget(widget_id):
    result = tree.find(f".//object[@id='{widget_id}']")
    assert result is not None, f"Missing widget: {widget_id}"
    return result


def properties(widget_id):
    return {p.attrib["name"]: p.text.strip().lower()
            for p in widget(widget_id).findall("property")}


root = widget("dialog-vbox1")
assert properties("dialog-vbox1").get("orientation") == "vertical", \
    "Close action must be below the form, not beside it"
children = [child.find("object").attrib["id"] for child in root.findall("child")]
assert children == ["hbox1", "dialog-action_area1"], \
    "The form must precede the footer"
assert properties("hbox1").get("vexpand") == "true"
assert properties("hbox1").get("hexpand") == "true"
assert properties("dialog-action_area1").get("vexpand") == "false"
assert properties("closebutton1").get("vexpand") == "false"
assert properties("closebutton1").get("valign") == "center", \
    "Close action must retain its natural height"
assert int(properties("scrolledwindow")["width_request"]) >= 220, \
    "Sidebar must fit translated headings and indentation"
action = tree.find(".//action-widget[@response='-7']")
assert action is not None and action.text == "closebutton1", \
    "Close action must retain its dialog response"
print("preferences_resource_layout_test: PASS")
