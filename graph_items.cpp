#include "graph_items.h"
#include <QPainter>
#include <QGraphicsScene>

// ... (GraphNode 的实现代码保持完全不变) ...
GraphNode::GraphNode(const QString& id, int type, const QString& label)
    : m_id(id), m_type(type), m_label(label),
    m_color(type == 0 ? QColor(200, 230, 255) : QColor(255, 210, 180)), // 默认颜色维持原样
    m_shape(0) // 默认形状 0 (圆角矩形)
{
    setFlag(ItemIsMovable);
    setFlag(ItemSendsGeometryChanges);
    setZValue(1);
}
void GraphNode::setStyle(const QColor& color, int shape) {
    m_color = color;
    m_shape = shape;
    update(); // 告诉 Qt 立即重新绘制这个节点
}
void GraphNode::addEdge(GraphEdge *edge) { m_edges.append(edge); edge->adjust(); }
QRectF GraphNode::boundingRect() const {
    // 【核心修复】：将包围盒统一修改为 100x100 的绝对正方形
    // 这是绘制标准正多边形（正菱形、正六边形、正圆）的数学基础
    return QRectF(-50, -50, 100, 100);
}
void GraphNode::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    painter->setBrush(m_color);
    painter->setPen(QPen(Qt::darkGray, 2));

    // 根据 shape 变量画出标准的几何形状
    if (m_shape == 0) {
        // 0: 标准正圆 (直径 80)
        painter->drawEllipse(QRectF(-40, -40, 80, 80));
    }
    else if (m_shape == 1) {
        // 1: 标准正方形 (边长 80)
        painter->drawRect(QRectF(-40, -40, 80, 80));
    }
    else if (m_shape == 2) {
        // 2: 标准菱形 (对角线完全相等，即旋转 45 度的正方形)
        QPolygonF diamond;
        diamond << QPointF(0, -50)    // 上顶点
                << QPointF(50, 0)     // 右顶点
                << QPointF(0, 50)     // 下顶点
                << QPointF(-50, 0);   // 左顶点
        painter->drawPolygon(diamond);
    }
    else if (m_shape == 3) {
        // 3: 标准正六边形 (平顶)
        // 数学计算：半径 R=50。
        // X轴偏移量：R * cos(60°) = 50 * 0.5 = 25
        // Y轴偏移量：R * sin(60°) ≈ 50 * 0.8660 = 43.3
        QPolygonF hexagon;
        hexagon << QPointF(-25, -43.3)  // 左上
                << QPointF(25, -43.3)   // 右上
                << QPointF(50, 0)       // 右中
                << QPointF(25, 43.3)    // 右下
                << QPointF(-25, 43.3)   // 左下
                << QPointF(-50, 0);     // 左中
        painter->drawPolygon(hexagon);
    }

    // 绘制内部文字（在 100x100 的包围盒中绝对居中）
    painter->setPen(Qt::black);
    painter->drawText(boundingRect(), Qt::AlignCenter, m_label);
}
QVariant GraphNode::itemChange(GraphicsItemChange change, const QVariant &value) {
    if (change == ItemPositionHasChanged && scene()) {
        for (GraphEdge *edge : qAsConst(m_edges)) {
            edge->adjust();
        }
    }
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