#pragma once

#include <QDialog>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "SceneWallConfig.h"

class SettingsDialog : public QDialog {
	Q_OBJECT
public:
	explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
	void saveSettings();
	void pickColor(int row, int column);

private:
	QTableWidget *tabTable;
	SceneWallConfigData config;

	void addTabRow(const QString &name, const QString &color, const QString &id, bool isAll);
	void loadSettings();
};
