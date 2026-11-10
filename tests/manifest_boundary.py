"""Validate HelloGUI's builder graph without conflating the app and SDK layers."""
from pathlib import Path


def hello_manifest_violations(manifest, manifest_dir, iigui_root):
    errors = []
    if not isinstance(manifest, dict) or not isinstance(manifest.get("sdks"), list):
        return ["HelloGUI manifest must contain an SDK list"]
    sdks = {}
    for item in manifest["sdks"]:
        if not isinstance(item, dict) or not isinstance(item.get("name"), str) or not item["name"]:
            errors.append("HelloGUI manifest contains an invalid SDK entry")
            continue
        name = item["name"].casefold()
        if name in sdks:
            errors.append("HelloGUI manifest contains a duplicate SDK name: " + item["name"])
        else:
            sdks[name] = item
        if name.startswith(("qt", "qml", "lvrs")):
            errors.append("HelloGUI manifest selects a forbidden SDK: " + item["name"])
        depends = item.get("depends", [])
        if not isinstance(depends, list) or depends:
            errors.append("SDK projects cannot depend on other SDK projects: " + item["name"])
    host = sdks.get("iigui")
    if not host:
        errors.append("HelloGUI manifest must contain iiGUI")
    elif not isinstance(host.get("source"), str) or (Path(manifest_dir) / host["source"]).resolve() != Path(iigui_root).resolve():
        errors.append("HelloGUI iiGUI SDK must use the current iiGUI source")
    app = manifest.get("app")
    dependencies = app.get("depends") if isinstance(app, dict) else None
    if not isinstance(dependencies, list) or any(not isinstance(name, str) or not name for name in dependencies):
        errors.append("HelloGUI app must have a valid SDK dependency list")
    else:
        normalized = [name.casefold() for name in dependencies]
        if "iigui" not in normalized:
            errors.append("HelloGUI app must select iiGUI")
        known = set(sdks) | {"iisdkcore"}  # iiSDKBuilder can select Core automatically.
        if any(name not in known for name in normalized):
            errors.append("HelloGUI app selects an unknown SDK")
    return errors
