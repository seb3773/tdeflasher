#ifndef URLINPUTDIALOG_H
#define URLINPUTDIALOG_H

#include <ntqdialog.h>
#include <ntqstring.h>

class TQLineEdit;
class TQPushButton;
class TQWidget;

class UrlInputDialog : public TQDialog {
  TQ_OBJECT

public:
  UrlInputDialog(TQWidget *parent = 0, const char *name = 0);
  ~UrlInputDialog();

  TQString url() const;
  TQString username() const;
  TQString password() const;

private slots:
  void toggleAuth();
  void validateInput();

private:
  TQLineEdit *m_txtUrl;
  TQWidget *m_authContainer;
  TQLineEdit *m_txtUsername;
  TQLineEdit *m_txtPassword;
  TQPushButton *m_btnAuthToggle;
  TQPushButton *m_btnOk;
  bool m_authVisible;
};

#endif
