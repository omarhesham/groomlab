# SPDX-License-Identifier: Apache-2.0
"""The helper's client for the tests: the add-on's own (hosts/blender/addon/groomlab/helper_client.py,
plain Python with numpy), loaded from the sources so the tests and the add-on never drift apart."""

import importlib.util
import os

_SOURCE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "hosts", "blender", "addon", "groomlab",
                       "helper_client.py")
_spec = importlib.util.spec_from_file_location("groomlab_addon_helper_client", os.path.normpath(_SOURCE))
_module = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_module)

HelperClient = _module.HelperClient
HelperError = _module.HelperError
CREATE_NO_WINDOW = _module.CREATE_NO_WINDOW
