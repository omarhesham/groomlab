# SPDX-License-Identifier: Apache-2.0
"""Links GroomLab opens in the browser: the docs and Support GroomLab.

GroomLab is free for any use; it is funded by donations (DONATE_URL).

    groomlab.links.open_donate()     opens the Support GroomLab page (Ko-fi)
    groomlab.links.open_url(url)     opens any link; False when no browser opened it
"""

DOCS_URL = "https://getgroomlab.com/docs"
DONATE_URL = "https://ko-fi.com/getgroomlab"
REPO_URL = "https://github.com/thekesslereffect/groomlab"  # the source (Apache-2.0)


def _open_in_browser(url):
    try:
        from groomlab.ui.qt import QtCore, QtGui
        if QtGui.QDesktopServices.openUrl(QtCore.QUrl(url)):
            return True
    except ImportError:
        pass
    import webbrowser
    return webbrowser.open(url)


# What opens a link: a test replaces it to see the URL without a browser.
OPEN_URL = _open_in_browser


def open_url(url):
    return OPEN_URL(url)


def open_donate():
    """GroomLab > Help > Support GroomLab: the donations page in the browser."""
    open_url(DONATE_URL)
    return DONATE_URL
