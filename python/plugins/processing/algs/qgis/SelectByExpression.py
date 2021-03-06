# -*- coding: utf-8 -*-

"""
***************************************************************************
    SelectByExpression.py
    ---------------------
    Date                 : July 2014
    Copyright            : (C) 2014 by Michael Douchin
***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************
"""

__author__ = 'Michael Douchin'
__date__ = 'July 2014'
__copyright__ = '(C) 2014, Michael Douchin'

# This will get replaced with a git SHA1 when you do a git archive

__revision__ = '$Format:%H$'

from qgis.core import (QgsApplication,
                       QgsExpression,
                       QgsVectorLayer,
                       QgsProcessingUtils)
from processing.core.GeoAlgorithmExecutionException import GeoAlgorithmExecutionException
from processing.core.parameters import ParameterVector
from processing.core.parameters import ParameterSelection
from processing.core.outputs import OutputVector
from processing.algs.qgis.QgisAlgorithm import QgisAlgorithm
from processing.core.parameters import ParameterExpression


class SelectByExpression(QgisAlgorithm):

    LAYERNAME = 'LAYERNAME'
    EXPRESSION = 'EXPRESSION'
    RESULT = 'RESULT'
    METHOD = 'METHOD'

    def icon(self):
        return QgsApplication.getThemeIcon("/providerQgis.svg")

    def svgIconPath(self):
        return QgsApplication.iconPath("providerQgis.svg")

    def group(self):
        return self.tr('Vector selection tools')

    def __init__(self):
        super().__init__()
        self.methods = [self.tr('creating new selection'),
                        self.tr('adding to current selection'),
                        self.tr('removing from current selection'),
                        self.tr('selecting within current selection')]

        self.addParameter(ParameterVector(self.LAYERNAME,
                                          self.tr('Input Layer')))
        self.addParameter(ParameterExpression(self.EXPRESSION,
                                              self.tr("Expression"), parent_layer=self.LAYERNAME))
        self.addParameter(ParameterSelection(self.METHOD,
                                             self.tr('Modify current selection by'), self.methods, 0))
        self.addOutput(OutputVector(self.RESULT, self.tr('Selected (expression)'), True))

    def name(self):
        return 'selectbyexpression'

    def displayName(self):
        return self.tr('Select by expression')

    def processAlgorithm(self, parameters, context, feedback):
        filename = self.getParameterValue(self.LAYERNAME)
        layer = QgsProcessingUtils.mapLayerFromString(filename, context)
        method = self.getParameterValue(self.METHOD)

        if method == 0:
            behavior = QgsVectorLayer.SetSelection
        elif method == 1:
            behavior = QgsVectorLayer.AddToSelection
        elif method == 2:
            behavior = QgsVectorLayer.RemoveFromSelection
        elif method == 3:
            behavior = QgsVectorLayer.IntersectSelection

        expression = self.getParameterValue(self.EXPRESSION)
        qExp = QgsExpression(expression)
        if qExp.hasParserError():
            raise GeoAlgorithmExecutionException(qExp.parserErrorString())

        layer.selectByExpression(expression, behavior)
        self.setOutputValue(self.RESULT, filename)
