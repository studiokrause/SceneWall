#include "SettingsDialog.h"

#include "AboutDialog.h"
#include "Locale.h"

#include <QColorDialog>
#include <QTableWidgetItem>

namespace {
void applyColorToItem(QTableWidgetItem *item, const QColor &color)
{
	if (!item)
		return;

	item->setText(color.name());
	item->setBackground(color);
	item->setForeground(color.lightness() < 128 ? Qt::white : Qt::black);
}
} // namespace

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(T("Settings"));
	auto *mainLayout = new QVBoxLayout(this);

	tabTable = new QTableWidget(0, 2, this);
	tabTable->setHorizontalHeaderLabels({T("Name"), T("Color")});
	mainLayout->addWidget(tabTable);

	auto *btnLayout = new QHBoxLayout();
	auto *addBtn = new QPushButton(T("AddTab"), this);
	auto *removeBtn = new QPushButton(T("RemoveTab"), this);
	auto *aboutBtn = new QPushButton(T("About"), this);
	auto *saveBtn = new QPushButton(T("Save"), this);
	btnLayout->addWidget(addBtn);
	btnLayout->addWidget(removeBtn);
	btnLayout->addStretch();
	btnLayout->addWidget(aboutBtn);
	btnLayout->addWidget(saveBtn);
	mainLayout->addLayout(btnLayout);

	connect(addBtn, &QPushButton::clicked, this,
		[this]() { addTabRow(T("NewTab"), defaultHeaderColor(), QString(), false); });
	connect(removeBtn, &QPushButton::clicked, this, [this]() {
		if (tabTable->currentRow() >= 0)
			tabTable->removeRow(tabTable->currentRow());
	});
	connect(aboutBtn, &QPushButton::clicked, this, [this]() {
		AboutDialog dialog(this);
		dialog.exec();
	});
	connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::saveSettings);
	connect(tabTable, &QTableWidget::cellClicked, this, &SettingsDialog::pickColor);

	loadSettings();
}

void SettingsDialog::addTabRow(const QString &name, const QString &color, const QString &id, bool isAll)
{
	const int row = tabTable->rowCount();
	tabTable->insertRow(row);

	auto *nameItem = new QTableWidgetItem(name);
	nameItem->setData(Qt::UserRole, id);
	nameItem->setData(Qt::UserRole + 1, isAll);
	tabTable->setItem(row, 0, nameItem);

	auto *colorItem = new QTableWidgetItem();
	colorItem->setFlags(colorItem->flags() & ~Qt::ItemIsEditable);
	tabTable->setItem(row, 1, colorItem);

	const QColor parsed(color);
	applyColorToItem(colorItem, parsed.isValid() ? parsed : QColor(defaultHeaderColor()));
}

void SettingsDialog::pickColor(int row, int column)
{
	if (column != 1)
		return;

	QTableWidgetItem *item = tabTable->item(row, column);
	const QColor initial = item ? QColor(item->text()) : QColor(defaultHeaderColor());
	const QColor chosen = QColorDialog::getColor(initial.isValid() ? initial : QColor(defaultHeaderColor()),
						     this, T("SelectTabColor"));
	if (chosen.isValid())
		applyColorToItem(item, chosen);
}

void SettingsDialog::saveSettings()
{
	config.tabs.clear();
	for (int i = 0; i < tabTable->rowCount(); ++i) {
		QTableWidgetItem *nameItem = tabTable->item(i, 0);
		QTableWidgetItem *colorItem = tabTable->item(i, 1);
		if (!nameItem || nameItem->text().isEmpty())
			continue;

		SceneWallTab tab;
		tab.id = nameItem->data(Qt::UserRole).toString();
		if (tab.id.isEmpty())
			tab.id = newSceneWallTabId();
		tab.isAll = nameItem->data(Qt::UserRole + 1).toBool();
		tab.name = nameItem->text();
		tab.color = colorItem ? colorItem->text() : defaultHeaderColor();
		config.tabs.append(tab);
	}

	saveSceneWallConfig(config);
	accept();
}

void SettingsDialog::loadSettings()
{
	config = loadSceneWallConfig();
	for (const SceneWallTab &tab : config.tabs)
		addTabRow(tab.name, tab.color, tab.id, tab.isAll);
}
