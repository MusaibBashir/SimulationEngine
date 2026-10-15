#include "MainWindow.hpp"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QSaveFile>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <sstream>
#include "AskPanel.hpp"
#include "Compiler.hpp"
#include "DocumentFormat.hpp"
#include "FlowView.hpp"
#include "ModelEditor.hpp"
#include "ModuleSchema.hpp"
#include "ResultsView.hpp"
#include "RunSetup.hpp"
#include "SettingsDialog.hpp"
#include "Theme.hpp"
#include "TuiState.hpp"   // templateFor(), starterModelText()

MainWindow::MainWindow(const QString& path, QWidget* parent)
    : QMainWindow(parent), m_editor(new ModelEditor(this)), m_flow(new FlowView(this)),
      m_results(new ResultsView(this)), m_tabs(new QTabWidget(this)), m_modules(new QTreeWidget(this)),
      m_problems(new QTreeWidget(this)), m_ask(new AskPanel(this)), m_runPicker(new QComboBox(this)),
      m_compileState(new QLabel(this)), m_progress(new QProgressBar(this)), m_reparseTimer(new QTimer(this)),
      m_runTimer(new QTimer(this)), m_runAction(nullptr), m_stopAction(nullptr) {
    setWindowIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    m_tabs->setDocumentMode(false);
    m_tabs->addTab(m_editor, "Model");
    m_tabs->addTab(m_flow, "Flow");
    m_tabs->addTab(m_results, "Results");
    auto* central = new QWidget(this);
    auto* lay = new QVBoxLayout(central);
    lay->setContentsMargins(8, 8, 8, 0);
    lay->addWidget(m_tabs);
    setCentralWidget(central);

    buildMenusAndToolbar();
    buildDocks();

    statusBar()->addWidget(m_compileState, 1);
    m_progress->setMaximumWidth(220);
    m_progress->hide();
    statusBar()->addPermanentWidget(m_progress);

    // Re-read the text a moment after typing stops, not on every key: a
    // compile is quick, but not quick enough to run between keystrokes.
    m_reparseTimer->setSingleShot(true);
    m_reparseTimer->setInterval(250);
    connect(m_reparseTimer, &QTimer::timeout, this, &MainWindow::reparse);
    connect(m_editor, &QPlainTextEdit::textChanged, this, [this] {
        m_reparseTimer->start();
        m_ask->setCurrentText(m_editor->toPlainText());
    });
    connect(m_editor->document(), &QTextDocument::modificationChanged, this, [this] { updateTitle(); });
    connect(m_runTimer, &QTimer::timeout, this, &MainWindow::stepRun);
    connect(m_flow, &FlowView::blockActivated, this, [this](int line) {
        m_tabs->setCurrentWidget(m_editor);
        m_editor->goToLine(line);
    });
    connect(m_ask, &AskPanel::modelReady, this, [this](const QString& text) {
        m_editor->replaceAll(text);
        m_tabs->setCurrentWidget(m_flow);   // the new model, seen as a model
        reparse();
    });
    connect(m_ask, &AskPanel::settingsRequested, this, &MainWindow::openSettings);

    m_ask->setConfig(des::LlmConfig::load());
    resize(1360, 860);
    if (!path.isEmpty()) openFile(path);
    reparse();
    updateTitle();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildMenusAndToolbar() {
    QStyle* st = style();
    auto* file = menuBar()->addMenu("&File");
    auto* newAct = file->addAction(st->standardIcon(QStyle::SP_FileIcon), "&New", this, &MainWindow::newModel);
    newAct->setShortcut(QKeySequence::New);
    auto* openAct = file->addAction(st->standardIcon(QStyle::SP_DialogOpenButton), "&Open...", this, &MainWindow::openDialog);
    openAct->setShortcut(QKeySequence::Open);
    auto* saveAct = file->addAction(st->standardIcon(QStyle::SP_DialogSaveButton), "&Save", this, [this] { save(); });
    saveAct->setShortcut(QKeySequence::Save);
    file->addAction("Save &As...", this, [this] { saveAs(); })->setShortcut(QKeySequence::SaveAs);
    file->addSeparator();
    file->addAction("&Quit", this, &QWidget::close)->setShortcut(QKeySequence::Quit);

    auto* edit = menuBar()->addMenu("&Edit");
    edit->addAction("&Undo", m_editor, &QPlainTextEdit::undo)->setShortcut(QKeySequence::Undo);
    edit->addAction("&Redo", m_editor, &QPlainTextEdit::redo)->setShortcut(QKeySequence::Redo);
    edit->addSeparator();
    edit->addAction("Cu&t", m_editor, &QPlainTextEdit::cut)->setShortcut(QKeySequence::Cut);
    edit->addAction("&Copy", m_editor, &QPlainTextEdit::copy)->setShortcut(QKeySequence::Copy);
    edit->addAction("&Paste", m_editor, &QPlainTextEdit::paste)->setShortcut(QKeySequence::Paste);

    auto* model = menuBar()->addMenu("&Model");
    model->addAction("Insert &starter model", this, [this] {
        m_editor->replaceAll(QString::fromStdString(des::starterModelText()));
    });
    m_runAction = model->addAction(st->standardIcon(QStyle::SP_MediaPlay), "&Run", this, &MainWindow::startRun);
    m_runAction->setShortcut(Qt::Key_F5);
    m_stopAction = model->addAction(st->standardIcon(QStyle::SP_MediaStop), "S&top", this, &MainWindow::stopRun);
    m_stopAction->setShortcut(Qt::SHIFT | Qt::Key_F5);
    m_stopAction->setEnabled(false);
    model->addSeparator();
    auto* describe = model->addAction(st->standardIcon(QStyle::SP_MessageBoxQuestion), "&Describe a model",
                                      this, [this] { m_ask->focusInput(); });
    describe->setShortcut(Qt::Key_F2);
    auto* talk = model->addAction("&Talk", this, [this] { m_ask->toggleTalk(); });
    talk->setShortcut(Qt::Key_F3);
    auto* settings = model->addAction(st->standardIcon(QStyle::SP_FileDialogDetailedView), "Se&ttings...",
                                      this, &MainWindow::openSettings);

    auto* tb = addToolBar("Main");
    tb->setMovable(false);
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    tb->setIconSize(QSize(18, 18));
    tb->addAction(newAct);
    tb->addAction(openAct);
    tb->addAction(saveAct);
    tb->addSeparator();
    tb->addAction(m_runAction);
    tb->addAction(m_stopAction);
    m_runPicker->setToolTip("Which [Run] record to run");
    m_runPicker->setMinimumWidth(120);
    tb->addWidget(m_runPicker);
    tb->addSeparator();
    tb->addAction(describe);
    tb->addAction(settings);
}

void MainWindow::buildDocks() {
    // Modules, grouped as Arena groups them. Double-click inserts the module's
    // template at the cursor; hovering shows what it is for.
    m_modules->setHeaderHidden(true);
    m_modules->setIndentation(14);
    QTreeWidgetItem* flow = new QTreeWidgetItem(m_modules, {"Flowchart"});
    QTreeWidgetItem* dataGroup = new QTreeWidgetItem(m_modules, {"Data"});
    QTreeWidgetItem* child = new QTreeWidgetItem(m_modules, {"Rows of a module"});
    for (const des::ModuleSchema& s : des::ModuleRegistry::instance().all()) {
        if (s.readOnly) continue;
        QTreeWidgetItem* parent = s.kind == des::ModuleKind::Flowchart ? flow
                                  : s.kind == des::ModuleKind::Data ? dataGroup : child;
        auto* item = new QTreeWidgetItem(parent, {QString::fromStdString(s.typeName)});
        item->setToolTip(0, QString::fromStdString(s.help));
        QPixmap swatch(10, 10);
        swatch.fill(theme::blockColor(QString::fromStdString(s.typeName)));
        item->setIcon(0, QIcon(swatch));
    }
    for (QTreeWidgetItem* g : {flow, dataGroup, child}) {
        QFont f = g->font(0);
        f.setBold(true);
        g->setFont(0, f);
        g->setFlags(Qt::ItemIsEnabled);
    }
    flow->setExpanded(true);
    dataGroup->setExpanded(true);
    connect(m_modules, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* it) {
        if (it->parent() != nullptr) insertModule(it->text(0));
    });
    auto* modDock = new QDockWidget("Modules", this);
    modDock->setWidget(m_modules);
    modDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, modDock);

    auto* askDock = new QDockWidget("Describe", this);
    askDock->setWidget(m_ask);
    askDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, askDock);

    m_problems->setHeaderLabels({"", "Line", "Problem"});
    m_problems->setRootIsDecorated(false);
    m_problems->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_problems->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_problems->header()->setStretchLastSection(true);
    connect(m_problems, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* it) {
        const int line = it->data(1, Qt::UserRole).toInt();
        if (line > 0) { m_tabs->setCurrentWidget(m_editor); m_editor->goToLine(line); }
    });
    auto* probDock = new QDockWidget("Problems", this);
    probDock->setWidget(m_problems);
    probDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::BottomDockWidgetArea, probDock);
    resizeDocks({modDock, askDock}, {190, 380}, Qt::Horizontal);
    resizeDocks({probDock}, {130}, Qt::Vertical);
}

