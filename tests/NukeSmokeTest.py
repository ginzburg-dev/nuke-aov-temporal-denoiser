import nuke


node = nuke.createNode("GinzburgTemporalDenoiser", inpanel=False)
if node.Class() != "GinzburgTemporalDenoiser":
    raise RuntimeError(f"unexpected node class: {node.Class()}")

required_knobs = {
    "albedo",
    "beauty",
    "depth",
    "motion",
    "normal",
    "position",
    "spatial_radius",
    "temporal_radius",
}
missing_knobs = required_knobs.difference(node.knobs())
if missing_knobs:
    raise RuntimeError(f"missing knobs: {sorted(missing_knobs)}")

print(f"Loaded {node.Class()} with Nuke {nuke.NUKE_VERSION_STRING}")
