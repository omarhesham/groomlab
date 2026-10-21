# SPDX-License-Identifier: Apache-2.0
"""Render the groom in RenderMan (RenderMan Pro Server's prman) from any Maya (2022 too: RenderMan for
Maya is not needed). RenderMan is free for non-commercial use.

    import groomlab
    job = groomlab.render_prman()                       # this frame, the studio's (or the view's) camera
    job = groomlab.render_prman(start=1, end=120)       # a turntable or a shot: one image a frame
    job.poll()                                          # (frames done, frames, finished, percent)
    job.wait(); job.images()                            # the PNGs, in groomlab/<scene>/renders/prman_<date>

What goes to prman, as one RIB beside the images (prman_scene.scene_text), the scene read as the
V-Ray Standalone render reads it (render_gather):
  - every visible groom through GroomLab's RenderMan procedural (groomlab_renderman, installed with
    the module): the groom written to a .groom beside the scene (with its scalp's animation cached
    when the scalp moves over the frames or motion blur is on; a groom rendering from its own groom
    file takes that file), its render density and overrides, the hair network fitted to the Arnold
    look (PxrHairColor + PxrMarschnerHair reading the procedural's primvars), deformation blur
    keys with motion_blur;
  - the visible meshes (or with meshes="selected" the selection and the grooms' scalps and
    colliders) with a PxrSurface from their shader's color, specular and roughness, subdivided
    when Arnold's subdivision or Maya's smooth preview is on;
  - the camera (camera=, else the studio's camera, else the viewport's, else a renderable one) and
    the field of view the render resolution gives it (film fit);
  - Arnold and Maya area lights (PxrRectLight, Arnold's spread as emissionFocus), sky domes
    (PxrDomeLight, their color: not a texture), directional lights (PxrDistantLight), light links
    and meshes that cast no shadow. Without any of them, a grey dome and a key from above the camera;
  - the resolution from the render settings (or width/height), the quality's samples.
prman renders in the background (Maya stays free): GroomLab > Render > RenderMan (prman) shows its
progress, can stop it, and loads the image into Maya's Render View.

prman is found from groomlab.set_prman_path's setting, the GROOMLAB_PRMAN environment variable,
RMANTREE, the default install folder (Program Files/Pixar/RenderManProServer-*) or the PATH
(prman_scene.find_prman).
"""

import os

from maya import cmds

from groomlab import prman_scene, render_common, render_gather

PATH_VAR = "groomLabPrman"  # optionVar: prman.exe (or its folder, or RMANTREE)


# ---------------------------------------------------------------------------
# prman

def prman_path():
    """The prman GroomLab renders with, or None when none is found."""
    preferred = cmds.optionVar(query=PATH_VAR) if cmds.optionVar(exists=PATH_VAR) else None
    return prman_scene.find_prman(preferred or None)


def set_prman_path(path):
    """Remember prman (the executable, its folder or RenderMan Pro Server's) for RenderMan renders;
    None or "" forgets it."""
    if not path:
        if cmds.optionVar(exists=PATH_VAR):
            cmds.optionVar(remove=PATH_VAR)
        return None
    found = prman_scene._exe_in(path)
    if not found:
        raise ValueError("no %s at %s" % (prman_scene.EXE, path))
    cmds.optionVar(stringValue=(PATH_VAR, found))
    return found


def procedural_folder():
    """The folder holding GroomLab's RenderMan procedural, or None (renderman.dso_path)."""
    from groomlab import renderman
    dso = renderman.dso_path()
    return os.path.dirname(dso) if dso else None


# ---------------------------------------------------------------------------
# The grooms

