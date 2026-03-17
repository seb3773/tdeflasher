#include "mainwindow.h"
#include <ntqapplication.h>
#include <tqt.h>

int main(int argc, char **argv) {
  TQApplication app(argc, argv);

  MainWindow win;
  app.setMainWidget(&win);
  win.show();

  return app.exec();
}
