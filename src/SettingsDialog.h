#pragma once
#include "Config.h"

#include <QColor>
#include <QDialog>
#include <QPushButton>
#include <QTableWidget>

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(QWidget *parent = nullptr);

private:
    QTableWidget *tabTable;
    WallConfig config;

    void addTabRow(const TabConfig &tab);
    void pickColorFor(QWidget *button);
    void updateColorButton(QWidget *button, const QColor &color);
    int rowOf(QWidget *button) const;
    void saveSettings();
};
