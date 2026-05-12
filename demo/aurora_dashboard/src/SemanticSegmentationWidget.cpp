#include "SemanticSegmentationWidget.h"
#include <QPainter>
#include <QStyleOption>
#include <QMap>

// 英文類別名稱對應中文翻譯 (COCO 80類 + 戶外18類)
static const QMap<QString, QString>& segLabelZhMap() {
    static const QMap<QString, QString> map = {
        // COCO 80 classes
        {"person",          "人"},
        {"bicycle",         "自行車"},
        {"car",             "轎車"},
        {"motorcycle",      "機車"},
        {"airplane",        "飛機"},
        {"bus",             "公車"},
        {"train",           "火車"},
        {"truck",           "卡車"},
        {"boat",            "船"},
        {"traffic light",   "紅綠燈"},
        {"fire hydrant",    "消防栓"},
        {"stop sign",       "停止標誌"},
        {"parking meter",   "停車計費錶"},
        {"bench",           "長椅"},
        {"bird",            "鳥"},
        {"cat",             "貓"},
        {"dog",             "狗"},
        {"horse",           "馬"},
        {"sheep",           "羊"},
        {"cow",             "牛"},
        {"elephant",        "大象"},
        {"bear",            "熊"},
        {"zebra",           "斑馬"},
        {"giraffe",         "長頸鹿"},
        {"backpack",        "背包"},
        {"umbrella",        "雨傘"},
        {"handbag",         "手提包"},
        {"tie",             "領帶"},
        {"suitcase",        "行李箱"},
        {"frisbee",         "飛盤"},
        {"skis",            "滑雪板"},
        {"snowboard",       "單板滑雪"},
        {"sports ball",     "運動球"},
        {"kite",            "風箏"},
        {"baseball bat",    "棒球棒"},
        {"baseball glove",  "棒球手套"},
        {"skateboard",      "滑板"},
        {"surfboard",       "衝浪板"},
        {"tennis racket",   "網球拍"},
        {"bottle",          "瓶子"},
        {"wine glass",      "酒杯"},
        {"cup",             "杯子"},
        {"fork",            "叉子"},
        {"knife",           "刀"},
        {"spoon",           "湯匙"},
        {"bowl",            "碗"},
        {"banana",          "香蕉"},
        {"apple",           "蘋果"},
        {"sandwich",        "三明治"},
        {"orange",          "柳橙"},
        {"broccoli",        "花椰菜"},
        {"carrot",          "胡蘿蔔"},
        {"hot dog",         "熱狗"},
        {"pizza",           "披薩"},
        {"donut",           "甜甜圈"},
        {"cake",            "蛋糕"},
        {"chair",           "椅子"},
        {"couch",           "沙發"},
        {"potted plant",    "盆栽"},
        {"bed",             "床"},
        {"dining table",    "餐桌"},
        {"toilet",          "馬桶"},
        {"tv",              "電視"},
        {"laptop",          "筆記型電腦"},
        {"mouse",           "滑鼠"},
        {"remote",          "遙控器"},
        {"keyboard",        "鍵盤"},
        {"cell phone",      "手機"},
        {"microwave",       "微波爐"},
        {"oven",            "烤箱"},
        {"toaster",         "烤麵包機"},
        {"sink",            "水槽"},
        {"refrigerator",    "冰箱"},
        {"book",            "書"},
        {"clock",           "時鐘"},
        {"vase",            "花瓶"},
        {"scissors",        "剪刀"},
        {"teddy bear",      "玩具熊"},
        {"hair drier",      "吹風機"},
        {"toothbrush",      "牙刷"},
        // 戶外分割模型 (18類)
        {"road",            "道路"},
        {"sidewalk",        "人行道"},
        {"building",        "建築物"},
        {"wall",            "牆壁"},
        {"fence",           "圍欄"},
        {"pole",            "桿子"},
        {"traffic sign",    "交通標誌"},
        {"vegetation",      "植被"},
        {"terrain",         "地形"},
        {"sky",             "天空"},
        {"rider",           "騎士"},
    };
    return map;
}

static QString toZh(const QString& en) {
    const auto& map = segLabelZhMap();
    auto it = map.find(en);
    return (it != map.end()) ? it.value() : QString();
}

SemanticSegmentationWidget::SemanticSegmentationWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumHeight(0);
    setStyleSheet("background-color: #1a1a1a;");
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void SemanticSegmentationWidget::updateSegmentationFrame(QImage img, QString label) {
    segmentationImage_ = img;
    dominantLabel_ = label;
    hasFrame_ = !img.isNull();
    update();
}

void SemanticSegmentationWidget::clearFrame() {
    segmentationImage_ = QImage();
    dominantLabel_.clear();
    hasFrame_ = false;
    update();
}

void SemanticSegmentationWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0x0f, 0x0f, 0x1a));

    if (!hasFrame_) {
        painter.setPen(QColor(0x88, 0x88, 0x88));
        painter.setFont(QFont("Arial", 12));
        painter.drawText(rect(), Qt::AlignCenter, "No segmentation signal");
        return;
    }

    // Scale image to fit widget while preserving aspect ratio
    QSize imgSize = segmentationImage_.size();
    QSize widgetSize = size();

    if (imgSize.isEmpty()) {
        return;
    }

    double imgRatio = (double)imgSize.width() / imgSize.height();
    double widgetRatio = (double)widgetSize.width() / widgetSize.height();

    QRect destRect;
    if (imgRatio > widgetRatio) {
        // Image is wider than widget
        int newWidth = widgetSize.width();
        int newHeight = (int)(newWidth / imgRatio);
        int y = (widgetSize.height() - newHeight) / 2;
        destRect = QRect(0, y, newWidth, newHeight);
    } else {
        // Image is taller than widget
        int newHeight = widgetSize.height();
        int newWidth = (int)(newHeight * imgRatio);
        int x = (widgetSize.width() - newWidth) / 2;
        destRect = QRect(x, 0, newWidth, newHeight);
    }

    painter.drawImage(destRect, segmentationImage_);

    // Draw dominant label (yellow, top-left, bold) with Chinese translation
    if (!dominantLabel_.isEmpty()) {
        QString zh = toZh(dominantLabel_);
        QString displayText = zh.isEmpty() ? dominantLabel_ : dominantLabel_ + "  " + zh;

        painter.setFont(QFont("Arial", 14, QFont::Bold));
        painter.setPen(QColor(0, 0, 0));          // shadow
        painter.drawText(11, 36, displayText);
        painter.setPen(QColor(0xff, 0xff, 0x00)); // yellow
        painter.drawText(10, 35, displayText);
    }

    // Draw title
    painter.setPen(QColor(0xcc, 0xcc, 0xcc));
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.drawText(10, 20, "Semantic Segmentation");
}
