#include "AskPanel.hpp"

#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>
#include <thread>
#include "CurlTransport.hpp"
#include "DocumentFormat.hpp"
#include "Speech.hpp"
#include "Theme.hpp"

namespace {

// Enter sends; Shift+Enter is a new line. What every chat box does.
class InputBox : public QPlainTextEdit {
public:
    std::function<void()> onEnter;
    using QPlainTextEdit::QPlainTextEdit;

protected:
    void keyPressEvent(QKeyEvent* e) override {
        if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && !(e->modifiers() & Qt::ShiftModifier)) {
            if (onEnter) onEnter();
            return;
        }
        QPlainTextEdit::keyPressEvent(e);
    }
};

}  // namespace

AskPanel::AskPanel(QWidget* parent)
    : QWidget(parent), m_log(new QTextBrowser(this)), m_input(new InputBox(this)),
      m_talk(new QPushButton("Talk", this)), m_send(new QPushButton("Send", this)),
      m_state(new QLabel(this)), m_level(new QProgressBar(this)), m_tick(new QTimer(this)),
      m_generation(std::make_shared<int>(0)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 10);
    layout->setSpacing(8);
    m_log->setOpenExternalLinks(true);
    layout->addWidget(m_log, 1);
    m_state->setStyleSheet(QString("color: %1;").arg(theme::muted.name()));
    m_state->setWordWrap(true);
    layout->addWidget(m_state);
    m_level->setRange(0, 100);
    m_level->setTextVisible(false);
    m_level->setFixedHeight(8);
    m_level->hide();
    layout->addWidget(m_level);
    m_input->setPlaceholderText("Say or type what happens -- e.g. \"customers arrive thirty an hour, "
                                "two tellers, about 3 minutes each\"");
    m_input->setFixedHeight(84);
    layout->addWidget(m_input);
    auto* row = new QHBoxLayout;
    auto* settings = new QPushButton("Settings", this);
    m_talk->setObjectName("record");
    m_talk->setCheckable(true);
    m_talk->setToolTip("Record what you say (F3). Click again to stop; the words appear in the box to check.");
    m_send->setObjectName("primary");
    m_send->setToolTip("Build the model from these words (Enter)");
    row->addWidget(settings);
    row->addStretch(1);
    row->addWidget(m_talk);
    row->addWidget(m_send);
    layout->addLayout(row);

    static_cast<InputBox*>(m_input)->onEnter = [this] { send(); };
    connect(m_send, &QPushButton::clicked, this, &AskPanel::send);
    connect(m_talk, &QPushButton::clicked, this, &AskPanel::toggleTalk);
    connect(settings, &QPushButton::clicked, this, &AskPanel::settingsRequested);
    connect(m_tick, &QTimer::timeout, this, [this] {
        if (!m_recorder) return;
        m_level->setValue(static_cast<int>(qMin(1.0f, m_recorder->level() * 1.5f) * 100));
        const int secs = static_cast<int>(m_recorder->seconds());
        m_state->setText(QString("Recording %1:%2 -- speak, then click Talk again to stop.")
                             .arg(secs / 60).arg(secs % 60, 2, 10, QChar('0')));
        if (m_recorder->seconds() >= 120.0) stopTalking();
    });

    append("info", "Describe what happens -- what arrives and how often, what is done to it and how long "
                   "it takes, how many servers. Or click <b>Talk</b> and say it.");
}

void AskPanel::setConfig(const des::LlmConfig& c) {
    m_config = c;
    m_state->setText(m_config.needsKey() ? "No API key yet -- open Settings to add one (free at console.groq.com/keys)."
                                         : QString("Sent to %1 (%2).")
                                               .arg(QString::fromStdString(m_config.host()),
                                                    QString::fromStdString(m_config.model)));
}

void AskPanel::focusInput() { m_input->setFocus(); }

void AskPanel::describe(const QString& text) {
    m_input->setPlainText(text);
    send();
}

QString AskPanel::conversation() const { return m_log->toPlainText(); }

void AskPanel::append(const QString& kind, const QString& text) {
    QString color = theme::text.name(), prefix;
    if (kind == "you")        { prefix = "<b>You</b> "; }
    else if (kind == "understood") { color = theme::muted.name(); prefix = "Understood: "; }
    else if (kind == "assumed")    { color = theme::text.name(); prefix = "<span style='color:#B45309'>Assumed:</span> "; }
    else if (kind == "question")   { color = theme::accent.name(); prefix = "<b>?</b> "; }
    else if (kind == "error")      { color = theme::error.name(); prefix = "<b>!</b> "; }
    else if (kind == "info")       { color = theme::muted.name(); }
    const QString body = (kind == "info" ? text : text.toHtmlEscaped());
    m_log->append(QString("<div style='color:%1; margin:4px 0'>%2%3</div>").arg(color, prefix, body));
}

void AskPanel::setBusy(bool busy, const QString& what) {
    m_busy = busy;
    m_send->setEnabled(!busy);
    m_talk->setEnabled(!busy || m_recorder != nullptr);
    if (!what.isEmpty()) m_state->setText(what);
}

