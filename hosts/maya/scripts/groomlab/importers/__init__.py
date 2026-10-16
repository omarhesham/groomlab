# SPDX-License-Identifier: Apache-2.0
"""Importers of other hair systems' grooms (GroomLab > Import > From XGen..., From Yeti...).

    foreign   the mapping layer every importer shares: a foreign groom in GroomLab's terms
              (ForeignGroom, ForeignOp, MapSource), the import report, building the groom
    paths     finding the files a foreign groom names (tokens, environment variables, roots)
    textures  those files as GroomLab maps, copied into the scene's masks folder (Ptex too)
    seexpr    XGen's SeExpr attribute values as constants, map reads or expressions
    xgen      XGen's two flavors (Interactive Groom Splines and Core descriptions) read into the layer
    yeti_format  Yeti's scene data (graph XML, groom strands) read from Maya ASCII text, no Maya
    yeti      Yeti's graphs and grooms read into the layer, without Yeti's plugin
"""
