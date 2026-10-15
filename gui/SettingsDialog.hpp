// ============================================================================
// gui/SettingsDialog.hpp  --  the key, the model, the server
// ============================================================================
// A face for llm.conf, the same per-user file the terminal and the CLI read.
// Only what was changed is written, one line at a time, so a setting this
// dialog does not know about survives it.

#pragma once
#include <QDialog>
#include "ModelGenerator.hpp"

class QComboBox;
class QLineEdit;

class SettingsDialog : public QDialog {
public:
    explicit SettingsDialog(const des::LlmConfig& current, QWidget* parent = nullptr);
    // Writes what changed; false, with the reason, when the file could not be written.
    bool save(QString& error);
    des::LlmConfig result() const;

private:
    des::LlmConfig m_before;
    QLineEdit* m_key;
    QComboBox* m_model;
    QLineEdit* m_server;
    QComboBox* m_effort;
    QComboBox* m_stt;
    QLineEdit* m_language;
};
