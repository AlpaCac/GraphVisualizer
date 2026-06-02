#include "graph_items.h"
#include <QPainter>
#include <QGraphicsScene>

// ... (GraphNode 的实现代码保持完全不变) ...
GraphNode::GraphNode(const QString& id, int type, const QString& label) : m_id(id), m_type(type), m_label(label) {
    setFlag(ItemIsMovable); setFlag(ItemSendsGeometryChanges); setZValue(1);
}
void GraphNode::addEdge(GraphEdge *edge) { m_edges.append(edge); edge->adjust(); }
QRectF GraphNode::boundingRect() const { return QRectF(-50, -25, 100, 50); }
void GraphNode::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    painter->setBrush(m_type == 0 ? QColor(200, 230, 255) : QColor(255, 210, 180));
    painter->setPen(QPen(Qt::darkGray, 2));
    painter->drawRoundedRect(boundingRect(), 8, 8);
    painter->setPen(Qt::black); painter->drawText(boundingRect(), Qt::AlignCenter, m_label);
}
QVariant GraphNode::itemChange(GraphicsItemChange change, const QVariant &value) {
    if (change == ItemPositionHasChanged && scene()) { for (GraphEdge *edge : qAsConst(m_edges)) edge->adjust(); }
    return QGraphicsItem::itemChange(change, value);
}

// ================= GraphEdge 修改 =================
GraphEdge::GraphEdge(GraphNode *sourceNode, GraphNode *destNode, int type)
    : m_source(sourceNode), m_dest(destNode), m_type(type),
    m_color(type == 0 ? Qt::gray : Qt::darkBlue), // 默认颜色
    m_thickness(2),                               // 默认粗细
    m_penStyle(Qt::SolidLine)                     // 默认实线
{
    setZValue(0);
}

// 【新增】：接收新样式并触发重绘
void GraphEdge::setStyle(const QColor& color, int thickness, Qt::PenStyle style) {
    m_color = color;
    m_thickness = thickness;
    m_penStyle = style;
    update(); // 告诉 Qt 立即重新绘制这条线
}

void GraphEdge::adjust() {
    if (!m_source || !m_dest) return;
    m_sourcePoint = mapFromItem(m_source, 0, 0);
    m_destPoint = mapFromItem(m_dest, 0, 0);
    prepareGeometryChange();
}

QRectF GraphEdge::boundingRect() const {
    qreal extra = m_thickness + 2.0; // 根据粗细自动扩大包围盒防裁剪
    return QRectF(m_sourcePoint, QSizeF(m_destPoint.x() - m_sourcePoint.x(), m_destPoint.y() - m_sourcePoint.y()))
        .normalized().adjusted(-extra, -extra, extra, extra);
}

void GraphEdge::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    // 【修改】：使用我们自定义的变量，而不是写死
    QPen pen(m_color, m_thickness, m_penStyle);
    painter->setPen(pen);
    painter->drawLine(m_sourcePoint, m_destPoint);
}