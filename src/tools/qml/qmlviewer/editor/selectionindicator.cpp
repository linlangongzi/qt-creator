/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#include "selectionindicator.h"
#include "qdeclarativedesignview.h"
#include "qmlviewerconstants.h"

#include <QPen>
#include <cmath>
#include <QGraphicsScene>

namespace QmlViewer {

SelectionIndicator::SelectionIndicator(QDeclarativeDesignView *editorView, LayerItem *layerItem)
    : m_layerItem(layerItem), m_view(editorView)
{
}

SelectionIndicator::~SelectionIndicator()
{
    clear();
}

void SelectionIndicator::show()
{
    foreach (QGraphicsPolygonItem *item, m_indicatorShapeHash.values())
        item->show();
}

void SelectionIndicator::hide()
{
    foreach (QGraphicsPolygonItem *item, m_indicatorShapeHash.values())
        item->hide();
}

void SelectionIndicator::clear()
{
    if (m_layerItem) {
        foreach (QGraphicsItem *item, m_indicatorShapeHash.values())
            m_layerItem->scene()->removeItem(item);
    }
    m_indicatorShapeHash.clear();
}

//static void alignVertices(QPolygonF &polygon, double factor)
//{
//    QMutableVectorIterator<QPointF> iterator(polygon);
//    while (iterator.hasNext()) {
//        QPointF &vertex = iterator.next();
//        vertex.setX(std::floor(vertex.x()) + factor);
//        vertex.setY(std::floor(vertex.y()) + factor);
//    }
//
//}

QPolygonF SelectionIndicator::addBoundingRectToPolygon(QGraphicsItem *item, QPolygonF &polygon)
{
    polygon = polygon.united(item->mapToScene(item->boundingRect()));
    foreach(QGraphicsItem *child, item->childItems()) {
        if (!m_view->isEditorItem(child))
            addBoundingRectToPolygon(child, polygon);
    }
    return polygon;
}

void SelectionIndicator::setItems(const QList<QGraphicsObject*> &itemList)
{
    clear();

    // set selections to also all children if they are not editor items

    foreach (QGraphicsItem *item, itemList) {
        QGraphicsPolygonItem *newSelectionIndicatorGraphicsItem = new QGraphicsPolygonItem(m_layerItem.data());
        m_indicatorShapeHash.insert(item, newSelectionIndicatorGraphicsItem);

        QPolygonF boundingShapeInSceneSpace;
        addBoundingRectToPolygon(item, boundingShapeInSceneSpace);

        QPolygonF boundingRectInLayerItemSpace = m_layerItem->mapFromScene(boundingShapeInSceneSpace);


        QPen pen;
        pen.setColor(QColor(108, 141, 221));
        newSelectionIndicatorGraphicsItem->setData(Constants::EditorItemDataKey, QVariant(true));
        newSelectionIndicatorGraphicsItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
        newSelectionIndicatorGraphicsItem->setPolygon(boundingRectInLayerItemSpace);
        newSelectionIndicatorGraphicsItem->setPen(pen);
    }
}

void SelectionIndicator::updateItems(const QList<QGraphicsObject*> &itemList)
{
    foreach (QGraphicsItem *item, itemList) {
        if (m_indicatorShapeHash.contains(item)) {
            QGraphicsPolygonItem *indicatorGraphicsItem =  m_indicatorShapeHash.value(item);
            QPolygonF boundingRectInSceneSpace(item->mapToScene(item->boundingRect()));
            QPolygonF boundingRectInLayerItemSpace = m_layerItem->mapFromScene(boundingRectInSceneSpace);
            indicatorGraphicsItem->setPolygon(boundingRectInLayerItemSpace);
        }
    }
}

} //namespace QmlViewer

