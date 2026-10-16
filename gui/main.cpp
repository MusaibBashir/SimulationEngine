// ============================================================================
// gui/main.cpp  --  DES Simulator, the desktop app
// ============================================================================
//   DES-Simulator [model.des]
//   DES-Simulator --screenshot out.png [--tab model|flow|results] [--run] [model.des]
//   DES-Simulator --screenshot out.png --describe "customers arrive ..."
//
// --screenshot draws the window off screen and saves it, for the README and
// for checking a change to the layout without a display.

#include <QApplication>
#include <QCommandLineParser>
#include <QScreen>
#include <QTimer>
#include "AskPanel.hpp"
#include "MainWindow.hpp"
#include "Theme.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("DES Simulator");
    QApplication::setOrganizationName("DES Simulator");
    theme::apply(app);

    QCommandLineParser args;
    args.addHelpOption();
    args.addPositionalArgument("model", "A .des model to open.");
    QCommandLineOption shot("screenshot", "Save a picture of the window and exit.", "png");
    QCommandLineOption tab("tab", "With --screenshot: model, flow or results.", "name", "model");
    QCommandLineOption run("run", "With --screenshot: run the model first.");
    QCommandLineOption describe("describe", "With --screenshot: describe a model, wait for it, then draw.", "words");
    QCommandLineOption set("set", "With --screenshot: change a Run setup field first, e.g. \"Replications=5\".",
                           "field=value");
    args.addOptions({shot, tab, run, describe, set});
    args.process(app);

    const QStringList files = args.positionalArguments();
    MainWindow w(files.isEmpty() ? QString() : files.first());
    if (args.isSet(shot)) {
        w.resize(1360, 860);
        w.show();
        bool taken = false;
        const auto snap = [&] {
            if (taken) return;   // the answer, the poll and the grace timer may all ask
            taken = true;
            for (const QString& f : args.values(set))
                w.setRunField(f.section('=', 0, 0).trimmed(), f.section('=', 1).trimmed());
            if (args.isSet(run)) w.runToEndForScreenshot();
            if (args.isSet(tab)) w.showTab(args.value(tab));
            QTimer::singleShot(300, &app, [&] {
                w.grab().save(args.value(shot));
                w.discardChanges();
                app.quit();
            });
        };
        if (args.isSet(describe)) {
            // Draw once the answer is in -- a model, a question or an error --
            // with a minute's grace for a slow provider.
            QObject::connect(w.askPanel(), &AskPanel::modelReady, &app, [&] { QTimer::singleShot(500, &app, snap); });
            QTimer::singleShot(60000, &app, snap);
            QTimer::singleShot(0, &app, [&] { w.askPanel()->describe(args.value(describe)); });
            QTimer* poll = new QTimer(&app);
            QObject::connect(poll, &QTimer::timeout, &app, [&, poll] {
                const QString c = w.askPanel()->conversation();
                if (c.contains("\n? ") || c.contains("\n! ") || c.contains("\n!")) { poll->stop(); snap(); }
            });
            poll->start(200);
        } else {
            snap();
        }
        return app.exec();
    }
    w.show();
    return app.exec();
}
