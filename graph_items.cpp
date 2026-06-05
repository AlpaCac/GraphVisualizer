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
    setFlag(ItemIsSelectable); // 【新增】：允许被点击选中
    setZValue(10);
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
    // 【修改】：将选中边框换成统一的科幻亮青色 (#00d2d3)
    if (isSelected()) {
        // 顺便把边框厚度从 4 降到 3，让高亮光环更加锐利、干练
        painter->setPen(QPen(QColor("#00d2d3"), 3));
    } else {
        painter->setPen(QPen(Qt::darkGray, 2));
    }
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

    // 绘制内部文字
    // 【修改】：将显示门槛从 40 降低到 20，确保尺寸为 25 的普通节点也能显示 ID
    if (m_size >= 20) {
        painter->setPen(Qt::black);
        QFont font = painter->font();
        // 根据新尺寸自动适配一下字号：较小节点用 8 号字，较大节点用 10 号字
        font.setPointSize(m_size < 40 ? 8 : 10);
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
// 【核心修复】：使用初始化列表，赋予线条默认的颜色、粗细和线型
GraphEdge::GraphEdge(GraphNode *sourceNode, GraphNode *destNode, int type)
    : m_source(sourceNode), m_dest(destNode), m_type(type),
    m_color(Qt::darkGray), m_thickness(2), m_penStyle(Qt::SolidLine)
{
    setFlag(ItemIsSelectable);
    setZValue(-10);
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
    // 防御性判断：如果没有连接节点，则不绘制
    if (!m_source || !m_dest) return;

    QPen pen(m_color, m_thickness, m_penStyle);

    // 设置线段的两端为圆角 (RoundCap)，让插入节点时显得非常自然
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);

    // 判断是否被鼠标点击选中
    if (isSelected()) {
        pen.setColor(QColor("#00d2d3")); // 选中时的高亮亮青色
        pen.setWidth(m_thickness + 1);   // 选中时稍微加粗一点
    }

    painter->setPen(pen);

    // 【最关键的一句】：画出真正的连线！千万不能漏掉这一行！
    painter->drawLine(m_sourcePoint, m_destPoint);
}
// 【新增】：生成紧贴线条轮廓的 10 像素热区，方便用户用鼠标精准点中细线
QPainterPath GraphEdge::shape() const {
    QPainterPath path;
    if (!m_source || !m_dest) return path;
    path.moveTo(m_sourcePoint);
    path.lineTo(m_destPoint);

    QPainterPathStroker stroker;
    stroker.setWidth(10); // 设置点击热区的宽度为 10 像素
    return stroker.createStroke(path);
}