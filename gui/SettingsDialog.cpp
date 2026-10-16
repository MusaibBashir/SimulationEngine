#include "SettingsDialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include "Theme.hpp"

SettingsDialog::SettingsDialog(const des::LlmConfig& current, QWidget* parent)
    : QDialog(parent), m_before(current), m_key(new QLineEdit(this)), m_model(new QComboBox(this)),
      m_server(new QLineEdit(this)), m_effort(new QComboBox(this)), m_stt(new QComboBox(this)),
      m_language(new QLineEdit(this)) {
    setWindowTitle("Settings");
    setMinimumWidth(520);
    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel("Describing a model sends your words to a language model. A free Groq key works: "
                             "<a href='https://console.groq.com/keys'>console.groq.com/keys</a>.", this);
    intro->setOpenExternalLinks(true);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* form = new QFormLayout;
    m_key->setEchoMode(QLineEdit::Password);
    m_key->setText(QString::fromStdString(current.apiKey));
    m_key->setPlaceholderText("gsk_...");
    form->addRow("API key", m_key);
    m_model->setEditable(true);
    m_model->addItems({"qwen/qwen3.8-27b", "llama-3.1-8b-instant"});
    m_model->setCurrentText(QString::fromStdString(current.model));
    form->addRow("Model", m_model);
    m_server->setText(QString::fromStdString(current.baseUrl));
    form->addRow("Server", m_server);
    m_effort->addItems({"", "none", "low", "medium", "high"});
    m_effort->setCurrentText(QString::fromStdString(current.reasoningEffort));
    m_effort->setToolTip("For a reasoning model like Qwen 3: blank is the provider's default, none is fastest.");
    form->addRow("Reasoning effort", m_effort);
    m_stt->setEditable(true);
    m_stt->addItems({"whisper-large-v3-turbo", "whisper-large-v3"});
    m_stt->setCurrentText(QString::fromStdString(current.sttModel));
    form->addRow("Speech model", m_stt);
    m_language->setText(QString::fromStdString(current.sttLanguage));
    m_language->setPlaceholderText("blank: detect; or en, hi, ...");
    form->addRow("Speech language", m_language);
    layout->addLayout(form);

    auto* where = new QLabel("Saved for you alone in " + QString::fromStdString(des::llmConfigPath()), this);
    where->setStyleSheet(QString("color: %1;").arg(theme::muted.name()));
    where->setWordWrap(true);
    where->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(where);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setObjectName("primary");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

des::LlmConfig SettingsDialog::result() const {
    des::LlmConfig c = m_before;
    c.apiKey = m_key->text().trimmed().toStdString();
    c.model = m_model->currentText().trimmed().toStdString();
    c.baseUrl = m_server->text().trimmed().toStdString();
    while (!c.baseUrl.empty() && c.baseUrl.back() == '/') c.baseUrl.pop_back();
    c.reasoningEffort = m_effort->currentText().toStdString();
    c.sttModel = m_stt->currentText().trimmed().toStdString();
    c.sttLanguage = m_language->text().trimmed().toStdString();
    return c;
}

bool SettingsDialog::save(QString& error) {
    const des::LlmConfig c = result();
    const std::pair<const char*, std::pair<std::string, std::string>> fields[] = {
        {"api_key", {m_before.apiKey, c.apiKey}},
        {"model", {m_before.model, c.model}},
        {"base_url", {m_before.baseUrl, c.baseUrl}},
        {"reasoning_effort", {m_before.reasoningEffort, c.reasoningEffort}},
        {"stt_model", {m_before.sttModel, c.sttModel}},
        {"stt_language", {m_before.sttLanguage, c.sttLanguage}},
    };
    for (const auto& f : fields) {
        if (f.second.first == f.second.second) continue;
        std::string why;
        if (!des::saveLlmSetting(f.first, f.second.second, why)) {
            error = QString::fromStdString(why);
            return false;
        }
    }
    return true;
}