void MainWindow::reparse() {
    const std::string text = m_editor->toPlainText().toStdString();
    des::ReadResult read = des::readDocument(text);
    std::vector<des::Diagnostic> diags = read.diagnostics;
    const des::CompileResult compiled = des::compile(read.document);
    diags.insert(diags.end(), compiled.diagnostics.begin(), compiled.diagnostics.end());
    m_document = std::move(read.document);

    QMap<int, ModelEditor::Mark> marks;
    m_problems->clear();
    int errors = 0, warnings = 0;
    for (const des::Diagnostic& d : diags) {
        const bool err = d.severity == des::Severity::Error;
        (err ? errors : warnings)++;
        const int line = static_cast<int>(des::lineOf(m_document, d));
        QString where;
        if (d.cell) where = QString::fromStdString(d.cell->moduleType + " " + std::to_string(d.cell->row + 1) +
                                                   (d.cell->column.empty() ? "" : ", " + d.cell->column) + ": ");
        const QString msg = where + QString::fromStdString(d.message);
        if (line > 0 && (!marks.contains(line) || err)) marks[line] = {err, msg};
        auto* item = new QTreeWidgetItem(m_problems, {err ? "Error" : "Warning", line > 0 ? QString::number(line) : "", msg});
        item->setForeground(0, err ? theme::error : theme::warning);
        item->setData(1, Qt::UserRole, line);
    }
    m_editor->setMarks(marks);
    m_flow->showModel(m_document);

    const bool empty = m_editor->toPlainText().trimmed().isEmpty();
    if (empty)              m_compileState->setText("An empty model.");
    else if (errors > 0)    m_compileState->setText(QString("<span style='color:%1'>&#9679;</span> %2 error%3 -- see Problems")
                                                        .arg(theme::error.name()).arg(errors).arg(errors == 1 ? "" : "s"));
    else                    m_compileState->setText(QString("<span style='color:%1'>&#9679;</span> The model compiles%2")
                                                        .arg(theme::success.name(),
                                                             warnings ? QString(" (%1 warning%2)").arg(warnings).arg(warnings == 1 ? "" : "s") : ""));
    m_runAction->setEnabled(!empty && errors == 0 && !m_run);

    const QString picked = m_runPicker->currentText();
    m_runPicker->clear();
    for (const std::string& n : des::runNames(m_document)) m_runPicker->addItem(QString::fromStdString(n));
    if (m_runPicker->count() == 0) m_runPicker->addItem("(default)");
    const int keep = m_runPicker->findText(picked);
    if (keep >= 0) m_runPicker->setCurrentIndex(keep);
}

