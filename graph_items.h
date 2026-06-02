#ifndef GRAPH_ITEMS_H
#define GRAPH_ITEMS_H

#include <QGraphicsItem>
#include <QString>
#include <QList>
#include <QPen> // 新增 QPen

class GraphEdge;

// ... (GraphNode 类的代码保持完全不变) ...
class GraphNode : public QGraphicsItem
{
public:
    GraphNode(const QString& id, int type, const QString& label);

    void addEdge(GraphEdge *edge);
    QString getId() const { return m_id; }

    // 【新增接口】：实时修改节点的外观
    void setStyle(const QColor& color, int shape);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    QString m_id;
    int m_type;
    QString m_label;
    QList<GraphEdge *> m_edges;

    // 【新增属性】：储存当前节点样式
    QColor m_color;
    int m_shape; // 0: 圆角矩形, 1: 椭圆/圆形, 2: 菱形
};

// ================= GraphEdge 修改 =================
class GraphEdge : public QGraphicsItem
{
public:
    GraphEdge(GraphNode *sourceNode, GraphNode *destNode, int type);

    void adjust();

    // 【新增接口】：用于实时修改边的样式
    void setStyle(const QColor& color, int thickness, Qt::PenStyle style);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    GraphNode *m_source;
    GraphNode *m_dest;
    int m_type;
    QPointF m_sourcePoint;
    QPointF m_destPoint;

    // 【新增属性】：储存当前样式
    QColor m_color;
    int m_thickness;
    Qt::PenStyle m_penStyle;
};

#endif // GRAPH_ITEMS_H