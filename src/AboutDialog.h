#pragma once
#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QTextEdit>
#include "Locale.h"

#ifndef SCENEWALL_VERSION
#define SCENEWALL_VERSION "0.1"
#endif

class AboutDialog : public QDialog {
    Q_OBJECT
public:
    AboutDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle(T("AboutTitle"));
        setFixedSize(400, 300);
        
        QVBoxLayout *layout = new QVBoxLayout(this);
        
        QLabel *title = new QLabel("SceneWall", this);
        title->setAlignment(Qt::AlignCenter);
        QFont font = title->font();
        font.setPointSize(16);
        font.setBold(true);
        title->setFont(font);
        layout->addWidget(title);
        
        QLabel *version = new QLabel(T("AboutVersion").arg(SCENEWALL_VERSION), this);
        version->setAlignment(Qt::AlignCenter);
        layout->addWidget(version);
        
        QTextEdit *license = new QTextEdit(this);
        license->setReadOnly(true);
        license->setPlainText(
            "MIT License\n\n"
            "Copyright (c) 2026 studiokrause/OpenCode/Gemini 3.1\n\n"
            "Permission is hereby granted, free of charge, to any person obtaining a copy "
            "of this software and associated documentation files (the \"Software\"), to deal "
            "in the Software without restriction, including without limitation the rights "
            "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell "
            "copies of the Software, and to permit persons to whom the Software is "
            "furnished to do so, subject to the following conditions:\n\n"
            "The above copyright notice and this permission notice shall be included in all "
            "copies or substantial portions of the Software.\n\n"
            "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR "
            "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, "
            "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE "
            "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER "
            "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, "
            "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE "
            "SOFTWARE."
        );
        layout->addWidget(license);
        
        QLabel *credits = new QLabel(T("AboutCreator") + ": studiokrause/OpenCode/Gemini 3.1", this);
        credits->setAlignment(Qt::AlignCenter);
        layout->addWidget(credits);
    }
};
