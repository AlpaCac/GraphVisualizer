#include "graph_items.h"
#include <QPainter>
#include <QGraphicsScene>

GraphNode::GraphNode(const QString& id, int type, const QString& label)
    : m_id(id), m_type(type), m_label(label),
    m_color(type == 0 ? QColor(200, 230, 255) : QColor(255, 210, 180)),
    m_shape(0),
    m_size(80) // 默认尺寸为 80
{
    setFlag(ItemIsMovable);
    setFlag(ItemSendsGeometryChanges);
    setZValue(1);
}

void GraphNode::setStyle(const QColor& color, int shape, int size) {
    // 【核心极其重要】：在改变包围盒尺寸前，必须通知 Qt 图形引擎准备重绘，否则会产生残影！
    prepareGeometryChange();
    m_color = color;
    m_shape = shape;
    m_size = size;
    update();
}
void GraphNode::addEdge(GraphEdge *edge) { m_edges.append(edge); edge->adjust(); }
QRectF GraphNode::boundingRect() const {
    // 根据动态 size 生成包围盒，外加 20 像素的隐形拖拽边距
    double half = m_size / 2.0;
    return QRectF(-half - 10, -half - 10, m_size + 20, m_size + 20);
}

void GraphNode::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    painter->setBrush(m_color);
    painter->setPen(QPen(Qt::darkGray, 2));

    double R = m_size / 2.0; // 动态半径计算

    if (m_shape == 0) {
        // 0: 标准正圆
        painter->drawEllipse(QRectF(-R, -R, m_size, m_size));
    }
    else if (m_shape == 1) {
        // 1: 标准正方形
        painter->drawRect(QRectF(-R, -R, m_size, m_size));
    }
    else if (m_shape == 2) {
        // 2: 标准菱形
        QPolygonF diamond;
        diamond << QPointF(0, -R) << QPointF(R, 0) << QPointF(0, R) << QPointF(-R, 0);
        painter->drawPolygon(diamond);
    }
    else if (m_shape == 3) {
        // 3: 标准正六边形
        double Ry = R * 0.8660; // sin(60°)
        double Rx = R * 0.5;    // cos(60°)
        QPolygonF hexagon;
        hexagon << QPointF(-Rx, -Ry) << QPointF(Rx, -Ry) << QPointF(R, 0)
                << QPointF(Rx, Ry)   << QPointF(-Rx, Ry) << QPointF(-R, 0);
        painter->drawPolygon(hexagon);
    }

    // 绘制内部文字（叶子节点如果尺寸太小，可以隐藏文字或缩小字体，此处我们根据尺寸自动适配）
    if (m_size >= 40) {
        painter->setPen(Qt::black);
        QFont font = painter->font();
        font.setPointSize(m_size < 60 ? 7 : 9); // 小节点用小字体
        painter->setFont(font);
        painter->drawText(boundingRect(), Qt::AlignCenter, m_label);
    }
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