void MainWindow::updateTitle() {
    const QString name = m_path.isEmpty() ? "untitled.des" : QFileInfo(m_path).fileName();
    setWindowTitle(name + (m_editor->document()->isModified() ? " *" : "") + " - DES Simulator");
}

bool MainWindow::openFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Open", "Could not open " + path + ":\n" + f.errorString());
        return false;
    }
    m_editor->setPlainText(QString::fromUtf8(f.readAll()));
    m_editor->document()->setModified(false);
    m_path = path;
    m_results->clear();
    reparse();
    updateTitle();
    return true;
}

bool MainWindow::maybeSave() {
    if (!m_editor->document()->isModified()) return true;
    const auto r = QMessageBox::question(this, "Unsaved changes", "Save the changes to this model first?",
                                         QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (r == QMessageBox::Cancel) return false;
    return r == QMessageBox::Discard || save();
}

bool MainWindow::save() {
    if (m_path.isEmpty()) return saveAs();
    // QSaveFile: written to a temporary file and renamed into place, so a
    // failure half way leaves the old model rather than half of the new one.
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly) || f.write(m_editor->toPlainText().toUtf8()) < 0 || !f.commit()) {
        QMessageBox::warning(this, "Save", "Could not save " + m_path + ":\n" + f.errorString());
        return false;
    }
    m_editor->document()->setModified(false);
    updateTitle();
    statusBar()->showMessage("Saved " + m_path, 3000);
    return true;
}

