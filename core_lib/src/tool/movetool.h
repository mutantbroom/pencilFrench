/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef MOVETOOL_H
#define MOVETOOL_H

#include "basetool.h"
#include "movemode.h"
#include "preferencemanager.h"

class Layer;
class VectorImage;


class MoveTool : public BaseTool
{
    Q_OBJECT
public:
    explicit MoveTool(QObject* parent);
    ToolType type() override;
    void loadSettings() override;
    QCursor cursor() override;
    QCursor cursor(MoveMode mode) const;

    void pointerPressEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;

    bool leavingThisTool() override;

    void resetToDefault() override;
    void setShowSelectionInfo(const bool b) override;

private:
    void applyTransformation();
    void updateSettings(const SETTING setting);

    void beginInteraction(Qt::KeyboardModifiers keyMod, Layer* layer);
    void createVectorSelection(Qt::KeyboardModifiers keyMod, Layer* layer);
    void transformSelection(Qt::KeyboardModifiers keyMod);
    void storeClosestVectorCurve(Layer* layer);

    void setCurveSelected(VectorImage* vectorImage, Qt::KeyboardModifiers keyMod);
    void setAreaSelected(VectorImage* vectorImage, Qt::KeyboardModifiers keyMod);

    Layer* currentPaintableLayer();

    qreal mRotatedAngle = 0.0;
    qreal mPreviousAngle = 0.0;
    int mRotationIncrement = 0;
    MoveMode mPerspMode;
    QPointF mOffset;
};

#endif