def _scalp_moves(groom, frames):
    """Whether the groom's scalp moves or deforms over `frames` (its world matrix or its points)."""
    from groomlab.base import _scalp_shape
    scalp = _scalp_shape(groom)
    if not scalp or len(frames) < 2:
        return False
    if render_gather.deforms(scalp):
        return True
    now = cmds.currentTime(query=True)
    try:
        seen = []
        for frame in sorted(set([frames[0], frames[len(frames) // 2], frames[-1]])):
            cmds.currentTime(frame, update=True)
            seen.append(render_gather.world_matrix(scalp))
    finally:
        cmds.currentTime(now, update=True)
    return any(max(abs(a - b) for a, b in zip(m, seen[0])) > 1e-6 for m in seen[1:])


def groom_entries(grooms, folder, frames, density=1.0, motion_blur=False):
    """Each groom as prman_scene.groom_text takes it, its .groom written into `folder` (the scalp's
    animation cached when it moves over the frames or the shutter); a groom that renders from its
    own groom file (Render > Render from Groom File) takes that file."""
    from groomlab.files import export_groom, groom_file
    from groomlab.shading import hair_look
    entries = []
    names = render_gather.unique_names(grooms)
    for groom in grooms:
        name = "groom_" + names[groom]
        path = groom_file(groom) if cmds.getAttr(groom + ".renderMode") == 1 else None
        if not path or not os.path.isfile(path):
            path = os.path.join(folder, name + ".groom").replace("\\", "/")
            moves = (len(frames) > 1 or motion_blur) and _scalp_moves(groom, frames + ([frames[-1] + 1] if motion_blur
                                                                                         else []))
            end = frames[-1] + (1 if motion_blur else 0)
            export_groom(groom, path, scalp_cache=moves, start=frames[0] if moves else None, end=end if moves else None)
        entries.append({"shape": groom, "name": name, "file": path,
                        "matrix": render_gather.world_matrix(render_gather.parent_of(groom)),
                        "density": float(density) * float(render_gather.get(groom, "renderDensity", 1.0)),
                        "width_scale": float(render_gather.get(groom, "renderWidthScale", 1.0)),
                        "length_scale": float(render_gather.get(groom, "renderLengthScale", 1.0)),
                        "tilt": hair_look(groom)["tilt"]})
    return entries


# ---------------------------------------------------------------------------
# The scene

def write_prman_scene(path, grooms=None, camera=None, start=None, end=None, step=1, width=None, height=None,
                      quality="preview", meshes="visible", density=1.0, motion_blur=False, image=None):
    """Write a RenderMan scene of the render (the module's docstring says what it holds) to `path`
    (a .rib), the grooms' .groom files and the meshes' archives beside it, rendering to `image`
    (default <path's stem>.png). Returns {"scene", "image", "frames", "grooms" (shape -> its
    .groom), "camera", "lights" (light names), "meshes", "skipped_lights", "width", "height"}.
    Raises RuntimeError when the procedural is not installed."""
    procedurals = procedural_folder()
    if not procedurals:
        raise RuntimeError(prman_scene.PROCEDURAL_MISSING)
    now = cmds.currentTime(query=True)
    start = now if start is None else start
    frames = render_common.frame_list(start, end if end is not None else start, step)
    if not frames:
        raise ValueError("no frames from %s to %s" % (start, end))
    width, height = (int(width or render_gather.resolution()[0]), int(height or render_gather.resolution()[1]))
    grooms = render_gather.render_grooms(grooms)
    camera = render_gather.render_camera(camera)
    shapes = render_gather.render_meshes(grooms, meshes)
    folder = os.path.dirname(os.path.abspath(path))
    if not os.path.isdir(folder):
        os.makedirs(folder)
    image = image or os.path.splitext(path)[0] + ".png"
    entries = groom_entries(grooms, folder, frames, density, motion_blur)
    cam, lights, mesh_data, skipped = render_gather.gather(frames, grooms, camera, shapes,
                                                           float(width) / float(max(height, 1)))
    for mesh in mesh_data:
        prman_scene.write_mesh_archives(mesh, folder, frames)
    text = prman_scene.scene_text(width, height, frames, image, cam, lights=lights, meshes=mesh_data, grooms=entries,
                                  quality=quality, procedurals=procedurals, folder=folder, motion_blur=motion_blur,
                                  comment="%s, camera %s" % (cmds.file(query=True, sceneName=True) or "untitled",
                                                             camera.split("|")[-1]))
    with open(path, "w") as f:
        f.write(text)
    return {"scene": path, "image": image, "frames": frames, "grooms": dict((e["shape"], e["file"]) for e in entries),
            "camera": camera, "lights": [l["name"] for l in lights], "meshes": [m["shape"] for m in mesh_data],
            "skipped_lights": skipped, "width": width, "height": height}


def render_prman(grooms=None, camera=None, start=None, end=None, step=1, width=None, height=None, quality="preview",
                 meshes="visible", folder=None, name="prman", density=1.0, motion_blur=False, threads=0):
    """Render the groom(s) in RenderMan (prman) in the background. Frames: `start` (default the
    current frame) to `end` by `step`; `quality` "draft", "preview" or "final"; `motion_blur`
    deformation blur over half a frame; `folder` the output folder (default <scene's groomlab
    folder>/renders/<name>_<date>). Returns a prman_scene.PrmanJob (poll, wait, cancel, images) with
    .info (write_prman_scene's result). Raises RuntimeError when prman or the procedural is not found."""
    prman = prman_path()
    if not prman:
        raise RuntimeError(prman_scene.NOT_FOUND)
    if quality not in prman_scene.QUALITY:
        raise ValueError("quality is one of %s" % ", ".join(sorted(prman_scene.QUALITY)))
    out = render_gather.output_folder(folder, name)
    info = write_prman_scene(os.path.join(out, name + ".rib"), grooms, camera, start, end, step, width, height,
                             quality, meshes, density, motion_blur, image=os.path.join(out, name + ".png"))
    job = prman_scene.start(prman, info["scene"], info["image"].replace("\\", "/"), info["frames"], threads)
    job.info = info
    return job
