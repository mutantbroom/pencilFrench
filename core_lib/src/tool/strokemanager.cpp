/***************************************************************************
 * This code is heavily influenced by the instrument proxy from QAquarelle *
 * QAquarelle -   Copyright (C) 2009 by Anton R. <commanderkyle@gmail.com> *
 *                                                                         *
 *   QAquarelle is free software; you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "strokemanager.h"

#include <cmath>
#include <limits>
#include <QDebug>
#include <QLineF>
#include <QPainterPath>
#include "object.h"
#include "pointerevent.h"


StrokeManager::StrokeManager()
{
    m_timeshot = 0;

    mTabletInUse = false;
    mTabletPressure = 0;

    reset();
    connect(&timer, &QTimer::timeout, this, &StrokeManager::interpolatePollAndPaint);
}

void StrokeManager::reset()
{
    mStrokeStarted = false;
    pressureQueue.clear();
    strokeQueue.clear();
    pressure = 0.0f;
    mHasTangent = false;
    timer.stop();
    mStabilizerLevel = -1;
}

void StrokeManager::setPressure(float pressure)
{
    mTabletPressure = pressure;
}

void StrokeManager::pointerPressEvent(PointerEvent* event)
{
    reset();
    if (!(event->button() == Qt::NoButton)) // if the user is pressing the left/right button
    {
        //qDebug() << "press";
        mLastPressPixel = mCurrentPressPixel;
        mCurrentPressPixel = event->posF();
    }

    mLastPixel = mCurrentPixel = event->posF();

    mStrokeStarted = true;
    setPressure(event->pressure());
}

void StrokeManager::pointerMoveEvent(PointerEvent* event)
{
    // only applied to drawing tools.
    if (mStabilizerLevel != -1)
    {
        smoothMousePos(event->posF());
    }
    else
    {
        // No smoothing
        mLastPixel = mCurrentPixel;
        mCurrentPixel = event->posF();
        mLastInterpolated = mCurrentPixel;
    }
    if(event->isTabletEvent())
    {
        setPressure(event->pressure());
    }
}

void StrokeManager::pointerReleaseEvent(PointerEvent* event)
{
    // flush out stroke
    if (mStrokeStarted)
    {
        pointerMoveEvent(event);
    }

    mStrokeStarted = false;
}

void StrokeManager::setStabilizerLevel(int level)
{
    mStabilizerLevel = level;
}

void StrokeManager::smoothMousePos(QPointF pos)
{
    // Smooth mouse position before drawing
    QPointF smoothPos;

    if (mStabilizerLevel == StabilizationLevel::NONE)
    {
        mLastPixel = mCurrentPixel;
        mCurrentPixel = pos;
        mLastInterpolated = mCurrentPixel;
    }
    else if (mStabilizerLevel == StabilizationLevel::SIMPLE)
    {
        // simple interpolation
        smoothPos = QPointF((pos.x() + mCurrentPixel.x()) / 2.0, (pos.y() + mCurrentPixel.y()) / 2.0);
        mLastPixel = mCurrentPixel;
        mCurrentPixel = smoothPos;
        mLastInterpolated = mCurrentPixel;

        // shift queue
        while (strokeQueue.size() >= STROKE_QUEUE_LENGTH)
        {
            strokeQueue.pop_front();
        }

        strokeQueue.push_back(smoothPos);
    }
    else if (mStabilizerLevel == StabilizationLevel::STRONG)
    {
        smoothPos = QPointF((pos.x() + mLastInterpolated.x()) / 2.0, (pos.y() + mLastInterpolated.y()) / 2.0);

        mLastInterpolated = mCurrentPixel;
        mCurrentPixel = smoothPos;
        mLastPixel = mLastInterpolated;
    }

    mousePos = pos;

    if (!mStrokeStarted)
    {
        return;
    }

    if (!mTabletInUse)   // a mouse is used instead of a tablet
    {
        setPressure(1.0);
    }
}


QPointF StrokeManager::interpolateStart(QPointF firstPoint)
{
    if (mStabilizerLevel == StabilizationLevel::SIMPLE)
    {
        // Clear queue
        strokeQueue.clear();
        pressureQueue.clear();

        mSingleshotTime.start();
        previousTime = mSingleshotTime.elapsed();

        mLastPixel = firstPoint;
    }
    else if (mStabilizerLevel == StabilizationLevel::STRONG)
    {
        mSingleshotTime.start();
        previousTime = mSingleshotTime.elapsed();

        // Clear queue
        strokeQueue.clear();
        pressureQueue.clear();

        const int sampleSize = 5;
        Q_ASSERT(sampleSize > 0);

        // fill strokeQueue with firstPoint x times
        for (int i = sampleSize; i > 0; i--)
        {
            strokeQueue.enqueue(firstPoint);
        }

        // last interpolated stroke should always be firstPoint
        mLastInterpolated = firstPoint;

        // draw and poll each millisecond
        timer.setInterval(sampleSize);
        timer.start();
    }
    else if (mStabilizerLevel == StabilizationLevel::NONE)
    {
        // Clear queue
        strokeQueue.clear();
        pressureQueue.clear();

        mLastPixel = firstPoint;
    }
    return firstPoint;
}

void StrokeManager::interpolatePoll()
{
    // remove oldest stroke
    strokeQueue.dequeue();

    // add new stroke with the last interpolated pixel position
    strokeQueue.enqueue(mLastInterpolated);
}

void StrokeManager::interpolatePollAndPaint()
{
    //qDebug() <<"inpol:" << mStabilizerLevel << "strokes"<< strokeQueue;
    if (!strokeQueue.isEmpty())
    {
        interpolatePoll();
        interpolateStroke();
    }
}

QList<QPointF> StrokeManager::interpolateStroke()
{
    // is nan initially
    QList<QPointF> result;

    if (mStabilizerLevel == StabilizationLevel::SIMPLE)
    {
        result = tangentInpolOp(result);

    }
    else if (mStabilizerLevel == StabilizationLevel::STRONG)
    {
        qreal x = 0;
        qreal y = 0;
        qreal pressure = 0;
        result = meanInpolOp(result, x, y, pressure);

    }
    else if (mStabilizerLevel == StabilizationLevel::NONE)
    {
        result = noInpolOp(result);
    }
    return result;
}

QList<QPointF> StrokeManager::noInpolOp(QList<QPointF> points)
{
    setPressure(getPressure());

    points << mLastPixel << mLastPixel << mCurrentPixel << mCurrentPixel;

    // Set lastPixel to CurrentPixel
    // new interpolated pixel
    mLastPixel = mCurrentPixel;

    return points;
}

QList<QPointF> StrokeManager::tangentInpolOp(QList<QPointF> points)
{
    int time = mSingleshotTime.elapsed();
    static const qreal smoothness = 1.f;
    QLineF line(mLastPixel, mCurrentPixel);

    qreal scaleFactor = line.length() * 3.f;

    if (!mHasTangent && scaleFactor > 0.01f)
    {
        mHasTangent = true;
        /*
        qDebug() << "scaleFactor" << scaleFactor
                 << "current pixel " << mCurrentPixel
                 << "last pixel" << mLastPixel;
         */
        m_previousTangent = (mCurrentPixel - mLastPixel) * smoothness / (3.0 * scaleFactor);
        //qDebug() << "previous tangent" << m_previousTangent;
        QLineF _line(QPointF(0, 0), m_previousTangent);
        // don't bother for small tangents, as they can induce single pixel wobbliness
        if (_line.length() < 2)
        {
            m_previousTangent = QPointF(0, 0);
        }
    }
    else
    {
        QPointF c1 = mLastPixel + m_previousTangent * scaleFactor;
        QPointF newTangent = (mCurrentPixel - c1) * smoothness / (3.0 * scaleFactor);
        //qDebug() << "scalefactor1=" << scaleFactor << m_previousTangent << newTangent;
        if (scaleFactor == 0)
        {
            newTangent = QPointF(0, 0);
        }
        else
        {
            //QLineF _line(QPointF(0,0), newTangent);
            //if (_line.length() < 2)
            //{
            //    newTangent = QPointF(0,0);
            //}
        }
        QPointF c2 = mCurrentPixel - newTangent * scaleFactor;
        //c1 = mLastPixel;
        //c2 = mCurrentPixel;
        points << mLastPixel << c1 << c2 << mCurrentPixel;
        //qDebug() << mLastPixel << c1 << c2 << mCurrentPixel;
        m_previousTangent = newTangent;
    }

    previousTime = time;
    return points;
}

