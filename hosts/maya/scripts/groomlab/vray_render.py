# SPDX-License-Identifier: Apache-2.0
"""Render the groom in V-Ray Standalone from any Maya (2022 too: V-Ray for Maya is not needed).

    import groomlab
    job = groomlab.render_vray()                        # this frame, the studio's (or the view's) camera
    job = groomlab.render_vray(start=1, end=120)        # a turntable or a shot: one image a frame
    job.poll()                                          # (frames done, frames, finished, percent)
    job.wait(); job.images()                            # the PNGs, in groomlab/<scene>/renders/vray_<date>

What goes to V-Ray, as one .vrscene beside the images (vray_scene.scene_text):
  - every visible groom, written by groomlab.export_vrscene over the frames (the render density and
    overrides; its V-Ray hair shader fitted to the Arnold look; the groom's world matrix);
  - the visible meshes (the scalp, the head, a studio's backdrop; or with meshes="selected" the
    selection and the grooms' scalps and colliders) as triangles with Maya's normals and a simple
    material from their shader's color, specular and roughness (aiStandardSurface, standardSurface,
    lambert and the like; textures are not followed: their color is);
  - the camera (camera=, else the studio's camera, else the viewport's, else a renderable one):
    its world matrix and the field of view the render resolution gives it (film fit);
  - the lights V-Ray can take as they are: Arnold area lights and Maya area lights (LightRectangle,
    exposure and normalize as Arnold has them, spread fitted as V-Ray's directional), sky domes (LightDome,
    their color: not a texture), directional lights (LightDirect); light links and meshes that cast
    no shadow. Without any of them, a grey dome and a key light from above the camera;
  - the resolution from the render settings (or width/height), the quality's samples.
A value that changes over the frames (the turntable's orbiting camera and lights, a deforming mesh)
is keyed per frame. vray.exe renders in the background (Maya stays free): GroomLab > Render > V-Ray
Standalone shows its progress, can stop it, and loads the image into Maya's Render View.

vray.exe is found from groomlab.set_vray_path's setting, the GROOMLAB_VRAY environment variable,
VRAY_PATH, the default install folder or the PATH (vray_scene.find_vray).
"""


import os

from maya import cmds

from groomlab import render_gather, vray_scene
# The scene as render_gather reads it (shared with the RenderMan render, prman_render); the names
# kept here for scripts that used them from this module.
from groomlab.render_gather import (LIGHT_TYPES, SKIPPED_LIGHT_TYPES, gather, mesh_material, render_camera,  # noqa: F401
                                    render_grooms, render_meshes, resolution, show_image)
from groomlab.render_gather import world_matrix as _matrix  # noqa: F401
from groomlab.vray_scene import spread_to_directional  # noqa: F401

PATH_VAR = "groomLabVRayStandalone"  # optionVar: vray.exe (or its folder)


# ---------------------------------------------------------------------------
# vray.exe

def vray_path():
    """The vray.exe GroomLab renders with, or None when none is found."""
    preferred = cmds.optionVar(query=PATH_VAR) if cmds.optionVar(exists=PATH_VAR) else None
    return vray_scene.find_vray(preferred or None)


def set_vray_path(path):
    """Remember vray.exe (or its folder) for V-Ray Standalone renders; None or "" forgets it."""
    if not path:
        if cmds.optionVar(exists=PATH_VAR):
            cmds.optionVar(remove=PATH_VAR)
        return None
    found = vray_scene._exe_in(path)
    if not found:
        raise ValueError("no %s at %s" % (vray_scene.EXE, path))
    cmds.optionVar(stringValue=(PATH_VAR, found))
    return found


# ---------------------------------------------------------------------------
# The scene

def write_vray_scene(path, grooms=None, camera=None, start=None, end=None, step=1, width=None, height=None,
                     quality="preview", meshes="visible", density=1.0):
    """Write a V-Ray Standalone scene of the render (the module's docstring says what it holds) to
    `path`, the grooms' .vrscene files beside it. Returns {"scene", "frames", "grooms" (shape ->
    its .vrscene), "camera", "lights" (V-Ray light names), "meshes", "skipped_lights", "width", "height"}."""
    import groomlab
    now = cmds.currentTime(query=True)
    start = now if start is None else start
    frames = vray_scene.frame_list(start, end if end is not None else start, step)
    if not frames:
        raise ValueError("no frames from %s to %s" % (start, end))
    width, height = (int(width or resolution()[0]), int(height or resolution()[1]))
    grooms = render_grooms(grooms)
    camera = render_camera(camera)
    shapes = render_meshes(grooms, meshes)
    folder = os.path.dirname(os.path.abspath(path))
    if not os.path.isdir(folder):
        os.makedirs(folder)
    groom_files = {}
    names = render_gather.unique_names(grooms)
    for groom in grooms:
        groom_path = os.path.join(folder, "groom_%s.vrscene" % names[groom]).replace("\\", "/")
        groomlab.export_vrscene(groom, groom_path, start=frames[0], end=frames[-1], step=max(1, int(step)),
                                density=density, render_overrides=True)
        groom_files[groom] = groom_path
    cam, lights, mesh_data, skipped = gather(frames, grooms, camera, shapes, float(width) / float(max(height, 1)))
    text = vray_scene.scene_text(width, height, includes=list(groom_files.values()), camera=cam, lights=lights,
                                 meshes=mesh_data, quality=quality,
                                 units={"meters_scale": 0.01, "fps": render_gather.fps(), "up": render_gather.up_axis()},
                                 comment="%s, camera %s" % (cmds.file(query=True, sceneName=True) or "untitled",
                                                            camera.split("|")[-1]))
    with open(path, "w") as f:
        f.write(text)
    return {"scene": path, "frames": frames, "grooms": groom_files, "camera": camera,
            "lights": [l["name"] for l in lights], "meshes": [m["shape"] for m in mesh_data], "skipped_lights": skipped,
            "width": width, "height": height}


def render_vray(grooms=None, camera=None, start=None, end=None, step=1, width=None, height=None, quality="preview",
                meshes="visible", folder=None, name="vray", density=1.0, threads=0):
    """Render the groom(s) in V-Ray Standalone in the background. Frames: `start` (default the
    current frame) to `end` by `step`; `quality` "draft", "preview" or "final"; `folder` the
    output folder (default <scene's groomlab folder>/renders/<name>_<date>). Returns a
    vray_scene.VRayJob (poll, wait, cancel, images) with .info (write_vray_scene's result). Raises
    RuntimeError when vray.exe is not found."""
    vray = vray_path()
    if not vray:
        raise RuntimeError(vray_scene.NOT_FOUND)
    if quality not in vray_scene.QUALITY:
        raise ValueError("quality is one of %s" % ", ".join(sorted(vray_scene.QUALITY)))
    out = render_gather.output_folder(folder, name)
    info = write_vray_scene(os.path.join(out, name + ".vrscene"), grooms, camera, start, end, step, width, height,
                            quality, meshes, density)
    job = vray_scene.start(vray, info["scene"], os.path.join(out, name + ".png").replace("\\", "/"), info["frames"],
                           threads)
    job.info = info
    return job
