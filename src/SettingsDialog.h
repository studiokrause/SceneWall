#pragma once
#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(QWidget *parent = nullptr);
    void saveSettings();
    void loadSettings();
private:
    QTableWidget *tabTable;
    void addTabRow(const QString &name, const QString &color);
};