// Mean sampling interpolation operation
QList<QPointF> StrokeManager::meanInpolOp(QList<QPointF> points, qreal x, qreal y, qreal pressure)
{
    for (int i = 0; i < strokeQueue.size(); i++)
    {
        x += strokeQueue[i].x();
        y += strokeQueue[i].y();
        pressure += getPressure();
    }

    // get arithmetic mean of x, y and pressure
    x /= strokeQueue.size();
    y /= strokeQueue.size();
    pressure /= strokeQueue.size();

    // Use our interpolated points
    QPointF mNewInterpolated(x, y);

    points << mLastPixel << mLastInterpolated << mNewInterpolated << mCurrentPixel;

    // Set lastPixel non interpolated pixel to our
    // new interpolated pixel
    mLastPixel = mNewInterpolated;

    return points;
}

void StrokeManager::interpolateEnd()
{
    // Stop timer
    timer.stop();
    if (mStabilizerLevel == StabilizationLevel::STRONG)
    {
        if (!strokeQueue.isEmpty())
        {
            // How many samples should we get point from?
            // TODO: Qt slider.
            int sampleSize = 5;

            Q_ASSERT(sampleSize > 0);
            for (int i = sampleSize; i > 0; i--)
            {
                interpolatePoll();
                interpolateStroke();
            }
        }
    }
}
