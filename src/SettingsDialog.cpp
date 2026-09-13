#include "SettingsDialog.h"
#include <QTableWidgetItem>
#include <QColorDialog>
#include <obs.h>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("SceneWall Settings");
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    tabTable = new QTableWidget(0, 2, this);
    tabTable->setHorizontalHeaderLabels({"Name", "Color"});
    mainLayout->addWidget(tabTable);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton("Add Tab", this);
    QPushButton *removeBtn = new QPushButton("Remove Tab", this);
    QPushButton *saveBtn = new QPushButton("Save", this);
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(removeBtn);
    btnLayout->addWidget(saveBtn);
    mainLayout->addLayout(btnLayout);

    connect(addBtn, &QPushButton::clicked, [this]() {
        addTabRow("New Tab", "#FFFFFF");
    });
    connect(removeBtn, &QPushButton::clicked, [this]() {
        if (tabTable->currentRow() >= 0) tabTable->removeRow(tabTable->currentRow());
    });
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::saveSettings);

    loadSettings();
}

void SettingsDialog::addTabRow(const QString &name, const QString &color) {
    int row = tabTable->rowCount();
    tabTable->insertRow(row);
    tabTable->setItem(row, 0, new QTableWidgetItem(name));
    tabTable->setItem(row, 1, new QTableWidgetItem(color));
}

QString getConfigPath() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/obs-studio/plugin_config/SceneWall/";
    QDir().mkpath(path);
    return path + "tabs.json";
}

void SettingsDialog::saveSettings() {
    QJsonArray tabs;
    for (int i = 0; i < tabTable->rowCount(); ++i) {
        QJsonObject tab;
        tab["name"] = tabTable->item(i, 0)->text();
        tab["color"] = tabTable->item(i, 1)->text();
        tabs.append(tab);
    }
    
    QJsonDocument doc(tabs);
    QFile file(getConfigPath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    }
    accept();
}

void SettingsDialog::loadSettings() {
    QFile file(getConfigPath());
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonArray tabs = doc.array();
        for (const auto& val : tabs) {
            QJsonObject tab = val.toObject();
            addTabRow(tab["name"].toString(), tab["color"].toString());
        }
        file.close();
    } else {
        addTabRow("All", "#808080");
    }
}
