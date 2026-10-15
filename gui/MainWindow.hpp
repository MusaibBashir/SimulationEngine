// ============================================================================
// gui/MainWindow.hpp  --  the DES Simulator desktop window
// ============================================================================
// Modules on the left, the model in the middle (text, flow, results), the
// Describe panel on the right, problems along the bottom. Every view is
// derived from the text after each edit -- the same rule as the terminal UI,
// and for the same reason: what is saved is exactly what was typed.

#pragma once
#include <QMainWindow>
#include <memory>
#include "ModelDocument.hpp"
#include "RunController.hpp"

class AskPanel;
class FlowView;
class ModelEditor;
class QAction;
class QComboBox;
class QLabel;
class QProgressBar;
class QTabWidget;
class QTimer;
class QTreeWidget;
class ResultsView;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString& path = QString(), QWidget* parent = nullptr);
    ~MainWindow() override;

    bool openFile(const QString& path);
    // For screenshots and tests: which tab is showing.
    void showTab(const QString& name);
    void runToEndForScreenshot();
    AskPanel* askPanel() const { return m_ask; }
    // Close without asking about unsaved changes -- for --screenshot, where a
    // "Save first?" dialog would wait for ever for a click nobody can make.
    void discardChanges();

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    ModelEditor* m_editor;
    FlowView*    m_flow;
    ResultsView* m_results;
    QTabWidget*  m_tabs;
    QTreeWidget* m_modules;
    QTreeWidget* m_problems;
    AskPanel*    m_ask;
    QComboBox*   m_runPicker;
    QLabel*      m_compileState;
    QProgressBar* m_progress;
    QTimer*      m_reparseTimer;
    QTimer*      m_runTimer;
    QAction*     m_runAction;
    QAction*     m_stopAction;

    QString                m_path;
    des::ModelDocument     m_document;
    std::unique_ptr<des::RunController> m_run;
    QString                m_runName;

    void buildMenusAndToolbar();
    void buildDocks();
    void reparse();
    void updateTitle();
    bool maybeSave();
    bool save();
    bool saveAs();
    void newModel();
    void openDialog();
    void insertModule(const QString& type);
    void startRun();
    void stopRun();
    void stepRun();
    void finishRun();
    void openSettings();
};