bool AskPanel::ensureKey() {
    if (!m_config.needsKey()) return true;
    append("error", QString::fromStdString(m_config.host()) + " needs an API key. Open Settings to add it.");
    Q_EMIT settingsRequested();
    return !m_config.needsKey();
}

void AskPanel::send() {
    if (m_busy) return;
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty()) return;
    if (!ensureKey()) return;
    append("you", text);
    m_input->clear();

    des::JsonValue previous;
    if (m_scenario.isObject()) {
        previous = m_scenario;
        if (!m_builtText.isEmpty() && m_current != m_builtText)
            append("info", "The model was edited by hand since the last answer; those edits will be replaced "
                           "(Undo brings them back).");
    }
    setBusy(true, QString("Asking %1 ...").arg(QString::fromStdString(m_config.model)));
    const int ticket = ++*m_generation;
    const auto generation = m_generation;
    QPointer<AskPanel> self(this);
    const des::LlmConfig cfg = m_config;
    const std::string description = text.toStdString();
    std::thread([self, generation, ticket, cfg, description, previous] {
        des::CurlTransport transport;
        des::GenerateResult r = des::generateModel(description, previous, cfg, transport);
        QMetaObject::invokeMethod(qApp, [self, generation, ticket, r] {
            if (!self || *generation != ticket) return;   // abandoned, or the panel is gone
            self->onResult(r);
        }, Qt::QueuedConnection);
    }).detach();
}

void AskPanel::onResult(const des::GenerateResult& r) {
    setBusy(false, QString("Sent to %1 (%2).").arg(QString::fromStdString(m_config.host()),
                                                   QString::fromStdString(m_config.model)));
    if (!r.error.empty()) { append("error", QString::fromStdString(r.error)); return; }
    if (r.scenarioJson.isObject()) m_scenario = r.scenarioJson;
    for (const auto& n : r.notes) append("understood", QString::fromStdString(n));
    if (!r.ok) {
        for (const auto& q : r.questions) append("question", QString::fromStdString(q.text));
        m_state->setText("Answer in the box below -- a number is enough for a server count.");
        focusInput();
        return;
    }
    for (const auto& a : r.assumptions) append("assumed", QString::fromStdString(a));
    m_builtText = QString::fromStdString(des::writeDocument(r.document));
    append("info", "Model written into the editor. <b>Undo</b> takes it back, <b>Run</b> runs it. "
                   "Type a change to make, like \"add a second teller\".");
    Q_EMIT modelReady(m_builtText);
}

void AskPanel::toggleTalk() {
    if (m_recorder) stopTalking();
    else            startTalking();
}

void AskPanel::startTalking() {
    m_talk->setChecked(false);
    if (m_busy || !ensureKey()) return;
    std::string why;
    std::unique_ptr<des::IRecorder> mic = des::openMicrophone(why);
    if (!mic || !mic->start(why)) {
        append("error", QString::fromStdString(why.empty() ? "No microphone could be opened." : why));
        return;
    }
    m_recorder = std::move(mic);
    m_talk->setChecked(true);
    m_talk->setText("Stop");
    m_level->show();
    m_tick->start(50);
    setBusy(true, "Recording -- speak, then click Stop.");
}

void AskPanel::stopTalking() {
    if (!m_recorder) return;
    m_tick->stop();
    m_level->hide();
    m_talk->setChecked(false);
    m_talk->setText("Talk");
    const std::vector<std::int16_t> samples = m_recorder->stop();
    m_recorder.reset();
    if (samples.size() < static_cast<std::size_t>(des::kSpeechSampleRate / 2)) {
        setBusy(false, "That recording was too short to send.");
        return;
    }
    setBusy(true, QString("Turning speech into text with %1 ...").arg(QString::fromStdString(m_config.sttModel)));
    const std::string wav = des::wavFromPcm16(samples, des::kSpeechSampleRate);
    const int ticket = ++*m_generation;
    const auto generation = m_generation;
    QPointer<AskPanel> self(this);
    const des::LlmConfig cfg = m_config;
    std::thread([self, generation, ticket, cfg, wav] {
        des::CurlTransport transport(120);
        des::TranscriptResult t = des::transcribe(wav, cfg, transport);
        QMetaObject::invokeMethod(qApp, [self, generation, ticket, t] {
            if (!self || *generation != ticket) return;
            self->setBusy(false, "Check what was heard, fix any numbers, then press Enter.");
            if (!t.ok) { self->append("error", QString::fromStdString(t.error)); return; }
            if (t.text.empty()) { self->m_state->setText("Nothing was heard. Try Talk again."); return; }
            QString now = self->m_input->toPlainText();
            if (!now.isEmpty() && !now.endsWith(' ')) now += ' ';
            self->m_input->setPlainText(now + QString::fromStdString(t.text));
            self->m_input->moveCursor(QTextCursor::End);
            self->focusInput();
        }, Qt::QueuedConnection);
    }).detach();
}
