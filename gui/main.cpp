#include "mainwindow.h"

#ifdef USE_TDE
#include <tdeapplication.h>
#include <tdeaboutdata.h>
#include <tdecmdlineargs.h>
#include <tdelocale.h>
#else
#include <ntqapplication.h>
#include <tqt.h>
#endif

int main(int argc, char **argv) {
#ifdef USE_TDE
  TDEAboutData aboutData("tdeflasher", I18N_NOOP("tdeFlasher"), "1.0.0",
                         I18N_NOOP("OS Image Flasher for Trinity Desktop"),
                         TDEAboutData::License_GPL);
  TDECmdLineArgs::init(argc, argv, &aboutData);
  TDEApplication app;
#else
  TQApplication app(argc, argv);
#endif

  MainWindow win;
  app.setMainWidget(&win);
  win.show();

  return app.exec();
}
