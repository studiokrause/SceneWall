#include "SettingsDialog.h"
#include "AboutDialog.h"

#include <QAbstractItemView>
#include <QColorDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QSizePolicy>
#include <QTableWidgetItem>
#include <QUuid>
#include <QVBoxLayout>

#include <obs-module.h>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(obs_module_text("Settings"));
    resize(288, 340);

    config = loadWallConfig();

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Two columns: name plus a single clickable colour picker.
    tabTable = new QTableWidget(0, 2, this);
    tabTable->setHorizontalHeaderLabels({obs_module_text("Name"), obs_module_text("Color")});
    tabTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tabTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    tabTable->setColumnWidth(1, 56);
    tabTable->verticalHeader()->setVisible(false);
    // Fixed, compact rows so the colour swatch fills the cell exactly and
    // leaves no empty strip underneath.
    tabTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    tabTable->verticalHeader()->setDefaultSectionSize(24);
    // Only ever one tab selected at a time.
    tabTable->setSelectionMode(QAbstractItemView::SingleSelection);
    tabTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mainLayout->addWidget(tabTable);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton(obs_module_text("AddTab"), this);
    QPushButton *removeBtn = new QPushButton(obs_module_text("RemoveTab"), this);
    QPushButton *aboutBtn = new QPushButton(obs_module_text("About"), this);
    QPushButton *saveBtn = new QPushButton(obs_module_text("Save"), this);
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(removeBtn);
    btnLayout->addWidget(aboutBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(saveBtn);
    mainLayout->addLayout(btnLayout);

    for (const TabConfig &tab : config.tabs)
        addTabRow(tab);

    connect(addBtn, &QPushButton::clicked, [this]() {
        TabConfig tab;
        tab.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        tab.name = obs_module_text("NewTab");
        tab.colorHex = "#808080";
        addTabRow(tab);
    });

    connect(removeBtn, &QPushButton::clicked, [this]() {
        const int row = tabTable->currentRow();
        if (row < 0)
            return;

        // The "All" tab is permanent and can never be removed.
        QTableWidgetItem *item = tabTable->item(row, 0);
        const QString id = item ? item->data(Qt::UserRole).toString() : QString();
        const int ti = id.isEmpty() ? -1 : config.indexOfTab(id);
        if (ti >= 0 && config.tabs[ti].isAll)
            return;

        tabTable->removeRow(row);
    });

    connect(aboutBtn, &QPushButton::clicked, [this]() {
        AboutDialog dialog(this);
        dialog.exec();
    });

    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::saveSettings);
}

void SettingsDialog::addTabRow(const TabConfig &tab)
{
    const int row = tabTable->rowCount();
    tabTable->insertRow(row);

    // "All" falls back to the translated default title when unnamed.
    const QString displayName =
            (tab.isAll && tab.name.isEmpty()) ? QString(obs_module_text("All")) : tab.name;

    QTableWidgetItem *nameItem = new QTableWidgetItem(displayName);
    // Stable identity so renaming/reordering rows cannot mix tabs up.
    nameItem->setData(Qt::UserRole, tab.id);
    tabTable->setItem(row, 0, nameItem);

    QPushButton *pickBtn = new QPushButton(this);
    // Fill the whole table cell (24px row) so the swatch has no empty strip
    // underneath and is easy to hit.
    pickBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    pickBtn->setMinimumHeight(24);
    pickBtn->setCursor(Qt::PointingHandCursor);
    pickBtn->setToolTip(obs_module_text("Color"));
    connect(pickBtn, &QPushButton::clicked, this,
            [this, pickBtn]() { pickColorFor(pickBtn); });

    tabTable->setCellWidget(row, 1, pickBtn);
    updateColorButton(pickBtn, tab.color());
}

int SettingsDialog::rowOf(QWidget *button) const
{
    for (int i = 0; i < tabTable->rowCount(); ++i)
        if (tabTable->cellWidget(i, 1) == button)
            return i;
    return -1;
}

void SettingsDialog::updateColorButton(QWidget *button, const QColor &color)
{
    QPushButton *btn = qobject_cast<QPushButton *>(button);
    if (!btn)
        return;

    // The button itself is the colour swatch; no hex text in the table.
    btn->setProperty("swatchColor", color);
    btn->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid #666; "
                               "border-radius: 3px; margin: 0; padding: 0; }")
                               .arg(color.name()));
}

void SettingsDialog::pickColorFor(QWidget *button)
{
    const int row = rowOf(button);
    if (row < 0)
        return;

    QColor current = button->property("swatchColor").value<QColor>();
    if (!current.isValid())
        current = QColor(128, 128, 128);

    // Non-native dialog so the swatch palette is always available; the hex
    // value is only ever shown inside this picker.
    const QColor chosen = QColorDialog::getColor(current, this, obs_module_text("Color"),
                                                 QColorDialog::DontUseNativeDialog);
    if (!chosen.isValid())
        return;

    updateColorButton(button, chosen);
}

void SettingsDialog::saveSettings()
{
    // Match rows back to tabs by id (not by position), preserving the "All"
    // flag and per-tab scene lists. Tabs removed from the table are dropped.
    QList<TabConfig> updated;

    for (int i = 0; i < tabTable->rowCount(); ++i) {
        QTableWidgetItem *nameItem = tabTable->item(i, 0);
        if (!nameItem)
            continue;

        const QString id = nameItem->data(Qt::UserRole).toString();
        const int existing = id.isEmpty() ? -1 : config.indexOfTab(id);

        TabConfig tab;
        if (existing >= 0)
            tab = config.tabs[existing];
        else
            tab.id = id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : id;

        const QString text = nameItem->text();
        tab.name = (tab.isAll && text == obs_module_text("All")) ? QString() : text;

        if (QWidget *w = tabTable->cellWidget(i, 1)) {
            const QColor c = w->property("swatchColor").value<QColor>();
            if (c.isValid())
                tab.colorHex = c.name();
        }

        updated.append(tab);
    }

    config.tabs = updated;
    saveWallConfig(config);
    accept();
}
