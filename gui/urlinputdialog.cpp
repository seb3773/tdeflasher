#include "urlinputdialog.h"

#include <tqfont.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqlineedit.h>
#include <tqpushbutton.h>

UrlInputDialog::UrlInputDialog(TQWidget *parent, const char *name)
    : TQDialog(parent, name, true), m_authVisible(false) {

  setCaption("Use Image URL");
  setMinimumWidth(400);

  TQVBoxLayout *mainLayout = new TQVBoxLayout(this, 20, 15);

  TQLabel *lblTitle = new TQLabel("Use Image URL", this);
  TQFont f = lblTitle->font();
  f.setPointSize(14);
  lblTitle->setFont(f);
  mainLayout->addWidget(lblTitle);

  TQLabel *lblUrl = new TQLabel("Image URL:", this);
  mainLayout->addWidget(lblUrl);
  m_txtUrl = new TQLineEdit(this);
  mainLayout->addWidget(m_txtUrl);

  m_btnAuthToggle = new TQPushButton("> Authentication", this);
  connect(m_btnAuthToggle, SIGNAL(clicked()), this, SLOT(toggleAuth()));
  mainLayout->addWidget(m_btnAuthToggle);

  m_authContainer = new TQWidget(this);
  TQVBoxLayout *authLayout = new TQVBoxLayout(m_authContainer, 0, 10);

  TQLabel *lblUsername = new TQLabel("Username:", m_authContainer);
  authLayout->addWidget(lblUsername);
  m_txtUsername = new TQLineEdit(m_authContainer);
  authLayout->addWidget(m_txtUsername);

  TQLabel *lblPassword = new TQLabel("Password:", m_authContainer);
  authLayout->addWidget(lblPassword);
  m_txtPassword = new TQLineEdit(m_authContainer);
  m_txtPassword->setEchoMode(TQLineEdit::Password);
  authLayout->addWidget(m_txtPassword);

  m_authContainer->hide();
  mainLayout->addWidget(m_authContainer);

  mainLayout->addStretch(1);

  TQHBoxLayout *btnLayout = new TQHBoxLayout(mainLayout);
  btnLayout->addStretch(1);

  TQPushButton *btnCancel = new TQPushButton("Cancel", this);
  btnCancel->setFixedWidth(100);
  connect(btnCancel, SIGNAL(clicked()), this, SLOT(reject()));
  btnLayout->addWidget(btnCancel);

  m_btnOk = new TQPushButton("OK", this);
  m_btnOk->setFixedWidth(100);
  m_btnOk->setEnabled(false);
  connect(m_btnOk, SIGNAL(clicked()), this, SLOT(accept()));
  btnLayout->addWidget(m_btnOk);

  connect(m_txtUrl, SIGNAL(textChanged(const TQString &)), this,
          SLOT(validateInput()));
}

UrlInputDialog::~UrlInputDialog() {}

TQString UrlInputDialog::url() const { return m_txtUrl->text(); }

TQString UrlInputDialog::username() const { return m_txtUsername->text(); }

TQString UrlInputDialog::password() const { return m_txtPassword->text(); }

void UrlInputDialog::toggleAuth() {
  m_authVisible = !m_authVisible;
  if (m_authVisible) {
    m_authContainer->show();
  } else {
    m_authContainer->hide();
  }
  if (m_authVisible) {
    m_btnAuthToggle->setText("v Authentication");
  } else {
    m_btnAuthToggle->setText("> Authentication");
    m_txtUsername->setText("");
    m_txtPassword->setText("");
  }
  adjustSize();
}

void UrlInputDialog::validateInput() {
  m_btnOk->setEnabled(!m_txtUrl->text().isEmpty());
}