bool MainWindow::saveAs() {
    const QString p = QFileDialog::getSaveFileName(this, "Save model", m_path.isEmpty() ? "untitled.des" : m_path,
                                                   "Models (*.des)");
    if (p.isEmpty()) return false;
    m_path = p.endsWith(".des") ? p : p + ".des";
    return save();
}

void MainWindow::newModel() {
    if (!maybeSave()) return;
    m_editor->clear();
    m_editor->document()->setModified(false);
    m_path.clear();
    m_results->clear();
    reparse();
    updateTitle();
}

void MainWindow::openDialog() {
    if (!maybeSave()) return;
    const QString p = QFileDialog::getOpenFileName(this, "Open model", QFileInfo(m_path).absolutePath(), "Models (*.des)");
    if (!p.isEmpty()) openFile(p);
}

void MainWindow::insertModule(const QString& type) {
    const des::ModuleSchema* schema = des::ModuleRegistry::instance().find(type.toStdString());
    if (schema == nullptr) return;
    QString block;
    for (const std::string& l : des::templateFor(*schema)) block += QString::fromStdString(l) + "\n";
    QTextCursor c = m_editor->textCursor();
    c.movePosition(QTextCursor::EndOfBlock);
    c.insertText((m_editor->toPlainText().isEmpty() ? "" : "\n\n") + block.trimmed() + "\n");
    m_editor->setTextCursor(c);
    m_tabs->setCurrentWidget(m_editor);
    m_editor->setFocus();
}

void MainWindow::startRun() {
    if (m_run) return;
    reparse();
    std::vector<des::Diagnostic> problems;
    const std::size_t which = static_cast<std::size_t>(qMax(0, m_runPicker->currentIndex()));
    m_run = des::RunController::fromDocument(m_document, problems, std::nullopt, which);
    if (!m_run) {
        QStringList lines;
        for (const auto& d : problems) lines << QString::fromStdString(d.message);
        QMessageBox::warning(this, "Run", "The model cannot run:\n\n" + lines.join("\n"));
        return;
    }
    m_runName = m_runPicker->currentText();
    m_runAction->setEnabled(false);
    m_stopAction->setEnabled(true);
    m_progress->setValue(0);
    m_progress->show();
    m_runTimer->start(0);
}

