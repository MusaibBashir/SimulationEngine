// ============================================================================
// gui/AskPanel.hpp  --  describe a model, in words or out loud
// ============================================================================
// The desktop face of the terminal's F2: the conversation above, a box below,
// Talk and Send. Everything that decides anything -- the pre-parser, the form,
// the questions, the builder -- is des_ai's, shared with the terminal and the
// CLI; this only shows it. Requests run on a worker thread and come back to
// the UI thread queued, so the window never stops drawing.

#pragma once
#include <QWidget>
#include <memory>
#include "Json.hpp"
#include "ModelGenerator.hpp"
#include "Recorder.hpp"

class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTextBrowser;
class QTimer;

class AskPanel : public QWidget {
    Q_OBJECT
public:
    explicit AskPanel(QWidget* parent = nullptr);

    void setConfig(const des::LlmConfig& c);
    const des::LlmConfig& config() const { return m_config; }
    // The editor's text now, to tell whether it was edited since the last answer.
    void setCurrentText(const QString& text) { m_current = text; }
    void focusInput();
    void toggleTalk();
    // Puts `text` in the box and sends it -- for --describe and for tests.
    void describe(const QString& text);
    // Something went wrong or a question came back: the conversation, as text.
    QString conversation() const;

Q_SIGNALS:
    void modelReady(const QString& text);
    void settingsRequested();

private:
    QTextBrowser*   m_log;
    QPlainTextEdit* m_input;
    QPushButton*    m_talk;
    QPushButton*    m_send;
    QLabel*         m_state;
    QProgressBar*   m_level;
    QTimer*         m_tick;

    des::LlmConfig  m_config;
    des::JsonValue  m_scenario;
    QString         m_builtText;
    QString         m_current;
    bool            m_busy{false};
    std::unique_ptr<des::IRecorder> m_recorder;
    std::shared_ptr<int> m_generation;   // bumped to abandon a request in flight

    void append(const QString& kind, const QString& text);
    void setBusy(bool busy, const QString& what = QString());
    bool ensureKey();
    void send();
    void startTalking();
    void stopTalking();
    void onResult(const des::GenerateResult& r);
};
