#pragma once
// Sliding on/off switch (Qt Widgets has no built-in one).

#include <QAbstractButton>
#include <QPainter>
#include <QPropertyAnimation>
#include <QSize>

class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(qreal knobPos READ knobPos WRITE setKnobPos)

public:
    explicit ToggleSwitch(QWidget *parent = nullptr) : QAbstractButton(parent)
    {
        setCheckable(true);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setFocusPolicy(Qt::StrongFocus);

        m_anim = new QPropertyAnimation(this, "knobPos", this);
        m_anim->setDuration(120);
        m_anim->setEasingCurve(QEasingCurve::InOutQuad);

        connect(this, &QAbstractButton::toggled, this, [this](bool on) {
            m_anim->stop();
            m_anim->setStartValue(m_knobPos);
            m_anim->setEndValue(on ? 1.0 : 0.0);
            m_anim->start();
        });
    }

    qreal knobPos() const { return m_knobPos; }
    void setKnobPos(qreal pos)
    {
        m_knobPos = pos;
        update();
    }

    QSize sizeHint() const override { return QSize(40, 20); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const qreal radius = height() / 2.0;
        const bool on = isChecked();

        QColor trackColor = on ? palette().color(QPalette::Highlight) : QColor(105, 105, 105);
        if (!isEnabled())
            trackColor = trackColor.darker(150);

        p.setPen(Qt::NoPen);
        p.setBrush(trackColor);
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

        const qreal d = height() - 6.0;
        const qreal x = 3.0 + m_knobPos * (width() - d - 6.0);
        p.setBrush(QColor(240, 240, 240));
        p.drawEllipse(QRectF(x, 3.0, d, d));
    }

private:
    QPropertyAnimation *m_anim = nullptr;
    qreal m_knobPos = 0.0;
};