void MainWindow::stepRun() {
    if (!m_run) return;
    // A slice of events per tick: enough to finish quickly, little enough that
    // the window stays responsive -- the shape RunController::advance exists for.
    m_run->advance(20000);
    const des::RunProgress p = m_run->progress();
    const double done = p.replications > 0
        ? ((p.replication > 0 ? p.replication - 1 : 0) + p.fraction.value_or(0.0)) / p.replications : 0.0;
    m_progress->setValue(static_cast<int>(done * 100));
    m_progress->setFormat(QString("Replication %1 of %2").arg(qMax(1, p.replication)).arg(p.replications));
    m_progress->setTextVisible(p.replications > 1);
    if (m_run->state() != des::RunState::Ready && m_run->state() != des::RunState::Running) finishRun();
}

void MainWindow::finishRun() {
    m_runTimer->stop();
    m_progress->hide();
    m_stopAction->setEnabled(false);
    std::unique_ptr<des::RunController> run = std::move(m_run);
    if (run->state() == des::RunState::Failed) {
        QMessageBox::warning(this, "Run", "The run failed:\n" + QString::fromStdString(run->failure()));
    } else {
        std::ostringstream report;
        run->report(report);
        std::vector<std::pair<QString, double>> bars;
        // Each station's utilisation, averaged over every run of the study --
        // the time-averaged figure. (A resource snapshot's `busy` is how many
        // were busy at the final instant, not over the run, so it is not used.)
        // A station on a SHARED resource is measured against that resource's
        // capacity, so the stations sharing one add up to its total.
        QStringList order;
        QMap<QString, std::pair<double, int>> sum;
        for (const des::RunSnapshot& snap : run->finalSnapshots())
            for (const des::BlockSnapshot& b : snap.blocks) {
                const QString n = QString::fromStdString(b.name);
                if (!sum.contains(n)) order << n;
                sum[n].first += b.utilisation;
                sum[n].second += 1;
            }
        QMap<QString, QString> sharedBy;   // station -> its shared resource
        for (std::size_t r = 0; r < m_document.rowCount("Process"); ++r) {
            const std::string res = m_document.cell("Process", r, "Resource");
            if (!res.empty())
                sharedBy[QString::fromStdString(m_document.cell("Process", r, "Name"))] = QString::fromStdString(res);
        }
        QStringList resources;
        QMap<QString, double> resourceTotal;
        for (const QString& n : order) {
            const double mean = sum[n].first / qMax(1, sum[n].second);
            if (mean <= 0) continue;
            const QString res = sharedBy.value(n);
            bars.push_back({res.isEmpty() ? n : n + "  (" + res + ")", mean});
            if (!res.isEmpty()) {
                if (!resourceTotal.contains(res)) resources << res;
                resourceTotal[res] += mean;
            }
        }
        for (const QString& r : resources) bars.push_back({r + ", all uses", resourceTotal[r]});
        const des::RunProgress p = run->progress();
        m_results->showResults(QString("Results: %1  -  %2 replication%3")
                                   .arg(m_runName).arg(p.replications).arg(p.replications == 1 ? "" : "s"),
                               QString::fromStdString(report.str()), std::move(bars));
        m_tabs->setCurrentWidget(m_results);
    }
    reparse();
}

void MainWindow::stopRun() {
    if (!m_run) return;
    m_run->cancel();
    finishRun();
    statusBar()->showMessage("Run stopped", 3000);
}

void MainWindow::runToEndForScreenshot() {
    startRun();
    while (m_run) stepRun();
}

void MainWindow::openSettings() {
    SettingsDialog dlg(m_ask->config(), this);
    if (dlg.exec() != QDialog::Accepted) return;
    QString error;
    if (!dlg.save(error)) QMessageBox::warning(this, "Settings", "Could not save the settings:\n" + error);
    m_ask->setConfig(dlg.result());
}

void MainWindow::showTab(const QString& name) {
    for (int i = 0; i < m_tabs->count(); ++i)
        if (m_tabs->tabText(i).compare(name, Qt::CaseInsensitive) == 0) m_tabs->setCurrentIndex(i);
}

void MainWindow::discardChanges() { m_editor->document()->setModified(false); }

void MainWindow::closeEvent(QCloseEvent* e) {
    if (maybeSave()) e->accept();
    else             e->ignore();
}
