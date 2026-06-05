#ifndef GRAPH_ITEMS_H
#define GRAPH_ITEMS_H

#include <QGraphicsItem>
#include <QString>
#include <QList>
#include <QPen> // 新增 QPen
#include <QPainterPath> // 【新增】

class GraphEdge;

// ... (GraphNode 类的代码保持完全不变) ...
class GraphNode : public QGraphicsItem
{
public:
    // 【新增】：定义安全的类型标识，方便后续转换
    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    // 【新增】：暴露获取 Label 的接口
    QString getLabel() const { return m_label; }

    GraphNode(const QString& id, int type, const QString& label);

    void addEdge(GraphEdge *edge);
    QString getId() const { return m_id; }

    // 【新增接口】：实时修改节点的外观
    void setStyle(const QColor& color, int shape);
    void setStyle(const QColor& color, int shape, int size = 80);

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
    int m_size;
};

// ================= GraphEdge 修改 =================
class GraphEdge : public QGraphicsItem
{
public:
    // 【新增】：定义安全的类型标识
    enum { Type = UserType + 2 };
    int type() const override { return Type; }

    // 【新增】：暴露获取两端节点的接口
    GraphNode* getSource() const { return m_source; }
    GraphNode* getDest() const { return m_dest; }

    // 【新增】：重写热区函数，确保点击精准度
    QPainterPath shape() const override;

    // 【修改】：在末尾增加 const QString& linkType 参数
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