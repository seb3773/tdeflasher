#include "mainwindow.h"
#include "icons_data.h"
#include "urlinputdialog.h"
#include <curl/curl.h>
#include <ntqapplication.h>
#include <ntqdesktopwidget.h>
#include <ntqdialog.h>
#include <ntqfiledialog.h>
#include <ntqfileinfo.h>
#include <ntqimage.h>
#include <ntqlabel.h>
#include <ntqlayout.h>
#include <ntqlineedit.h>
#include <ntqlistview.h>
#include <ntqmessagebox.h>
#include <ntqmime.h>
#include <ntqprogressbar.h>
#include <ntqpushbutton.h>
#include <ntqtextedit.h>
#include <ntqtimer.h>
#include <tqevent.h>
#include <tqt.h>

#include "diskscanner.h"
#include "flasher.h"

#include <cstring>
#include <pthread.h>

// ---- Colors ----
static const char *BG_COLOR = "#4d5057";
static const char *BTN_ACTIVE_COLOR = "#00aeef";
static const char *BTN_INACTIVE_COLOR = "#3a3c41";
static const char *BTN_TEXT_COLOR = "#ffffff";
static const char *LINE_COLOR = "#6b6d73";
static const char *TEXT_MUTED = "#9b9b9b";
static const char *TEXT_LINK = "#2297de";
static const char *COLOR_WARNING = "#fabd2f";
static const char *COLOR_SYSTEM_BADGE = "#f8d7da";
static const char *COLOR_SOURCE_BADGE = "#d1ecf1";
static const char *COLOR_PROGRESS_PURPLE = "#da60ff";
static const char *COLOR_SIDEBAR_BG = "#4d5057";

// ---- Worker thread ----
struct FlashWorkerData {
  std::string imagePath;
  std::string devicePath;
  std::string urlUsername;
  std::string urlPassword;
  bool verify;
  volatile int state;
  volatile uint64_t bytesProcessed;
  volatile uint64_t totalBytes;
  volatile double speed;
  volatile int percentDone;
  volatile bool finished;
  volatile bool cancelled;
  char errorMsg[512];
  pthread_t thread;
};

static FlashWorkerData *g_worker = nullptr;

static void *flashThreadFunc(void *arg) {
  FlashWorkerData *w = (FlashWorkerData *)arg;
  flasher::flashImage(
      w->imagePath, w->devicePath, w->verify, &w->cancelled,
      [w](const flasher::FlashProgress &p) {
        w->state = p.state;
        w->bytesProcessed = p.bytesProcessed;
        w->totalBytes = p.totalBytes;
        w->speed = p.speed;
        w->percentDone = p.percentDone;
        if (!p.errorMsg.empty()) {
          strncpy(w->errorMsg, p.errorMsg.c_str(), sizeof(w->errorMsg) - 1);
          w->errorMsg[sizeof(w->errorMsg) - 1] = '\0';
        }
      },
      w->urlUsername, w->urlPassword);
  w->finished = true;
  return nullptr;
}

// ---- Custom rounded button (Etcher-style pill shape) ----
#include <ntqcursor.h>
#include <ntqpainter.h>

// ---- CustomProgressBar Implementation ----

CustomProgressBar::CustomProgressBar(TQWidget *parent)
    : TQWidget(parent), m_percent(0) {
  setFixedHeight(12);
}

void CustomProgressBar::setProgress(int percent) {
  if (percent < 0)
    percent = 0;
  if (percent > 100)
    percent = 100;
  if (m_percent != percent) {
    m_percent = percent;
    repaint();
  }
}

void CustomProgressBar::paintEvent(TQPaintEvent *) {
  TQPainter p(this);
  p.setPen(TQt::NoPen);

  // Clear background with parent's color
  if (parentWidget()) {
    p.fillRect(rect(), parentWidget()->paletteBackgroundColor());
  }

  int r = height(); // Full rounding

  // Draw background trough (dark gray #3a3c41)
  p.setBrush(TQColor(BTN_INACTIVE_COLOR));
  int roundXbg = r * 100 / width();
  p.drawRoundRect(0, 0, width(), height(), roundXbg, 100);

  // Draw filled area
  if (m_percent > 0) {
    int pw = width() * m_percent / 100;
    if (pw < r)
      pw = r;
    p.setBrush(TQColor(COLOR_PROGRESS_PURPLE));
    int roundXfg = r * 100 / pw;
    p.drawRoundRect(0, 0, pw, height(), roundXfg, 100);
  }
}

// ---- RoundButton Implementation ----

RoundButton::RoundButton(const TQString &text, const TQColor &bg,
                         TQWidget *parent)
    : TQWidget(parent), m_text(text), m_bgColor(bg), m_hover(false),
      m_enabled(true), m_flat(false), m_textOnly(false),
      m_textColor(TQColor(BTN_TEXT_COLOR)),
      m_hoverTextColor(TQColor("#ffffff")), m_callback(nullptr),
      m_callbackData(nullptr) {
  setMouseTracking(true);
}

void RoundButton::mousePressEvent(TQMouseEvent *) {
  if (m_enabled)
    repaint();
}

void RoundButton::mouseReleaseEvent(TQMouseEvent *e) {
  if (m_enabled && rect().contains(e->pos())) {
    if (m_callback) {
      m_callback(m_callbackData);
    }
  }
}

void RoundButton::paintEvent(TQPaintEvent *) {
  TQPainter p(this);

  // Clear background with parent's color
  if (parentWidget()) {
    p.fillRect(rect(), parentWidget()->paletteBackgroundColor());
  }

  TQColor bg = m_bgColor;
  bool shouldBeBlue = (m_hover && m_enabled);
  if (!m_flat && m_enabled)
    shouldBeBlue = true;

  if (m_textOnly)
    shouldBeBlue = false;

  if (!m_enabled) {
    bg = TQColor(BTN_INACTIVE_COLOR);
  } else if (shouldBeBlue) {
    if (m_hover) {
      bg = TQColor("#0090c0"); // Slightly darker blue
    } else {
      bg = TQColor(BTN_ACTIVE_COLOR);
    }
  }

  // Draw pill background
  if (!m_textOnly && (!m_flat || shouldBeBlue)) {
    p.setPen(TQt::NoPen);
    p.setBrush(bg);
    int roundingX = height() * 100 / width();
    p.drawRoundRect(0, 0, width(), height(), roundingX, 100);
  }

  TQColor penColor = m_textColor;
  if (shouldBeBlue) {
    penColor = m_hover ? TQColor("#d0d0d0") : TQColor("#ffffff");
  } else if (m_hover && m_enabled) {
    penColor = m_hoverTextColor;
  } else if (!m_enabled) {
    penColor = TQColor(TEXT_MUTED);
  }

  p.setPen(penColor);
  p.setFont(font());

  int iconW = m_icon.isNull() ? 0 : m_icon.width();
  int iconH = m_icon.isNull() ? 0 : m_icon.height();
  int spacing = m_icon.isNull() ? 0 : 2;
  TQFontMetrics fm(font());
  int textW = fm.width(m_text);
  int totalW = iconW + spacing + textW;
  int startX = (width() - totalW) / 2;

  if (!m_icon.isNull()) {
    p.drawPixmap(startX, (height() - iconH) / 2, m_icon);
  }
  p.drawText(startX + iconW + spacing, 0, width() - startX - iconW - spacing,
             height(), TQt::AlignVCenter, m_text);
}

// Helper callback wrappers
static void onFlashFile(void *w) {
  static_cast<MainWindow *>(w)->slotFlashFromFile();
}
static void onFlashUrl(void *w) {
  static_cast<MainWindow *>(w)->slotFlashFromUrl();
}
static void onCloneDrive(void *w) {
  static_cast<MainWindow *>(w)->slotCloneDrive();
}
static void onSelectTarget(void *w) {
  static_cast<MainWindow *>(w)->slotSelectTarget();
}
static void onFlash(void *w) { static_cast<MainWindow *>(w)->slotFlash(); }
static void onCancelFlash(void *w) {
  static_cast<MainWindow *>(w)->slotCancelFlash();
}
static void onRemoveImage(void *w) {
  static_cast<MainWindow *>(w)->slotRemoveImage();
}
static void onFlashAnother(void *w) {
  static_cast<MainWindow *>(w)->slotFlashAnother();
}
static void onShowImageInfo(void *w) {
  static_cast<MainWindow *>(w)->slotShowImageInfo();
}
static void onHideImageInfo(void *w) {
  static_cast<MainWindow *>(w)->slotHideImageInfo();
}
static void onShowAboutDialog(void *w) {
  static_cast<MainWindow *>(w)->slotShowAboutDialog();
}
static void onUrlOk(void *w) { static_cast<MainWindow *>(w)->slotUrlOk(); }
static void onUrlCancel(void *w) {
  static_cast<MainWindow *>(w)->slotUrlCancel();
}

// ---- Convert pixmap to grayed version ----
static TQPixmap grayPixmap(const TQPixmap &src) {
  TQImage img = src.convertToImage();
  for (int y = 0; y < img.height(); y++) {
    for (int x = 0; x < img.width(); x++) {
      TQRgb px = img.pixel(x, y);
      int gray = tqGray(px);
      int alpha = tqAlpha(px);
      // Make it darker and more muted
      gray = gray / 2 + 40;
      img.setPixel(x, y, tqRgba(gray, gray, gray, alpha));
    }
  }
  TQPixmap result;
  result.convertFromImage(img);
  return result;
}

// ---- MainWindow ----
MainWindow::MainWindow(TQWidget *parent, const char *name)
    : TQMainWindow(parent, name), currentStep(1), flashing(false),
      m_urlAuthVisible(false), m_selectingSourceDrive(false) {

  // Proportional size (~46% width, ~49% height of screen, like Etcher)
  TQDesktopWidget *dw = TQApplication::desktop();
  int winW = dw->width() * 46 / 100;
  int winH = dw->height() * 49 / 100;
  if (winW < 800)
    winW = 800;
  if (winH < 480)
    winH = 480;
  setFixedSize(winW, winH);
  setCaption("TDE Flasher");

  m_showHiddenDrives = false;
  m_selectedDriveCount = 0;

  // Dark background for entire window
  setPaletteBackgroundColor(TQColor(BG_COLOR));
  setPaletteForegroundColor(TQColor(BTN_TEXT_COLOR));

  loadEmbeddedIcons();

  // Set application icon (taskbar + window)
  setIcon(iconLogo);

  // ========== WARNING BANNER ==========
  warningBanner = new TQWidget(this);
  warningBanner->setGeometry(0, 0, winW, 40);
  warningBanner->setPaletteBackgroundColor(TQColor(COLOR_WARNING));
  warningBanner->hide();

  TQHBoxLayout *wbLayout = new TQHBoxLayout(warningBanner, 5);
  warningLabel = new TQLabel(warningBanner);
  warningLabel->setPaletteForegroundColor(TQColor("#ffffff"));
  warningLabel->setAlignment(TQt::AlignCenter);
  warningLabel->setText("<b>Warning!</b> Selecting your system drive is "
                        "dangerous and will erase your drive!");
  wbLayout->addWidget(warningLabel);

  // ========== MAIN PAGE ==========
  mainPage = new TQWidget(this);
  setCentralWidget(mainPage);
  mainPage->setPaletteBackgroundColor(TQColor(BG_COLOR));

  // ========== IMAGE INFO PANEL (overlay) ==========
  imageInfoPanel = new TQWidget(mainPage);
  imageInfoPanel->setGeometry(winW * 7 / 100, winH * 24 / 100, winW * 86 / 100,
                              winH * 59 / 100);
  imageInfoPanel->setPaletteBackgroundColor(TQColor("#ffffff"));
  imageInfoPanel->hide();

  TQVBoxLayout *iiLayout = new TQVBoxLayout(imageInfoPanel, 40, 5);
  infoTitle = new TQLabel("<font color='#4d5057' size='+3'>Image</font>",
                          imageInfoPanel);
  iiLayout->addWidget(infoTitle);
  iiLayout->addSpacing(20);

  infoName = new TQLabel("", imageInfoPanel);
  infoName->setPaletteForegroundColor(TQColor("#4d5057"));
  iiLayout->addWidget(infoName);

  infoPath = new TQLabel("", imageInfoPanel);
  infoPath->setPaletteForegroundColor(TQColor("#4d5057"));
  infoPath->setAlignment(TQt::AlignLeft | TQt::AlignVCenter | TQt::WordBreak);
  iiLayout->addWidget(infoPath);

  iiLayout->addStretch(1);

  TQHBoxLayout *iiBtnRow = new TQHBoxLayout(iiLayout, 5);
  iiBtnRow->addStretch(1);
  RoundButton *rbInfoOk =
      new RoundButton("OK", TQColor(BTN_ACTIVE_COLOR), imageInfoPanel);
  rbInfoOk->setFixedSize(winW * 25 / 100, 46);
  rbInfoOk->setClickCallback(onHideImageInfo, this);
  iiBtnRow->addWidget(rbInfoOk);
  btnInfoOk = rbInfoOk;

  // ========== ERROR PANEL (overlay — same style as imageInfoPanel) ==========
  errorPanel = new TQWidget(mainPage);
  errorPanel->setGeometry(winW * 7 / 100, winH * 24 / 100, winW * 86 / 100,
                          winH * 59 / 100);
  errorPanel->setPaletteBackgroundColor(TQColor("#ffffff"));
  errorPanel->hide();

  TQVBoxLayout *epLayout = new TQVBoxLayout(errorPanel, 40, 5);

  TQLabel *epTitle = new TQLabel(
      "<font color='#4d5057' size='+3'>Failed targets</font>", errorPanel);
  epLayout->addWidget(epTitle);
  epLayout->addSpacing(20);

  // Table header row
  TQWidget *epHeaderWidget = new TQWidget(errorPanel);
  epHeaderWidget->setPaletteBackgroundColor(TQColor("#e9edf1"));
  epHeaderWidget->setFixedHeight(36);
  TQHBoxLayout *epHeaderRow = new TQHBoxLayout(epHeaderWidget, 8, 0);
  TQLabel *hdrTarget = new TQLabel("Target", epHeaderWidget);
  hdrTarget->setPaletteForegroundColor(TQColor("#3a7fc0"));
  TQFont hdrFont = hdrTarget->font();
  hdrFont.setBold(true);
  hdrTarget->setFont(hdrFont);
  hdrTarget->setFixedWidth(winW * 30 / 100);
  epHeaderRow->addWidget(hdrTarget);
  TQLabel *hdrLocation = new TQLabel("Location", epHeaderWidget);
  hdrLocation->setPaletteForegroundColor(TQColor("#3a7fc0"));
  hdrLocation->setFont(hdrFont);
  hdrLocation->setFixedWidth(winW * 15 / 100);
  epHeaderRow->addWidget(hdrLocation);
  TQLabel *hdrError = new TQLabel("Error", epHeaderWidget);
  hdrError->setPaletteForegroundColor(TQColor("#3a7fc0"));
  hdrError->setFont(hdrFont);
  epHeaderRow->addWidget(hdrError, 1);
  epLayout->addWidget(epHeaderWidget);

  // Table data row
  TQWidget *epDataWidget = new TQWidget(errorPanel);
  epDataWidget->setPaletteBackgroundColor(TQColor("#ffffff"));
  TQHBoxLayout *epDataRow = new TQHBoxLayout(epDataWidget, 8, 0);
  errorTargetLabel = new TQLabel("", epDataWidget);
  errorTargetLabel->setPaletteForegroundColor(TQColor("#4d5057"));
  errorTargetLabel->setAlignment(TQt::AlignLeft | TQt::AlignTop |
                                 TQt::WordBreak);
  errorTargetLabel->setFixedWidth(winW * 30 / 100);
  epDataRow->addWidget(errorTargetLabel, 0, TQt::AlignTop);
  errorLocationLabel = new TQLabel("", epDataWidget);
  errorLocationLabel->setPaletteForegroundColor(TQColor("#4d5057"));
  errorLocationLabel->setAlignment(TQt::AlignLeft | TQt::AlignTop);
  errorLocationLabel->setFixedWidth(winW * 15 / 100);
  epDataRow->addWidget(errorLocationLabel, 0, TQt::AlignTop);
  errorMsgLabel = new TQLabel("", epDataWidget);
  errorMsgLabel->setPaletteForegroundColor(TQColor("#4d5057"));
  errorMsgLabel->setAlignment(TQt::AlignLeft | TQt::AlignTop | TQt::WordBreak);
  epDataRow->addWidget(errorMsgLabel, 1, TQt::AlignTop);
  epLayout->addWidget(epDataWidget);

  // Separator line
  TQWidget *epSep = new TQWidget(errorPanel);
  epSep->setFixedHeight(1);
  epSep->setPaletteBackgroundColor(TQColor("#e0e0e0"));
  epLayout->addWidget(epSep);

  epLayout->addStretch(1);

  // Button row
  TQHBoxLayout *epBtnRow = new TQHBoxLayout(epLayout, 15);
  epBtnRow->addStretch(1);
  btnErrorCancel = new RoundButton("Cancel", TQColor("#ffffff"), errorPanel);
  btnErrorCancel->setTextColor(TQColor("#4d5057"));
  btnErrorCancel->setHoverTextColor(TQColor("#9b9b9b"));
  btnErrorCancel->setFixedSize(winW * 25 / 100, 46);
  btnErrorCancel->setFlat(true);
  btnErrorCancel->setClickCallback(
      [](void *w) { static_cast<MainWindow *>(w)->slotErrorCancel(); }, this);
  epBtnRow->addWidget(btnErrorCancel);
  epBtnRow->addSpacing(15);
  btnErrorRetry = new RoundButton("Retry failed targets",
                                  TQColor(BTN_ACTIVE_COLOR), errorPanel);
  btnErrorRetry->setTextColor(TQColor(BTN_TEXT_COLOR));
  btnErrorRetry->setFixedSize(winW * 30 / 100, 46);
  btnErrorRetry->setClickCallback(
      [](void *w) { static_cast<MainWindow *>(w)->slotErrorRetry(); }, this);
  epBtnRow->addWidget(btnErrorRetry);

  // ========== WARNING PANEL (overlay — image validation warnings) ==========
  warningPanel = new TQWidget(mainPage);
  warningPanel->setGeometry(winW * 7 / 100, winH * 24 / 100, winW * 86 / 100,
                            winH * 59 / 100);
  warningPanel->setPaletteBackgroundColor(TQColor("#ffffff"));
  warningPanel->hide();

  TQVBoxLayout *wpLayout = new TQVBoxLayout(warningPanel, 40, 5);

  // Title row with ⚠ icon
  TQHBoxLayout *wpTitleRow = new TQHBoxLayout(wpLayout, 8);
  TQLabel *wpWarnIcon = new TQLabel(warningPanel);
  wpWarnIcon->setText("<font color='#e8a317' size='+4'>&#9888;</font>");
  wpTitleRow->addWidget(wpWarnIcon, 0, TQt::AlignVCenter);
  wpTitleRow->addSpacing(8);
  warningTitleLabel = new TQLabel("", warningPanel);
  warningTitleLabel->setPaletteForegroundColor(TQColor("#4d5057"));
  TQFont wpTitleFont = warningTitleLabel->font();
  wpTitleFont.setPointSize(wpTitleFont.pointSize() + 6);
  warningTitleLabel->setFont(wpTitleFont);
  wpTitleRow->addWidget(warningTitleLabel, 1, TQt::AlignVCenter);

  wpLayout->addSpacing(20);

  warningMsgLabel = new TQLabel("", warningPanel);
  warningMsgLabel->setPaletteForegroundColor(TQColor("#6b6e73"));
  warningMsgLabel->setAlignment(TQt::AlignLeft | TQt::AlignTop |
                                TQt::WordBreak);
  wpLayout->addWidget(warningMsgLabel);

  wpLayout->addStretch(1);

  // Buttons: Continue (orange) + Cancel (flat text)
  TQHBoxLayout *wpBtnRow = new TQHBoxLayout(wpLayout, 15);
  wpBtnRow->addStretch(1);
  btnWarningContinue =
      new RoundButton("Continue", TQColor("#e8a317"), warningPanel);
  btnWarningContinue->setTextColor(TQColor("#ffffff"));
  btnWarningContinue->setFixedSize(winW * 25 / 100, 46);
  btnWarningContinue->setClickCallback(
      [](void *w) { static_cast<MainWindow *>(w)->slotWarningContinue(); },
      this);
  wpBtnRow->addWidget(btnWarningContinue);
  wpBtnRow->addSpacing(15);
  btnWarningCancel =
      new RoundButton("Cancel", TQColor("#ffffff"), warningPanel);
  btnWarningCancel->setTextColor(TQColor("#4d5057"));
  btnWarningCancel->setHoverTextColor(TQColor("#9b9b9b"));
  btnWarningCancel->setFixedSize(winW * 18 / 100, 46);
  btnWarningCancel->setFlat(true);
  btnWarningCancel->setClickCallback(
      [](void *w) { static_cast<MainWindow *>(w)->slotWarningCancel(); }, this);
  wpBtnRow->addWidget(btnWarningCancel);

  // ========== GENERIC ERROR PANEL (overlay — blocking space errors) ==========
  genericErrorPanel = new TQWidget(mainPage);
  genericErrorPanel->setGeometry(winW * 7 / 100, winH * 24 / 100,
                                 winW * 86 / 100, winH * 59 / 100);
  genericErrorPanel->setPaletteBackgroundColor(TQColor("#ffffff"));
  genericErrorPanel->hide();

  TQVBoxLayout *gepLayout = new TQVBoxLayout(genericErrorPanel, 40, 5);

  TQHBoxLayout *gepTitleRow = new TQHBoxLayout(gepLayout, 8);
  TQLabel *gepWarnIcon = new TQLabel(genericErrorPanel);
  gepWarnIcon->setText("<font color='#ff4444' size='+4'>&#9888;</font>");
  gepTitleRow->addWidget(gepWarnIcon, 0, TQt::AlignVCenter);
  gepTitleRow->addSpacing(8);
  genericErrorTitleLabel = new TQLabel("", genericErrorPanel);
  genericErrorTitleLabel->setPaletteForegroundColor(TQColor("#4d5057"));
  TQFont gepTitleFont = genericErrorTitleLabel->font();
  gepTitleFont.setPointSize(gepTitleFont.pointSize() + 6);
  genericErrorTitleLabel->setFont(gepTitleFont);
  gepTitleRow->addWidget(genericErrorTitleLabel, 1, TQt::AlignVCenter);

  gepLayout->addSpacing(20);

  genericErrorMsgLabel = new TQLabel("", genericErrorPanel);
  genericErrorMsgLabel->setPaletteForegroundColor(TQColor("#6b6e73"));
  genericErrorMsgLabel->setAlignment(TQt::AlignLeft | TQt::AlignTop |
                                     TQt::WordBreak);
  gepLayout->addWidget(genericErrorMsgLabel);

  gepLayout->addStretch(1);

  TQHBoxLayout *gepBtnRow = new TQHBoxLayout(gepLayout, 15);
  gepBtnRow->addStretch(1);
  btnGenericErrorOk =
      new RoundButton("OK", TQColor(BTN_ACTIVE_COLOR), genericErrorPanel);
  btnGenericErrorOk->setTextColor(TQColor("#ffffff"));
  btnGenericErrorOk->setFixedSize(winW * 18 / 100, 46);
  btnGenericErrorOk->setClickCallback(
      [](void *w) { static_cast<MainWindow *>(w)->slotGenericErrorOk(); },
      this);
  gepBtnRow->addWidget(btnGenericErrorOk);

  // ========== URL INPUT PANEL (overlay — same style as imageInfoPanel)
  // ==========
  m_urlAuthVisible = false;
  urlInputPanel = new TQWidget(mainPage);
  urlInputPanel->setGeometry(winW * 7 / 100, winH * 10 / 100, winW * 86 / 100,
                             winH * 75 / 100);
  urlInputPanel->setPaletteBackgroundColor(TQColor("#ffffff"));
  urlInputPanel->hide();

  TQVBoxLayout *uipLayout = new TQVBoxLayout(urlInputPanel, 40, 10);

  TQLabel *uipTitle = new TQLabel(
      "<font color='#4d5057' size='+3'>Use Image URL</font>", urlInputPanel);
  uipLayout->addWidget(uipTitle);
  uipLayout->addSpacing(10);

  // URL field
  urlTxtUrl = new TQLineEdit(urlInputPanel);
  urlTxtUrl->setPaletteForegroundColor(TQColor("#4d5057"));
  uipLayout->addWidget(urlTxtUrl);

  // Authentication toggle link
  urlAuthLabel = new TQLabel(
      "<font color='#2297de'>&#9658; Authentication</font>", urlInputPanel);
  urlAuthLabel->setCursor(TQCursor(TQt::PointingHandCursor));
  uipLayout->addWidget(urlAuthLabel);
  // We'll connect urlAuthLabel click via eventFilter or just use a button
  RoundButton *btnAuthToggle =
      new RoundButton("> Authentication", TQColor("#ffffff"), urlInputPanel);
  btnAuthToggle->setTextColor(TQColor("#2297de"));
  btnAuthToggle->setHoverTextColor(TQColor("#4ab8ff"));
  btnAuthToggle->setFlat(true);
  btnAuthToggle->setTextOnly(true);
  btnAuthToggle->setFixedHeight(30);
  btnAuthToggle->setClickCallback(
      [](void *w) { static_cast<MainWindow *>(w)->slotUrlToggleAuth(); }, this);
  uipLayout->addWidget(btnAuthToggle);
  urlAuthLabel->hide(); // hide the static label, we use the button

  // Auth container (hidden by default)
  urlAuthContainer = new TQWidget(urlInputPanel);
  urlAuthContainer->hide();
  TQVBoxLayout *authLayout = new TQVBoxLayout(urlAuthContainer, 0, 8);

  TQLabel *lblUser =
      new TQLabel("<font color='#6b6e73'>Username</font>", urlAuthContainer);
  authLayout->addWidget(lblUser);
  urlTxtUsername = new TQLineEdit(urlAuthContainer);
  urlTxtUsername->setPaletteForegroundColor(TQColor("#4d5057"));
  authLayout->addWidget(urlTxtUsername);

  TQLabel *lblPass =
      new TQLabel("<font color='#6b6e73'>Password</font>", urlAuthContainer);
  authLayout->addWidget(lblPass);
  urlTxtPassword = new TQLineEdit(urlAuthContainer);
  urlTxtPassword->setPaletteForegroundColor(TQColor("#4d5057"));
  urlTxtPassword->setEchoMode(TQLineEdit::Password);
  authLayout->addWidget(urlTxtPassword);

  uipLayout->addWidget(urlAuthContainer);
  uipLayout->addStretch(1);

  // Button row: Cancel | OK
  TQHBoxLayout *uipBtnRow = new TQHBoxLayout(uipLayout, 15);
  uipBtnRow->addStretch(1);

  RoundButton *btnUipCancel =
      new RoundButton("Cancel", TQColor("#ffffff"), urlInputPanel);
  btnUipCancel->setTextColor(TQColor("#4d5057"));
  btnUipCancel->setHoverTextColor(TQColor("#9b9b9b"));
  btnUipCancel->setFlat(true);
  btnUipCancel->setTextOnly(true);
  btnUipCancel->setFixedSize(winW * 18 / 100, 46);
  btnUipCancel->setClickCallback(onUrlCancel, this);
  uipBtnRow->addWidget(btnUipCancel);

  uipBtnRow->addSpacing(15);

  btnUrlOk = new RoundButton("OK", TQColor("#3b3e44"), urlInputPanel);
  btnUrlOk->setTextColor(TQColor("#ffffff"));
  btnUrlOk->setFixedSize(winW * 18 / 100, 46);
  btnUrlOk->setClickCallback(onUrlOk, this);
  uipBtnRow->addWidget(btnUrlOk);

  // --- Title bar area ---
  topLogoLabel = new TQLabel(mainPage);
  // Scale logo smaller for title bar
  TQImage logoImg;
  logoImg.loadFromData(logo_png, logo_png_len, "PNG");
  TQPixmap smallLogo;
  smallLogo.convertFromImage(logoImg.smoothScale(20, 20));
  topLogoLabel->setPixmap(smallLogo);
  topLogoLabel->setGeometry(winW / 2 - 60, 23, 20, 20);
  topLogoLabel->setBackgroundMode(TQt::NoBackground);

  topTitleLabel = new TQLabel("<font color='#ffffff' size='+1'>"
                              "<b>tde</b></font>"
                              "<font color='#a5de37' size='+1'>"
                              "<b>Flasher</b></font>",
                              mainPage);
  topTitleLabel->setGeometry(winW / 2 - 37, 16, 200, 28);
  topTitleLabel->setBackgroundMode(TQt::NoBackground);
  topTitleLabel->setPaletteBackgroundColor(TQColor(BG_COLOR));

  topLogoLabel->setBackgroundMode(TQt::NoBackground);
  topLogoLabel->setPaletteBackgroundColor(TQColor(BG_COLOR));

  RoundButton *btnHelp = new RoundButton("", TQColor(BG_COLOR), mainPage);
  btnHelp->setFlat(true);
  btnHelp->setIcon(iconHelp);
  btnHelp->setGeometry(winW - 50, 16, 28, 28);
  btnHelp->setClickCallback(onShowAboutDialog, this);

  // --- Three step icons with connecting lines ---
  int iconY = winH * 22 / 100;  // ~22% from top
  int icon1X = winW * 16 / 100; // ~16% from left
  int icon2X = winW / 2;        // center
  int icon3X = winW * 84 / 100; // ~84% from left
  int iconW = 64, iconH = 64;

  // Step 1 icon
  iconLabel1 = new TQLabel(mainPage);
  iconLabel1->setAlignment(TQt::AlignCenter);
  iconLabel1->setGeometry(icon1X - iconW / 2, iconY, iconW, iconH);

  // Step 2 icon
  iconLabel2 = new TQLabel(mainPage);
  iconLabel2->setAlignment(TQt::AlignCenter);
  iconLabel2->setGeometry(icon2X - iconW / 2, iconY, iconW, iconH);

  // Step 3 icon
  iconLabel3 = new TQLabel(mainPage);
  iconLabel3->setAlignment(TQt::AlignCenter);
  iconLabel3->setGeometry(icon3X - 22, iconY, 43, iconH);

  // Connecting lines
  int lineY = iconY + iconH / 2 - 1;
  line1 = new TQLabel(mainPage);
  line1->setPaletteBackgroundColor(TQColor(LINE_COLOR));
  line1->setGeometry(icon1X + iconW / 2 + 10, lineY,
                     icon2X - icon1X - iconW - 20, 2);

  line2 = new TQLabel(mainPage);
  line2->setPaletteBackgroundColor(TQColor(LINE_COLOR));
  line2->setGeometry(icon2X + iconW / 2 + 10, lineY,
                     icon3X - icon2X - iconW - 20, 2);

  // --- Buttons row ---
  int btnY = iconY + iconH + winH * 8 / 100;
  int btnW = winW * 22 / 100;
  int btnH = 46;

  // Step 1: "Flash from file" button
  RoundButton *rb1 =
      new RoundButton("Flash from file", TQColor(BTN_ACTIVE_COLOR), mainPage);
  rb1->setIcon(iconFile);
  rb1->setFlat(true);
  rb1->setGeometry(icon1X - btnW / 2, btnY, btnW, btnH);
  rb1->setClickCallback(onFlashFile, this);
  btnFlashFile = rb1;

  // Step 2: "Select target" button
  RoundButton *rb2 =
      new RoundButton("Select target", TQColor(BTN_INACTIVE_COLOR), mainPage);
  rb2->setGeometry(icon2X - btnW / 2, btnY, btnW, btnH);
  rb2->setClickCallback(onSelectTarget, this);
  rb2->setEnabled(false);
  btnSelectTarget = rb2;

  // Step 3: "Flash!" button
  RoundButton *rb3 =
      new RoundButton("Flash!", TQColor(BTN_INACTIVE_COLOR), mainPage);
  rb3->setGeometry(icon3X - btnW / 2, btnY, btnW, btnH);
  rb3->setClickCallback(onFlash, this);
  rb3->setEnabled(false);
  btnFlash = rb3;

  // --- Sub-options under step 1 ---
  int subY = btnY + btnH + 12;

  RoundButton *rbUrl =
      new RoundButton("Flash from URL", TQColor(BG_COLOR), mainPage);
  rbUrl->setIcon(iconLink);
  rbUrl->setFlat(true);
  rbUrl->setGeometry(icon1X - 80, subY, 160, btnH);
  rbUrl->setClickCallback(onFlashUrl, this);
  btnFlashUrl = rbUrl;

  RoundButton *rbClone =
      new RoundButton("Clone drive", TQColor(BG_COLOR), mainPage);
  rbClone->setIcon(iconCopy);
  rbClone->setFlat(true);
  rbClone->setGeometry(icon1X - 70, subY + btnH + 8, 140, btnH);
  rbClone->setClickCallback(onCloneDrive, this);
  btnCloneDrive = rbClone;

  // --- Step labels (shown when image/drive selected) ---
  stepLabel1 = new TQLabel("", mainPage);
  stepLabel1->setAlignment(TQt::AlignCenter);
  stepLabel1->setPaletteForegroundColor(TQColor(TEXT_MUTED));
  stepLabel1->setGeometry(icon1X - 100, btnY + btnH + 5, 200, 40);
  stepLabel1->hide();

  // Step 2 sub-buttons
  RoundButton *rbChange =
      new RoundButton("Change", TQColor(BG_COLOR), mainPage);
  rbChange->setFlat(true);
  rbChange->setGeometry(icon2X - 80, subY, 160, btnH);
  rbChange->setClickCallback(onSelectTarget, this);
  btnChangeTarget = rbChange;
  btnChangeTarget->hide();

  RoundButton *rbTargetSize = new RoundButton("", TQColor(BG_COLOR), mainPage);
  rbTargetSize->setFlat(true);
  rbTargetSize->setGeometry(icon2X - 70, subY + btnH + 8, 140, btnH);
  rbTargetSize->setEnabled(false);
  btnTargetSize = rbTargetSize;
  btnTargetSize->hide();

  stepLabel3 = new TQLabel("", mainPage);
  stepLabel3->setAlignment(TQt::AlignCenter);
  stepLabel3->setPaletteForegroundColor(TQColor(TEXT_MUTED));
  stepLabel3->setGeometry(icon3X - 100, btnY + btnH + 5, 200, 20);
  stepLabel3->hide();

  // ========== DRIVE SELECTION PANEL (overlay) ==========
  drivePanel = new TQWidget(mainPage);
  drivePanel->setGeometry(winW * 2 / 100, winH * 16 / 100, winW * 96 / 100,
                          winH * 76 / 100);
  drivePanel->setPaletteBackgroundColor(TQColor("#ffffff"));
  drivePanel->hide();

  TQVBoxLayout *dpLayout = new TQVBoxLayout(drivePanel, 20, 8);
  TQLabel *dpTitle =
      new TQLabel("<font color='#4d5057' size='+2'><b>Select target</b></font>",
                  drivePanel);
  drivePanelTitle = dpTitle;
  dpLayout->addWidget(dpTitle);

  driveList = new TQListView(drivePanel);
  driveList->addColumn("Name");
  driveList->addColumn("Size");
  driveList->addColumn("Location");
  driveList->setAllColumnsShowFocus(true);
  driveList->setSelectionMode(TQListView::Single);
  driveList->setPaletteBackgroundColor(TQColor("#ffffff"));
  driveList->setPaletteForegroundColor(TQColor("#4d5057"));
  connect(driveList, SIGNAL(selectionChanged(TQListViewItem *)), this,
          SLOT(slotDriveSelected(TQListViewItem *)));
  dpLayout->addWidget(driveList);

  btnShowHidden = new TQPushButton("Show 0 hidden", drivePanel);
  btnShowHidden->setFlat(true);
  btnShowHidden->setPaletteForegroundColor(TQColor(TEXT_LINK));
  btnShowHidden->setCursor(TQCursor(TQt::PointingHandCursor));
  connect(btnShowHidden, SIGNAL(clicked()), this,
          SLOT(slotToggleHiddenDrives()));
  dpLayout->addWidget(btnShowHidden);

  TQHBoxLayout *dpBtnRow = new TQHBoxLayout(dpLayout, 30);
  dpBtnRow->addStretch(1);

  RoundButton *rbCancelD =
      new RoundButton("Cancel", TQColor("#ffffff"), drivePanel);
  rbCancelD->setFixedSize(winW * 30 / 100, 48);
  rbCancelD->setFlat(true);
  rbCancelD->setTextColor(TQColor(TEXT_LINK));
  rbCancelD->setHoverTextColor(TQColor("#1b7fb9"));
  rbCancelD->setClickCallback(
      [](void *w) { static_cast<MainWindow *>(w)->slotDriveCancelled(); },
      this);
  dpBtnRow->addWidget(rbCancelD);

  RoundButton *rbSelectD =
      new RoundButton("Select (0)", TQColor(BTN_ACTIVE_COLOR), drivePanel);
  rbSelectD->setFixedSize(winW * 30 / 100, 48);
  rbSelectD->setEnabled(false);
  rbSelectD->setClickCallback(
      [](void *w) { static_cast<MainWindow *>(w)->slotDriveAccepted(); }, this);
  dpBtnRow->addWidget(rbSelectD);
  btnDriveOk = rbSelectD; // We'll keep this pointer

  dpBtnRow->addStretch(1);

  // ========== PROGRESS PANEL (split-screen overhaul) ==========
  progressPanel = new TQWidget(mainPage);
  progressPanel->setGeometry(0, 0, winW, winH);
  progressPanel->setPaletteBackgroundColor(TQColor(BG_COLOR));
  progressPanel->hide();

  TQHBoxLayout *ppMainLayout = new TQHBoxLayout(progressPanel);
  ppMainLayout->setMargin(0);
  ppMainLayout->setSpacing(0);
  ppMainLayout->setAutoAdd(false); // We'll add manually

  // Left Sidebar Stack
  progressStack = new TQWidgetStack(progressPanel);
  progressStack->setFixedWidth(winW * 34 / 100);

  // --- PAGE 0: FLASHING ---
  pageFlashing = new TQWidget(progressStack);
  pageFlashing->setPaletteBackgroundColor(TQColor(COLOR_SIDEBAR_BG));

  TQVBoxLayout *sbLayout = new TQVBoxLayout(pageFlashing, 20, 15);
  sbLayout->addSpacing(20);

  TQHBoxLayout *imgRow = new TQHBoxLayout(sbLayout);
  TQVBoxLayout *imgIconV = new TQVBoxLayout(imgRow);
  imgIconV->addSpacing(6); // 2px up from 8
  TQLabel *imgIcon = new TQLabel(pageFlashing);
  TQImage iAdd = iconAdd.convertToImage();
  TQPixmap pAdd;
  pAdd.convertFromImage(iAdd.smoothScale(24, 24));
  imgIcon->setPixmap(pAdd);
  imgIcon->setFixedSize(24, 24);
  imgIconV->addWidget(imgIcon, 0, TQt::AlignTop);
  imgRow->addSpacing(10);
  progImageLabel = new TQLabel(pageFlashing);
  progImageLabel->setPaletteForegroundColor(TQColor("#ffffff"));
  imgRow->addWidget(progImageLabel, 1);

  TQHBoxLayout *drvRow = new TQHBoxLayout(sbLayout);
  TQVBoxLayout *drvIconV = new TQVBoxLayout(drvRow);
  drvIconV->addSpacing(6); // 2px up from 8
  TQLabel *drvIcon = new TQLabel(pageFlashing);
  TQImage iDrv = iconDrive.convertToImage();
  TQPixmap pDrv;
  pDrv.convertFromImage(iDrv.smoothScale(24, 24));
  drvIcon->setPixmap(pDrv);
  drvIcon->setFixedSize(24, 24);
  drvIconV->addWidget(drvIcon, 0, TQt::AlignTop);
  drvRow->addSpacing(10);
  progDriveLabel = new TQLabel(pageFlashing);
  progDriveLabel->setPaletteForegroundColor(TQColor("#9b9b9b"));
  drvRow->addWidget(progDriveLabel, 1);

  sbLayout->addStretch(1);

  progFlashIcon = new TQLabel(pageFlashing);
  progFlashIcon->setAlignment(TQt::AlignCenter);
  // We'll scale the lightning icon to make it larger
  TQImage flashImg = iconFlash.convertToImage();
  TQPixmap largeFlash;
  largeFlash.convertFromImage(flashImg.smoothScale(48, 72));
  progFlashIcon->setPixmap(largeFlash);
  sbLayout->addWidget(progFlashIcon);

  sbLayout->addStretch(1);

  TQHBoxLayout *statusRow = new TQHBoxLayout(sbLayout);
  statusLabel = new TQLabel("<font color='white' size='+1'>Flashing... </font>"
                            "<font color='#da60ff' size='+1'><b>0%</b></font>",
                            pageFlashing);
  statusLabel->setAlignment(TQt::AlignLeft | TQt::AlignVCenter);
  statusRow->addWidget(statusLabel, 1);

  progCancelBtn =
      new RoundButton("Cancel", TQColor(COLOR_SIDEBAR_BG), pageFlashing);
  progCancelBtn->setFlat(true);
  progCancelBtn->setTextOnly(true);
  progCancelBtn->setTextColor(TQColor(TEXT_LINK));
  progCancelBtn->setHoverTextColor(
      TQColor("#9b9b9b")); // matches other flat links
  progCancelBtn->setFixedSize(60, 24);
  progCancelBtn->setClickCallback(onCancelFlash, this);
  statusRow->addWidget(progCancelBtn, 0, TQt::AlignRight | TQt::AlignVCenter);

  progressBar = new CustomProgressBar(pageFlashing);
  progressBar->setProgress(0);

  sbLayout->addWidget(progressBar);

  TQHBoxLayout *sbInfoRow = new TQHBoxLayout(sbLayout);
  speedLabel =
      new TQLabel("<font color='#9b9b9b'>0.00 MB/s</font>", pageFlashing);
  sbInfoRow->addWidget(speedLabel);
  sbInfoRow->addStretch(1);
  etaLabel = new TQLabel("<font color='#9b9b9b'>ETA: 0m</font>", pageFlashing);
  sbInfoRow->addWidget(etaLabel);

  sbLayout->addStretch(2);
  sbLayout->addSpacing(20);

  // --- PAGE 1: FINISHED ---
  pageFinished = new TQWidget(progressStack);
  pageFinished->setPaletteBackgroundColor(TQColor(COLOR_SIDEBAR_BG));

  TQVBoxLayout *finLayout = new TQVBoxLayout(pageFinished, 8, 8);
  finLayout->addStretch(1);

  progFinishedIcon = new TQLabel(pageFinished);
  progFinishedIcon->setAlignment(TQt::AlignCenter);
  // Scale complete icon properly
  TQImage iComp = iconComplete.convertToImage();
  TQPixmap pComp;
  pComp.convertFromImage(iComp.smoothScale(48, 48));
  progFinishedIcon->setPixmap(pComp);
  finLayout->addWidget(progFinishedIcon);

  progFinishedNameLabel = new TQLabel(pageFinished);
  progFinishedNameLabel->setAlignment(TQt::AlignCenter);
  progFinishedNameLabel->setPaletteForegroundColor(TQColor(TEXT_MUTED));
  finLayout->addWidget(progFinishedNameLabel);

  finLayout->addSpacing(20);

  progFinishedTitleLabel = new TQLabel(
      "<font color='white' size='+3'>Flash Complete!</font>", pageFinished);
  progFinishedTitleLabel->setAlignment(TQt::AlignHCenter);
  finLayout->addWidget(progFinishedTitleLabel);

  finLayout->addSpacing(10);

  TQWidget *finStatusWidget = new TQWidget(pageFinished);
  finStatusWidget->setPaletteBackgroundColor(TQColor(COLOR_SIDEBAR_BG));
  TQHBoxLayout *finStatusRow = new TQHBoxLayout(finStatusWidget, 0, 0);
  TQLabel *dotLabel = new TQLabel(finStatusWidget);
  dotLabel->setPixmap(iconDot);
  dotLabel->setFixedSize(iconDot.width(), iconDot.height());
  finStatusRow->addWidget(dotLabel, 0, TQt::AlignVCenter);
  finStatusRow->addSpacing(4);
  TQLabel *countLabel = new TQLabel("1", finStatusWidget);
  TQFont countFont = countLabel->font();
  countFont.setBold(true);
  countLabel->setFont(countFont);
  countLabel->setPaletteForegroundColor(TQColor("#ffffff"));
  finStatusRow->addWidget(countLabel, 0, TQt::AlignVCenter);
  finStatusRow->addSpacing(4);
  progFinishedStatusLabel = new TQLabel("Successful target", finStatusWidget);
  progFinishedStatusLabel->setPaletteForegroundColor(TQColor(TEXT_MUTED));
  finStatusRow->addWidget(progFinishedStatusLabel, 0, TQt::AlignVCenter);
  finLayout->addWidget(finStatusWidget, 0, TQt::AlignHCenter);

  progFinishedSpeedLabel =
      new TQLabel("Effective speed: 0.0 MB/s", pageFinished);
  progFinishedSpeedLabel->setPaletteForegroundColor(TQColor(TEXT_MUTED));
  progFinishedSpeedLabel->setAlignment(TQt::AlignHCenter);
  finLayout->addWidget(progFinishedSpeedLabel);

  finLayout->addStretch(1);

  btnFlashAnother =
      new RoundButton("Flash another", TQColor(BTN_ACTIVE_COLOR), pageFinished);
  btnFlashAnother->setTextColor(TQColor(BTN_TEXT_COLOR));
  btnFlashAnother->setFixedSize(200, 40);
  btnFlashAnother->setClickCallback(onFlashAnother, this);
  finLayout->addWidget(btnFlashAnother, 0, TQt::AlignHCenter);

  finLayout->addSpacing(20);

  // Add pages to stack
  progressStack->addWidget(pageFlashing, 0);
  progressStack->addWidget(pageFinished, 1);
  ppMainLayout->addWidget(progressStack);

  // Right Main Area
  progressMain = new TQWidget(progressPanel);
  progressMain->setPaletteBackgroundColor(TQColor("#414449"));

  // ppMainLayout->addWidget(progressStack) is already called above
  ppMainLayout->addWidget(progressMain, 1);

  TQVBoxLayout *mainAreaLayout = new TQVBoxLayout(progressMain);
  mainAreaLayout->setSpacing(0);
  mainAreaLayout->setMargin(0);
  mainAreaLayout->addStretch(1);
  progLogoLabel = new TQLabel(progressMain);
  progLogoLabel->setAlignment(TQt::AlignCenter);
  progLogoLabel->setPixmap(iconTde);
  progLogoLabel->setMinimumSize(200, 200);
  mainAreaLayout->addWidget(progLogoLabel);
  mainAreaLayout->addStretch(1);

  // Progress poll timer
  progressTimer = new TQTimer(this);
  connect(progressTimer, SIGNAL(timeout()), this, SLOT(slotPollProgress()));

  // Set initial visual state
  updateStepVisuals();
}

MainWindow::~MainWindow() {
  progressTimer->stop();
  if (g_worker) {
    if (!g_worker->finished)
      pthread_join(g_worker->thread, nullptr);
    delete g_worker;
    g_worker = nullptr;
  }
}

void MainWindow::loadEmbeddedIcons() {
  iconAdd.loadFromData(add_image_png, add_image_png_len, "PNG");
  iconDrive.loadFromData(drive_png, drive_png_len, "PNG");
  iconFlash.loadFromData(flash_png, flash_png_len, "PNG");
  iconLogo.loadFromData(logo_png, logo_png_len, "PNG");
  if (!iconTde.loadFromData(tde_png, tde_png_len, "PNG")) {
    printf("DEBUG: Failed to load iconTde (size %u)\n", tde_png_len);
  }
  iconFile.loadFromData(file_png, file_png_len, "PNG");
  iconLink.loadFromData(link_png, link_png_len, "PNG");
  iconCopy.loadFromData(copy_png, copy_png_len, "PNG");
  iconComplete.loadFromData(complete_png, complete_png_len, "PNG");
  iconDot.loadFromData(dot_png, dot_png_len, "PNG");
  iconHelp.loadFromData(help_png, help_png_len, "PNG");

  iconAddGray = grayPixmap(iconAdd);
  iconDriveGray = grayPixmap(iconDrive);
  iconFlashGray = grayPixmap(iconFlash);

  // Register icons for HTML <img> tags
  TQMimeSourceFactory::defaultFactory()->setPixmap("add", iconAdd);
  TQMimeSourceFactory::defaultFactory()->setPixmap("drive", iconDrive);
}

void MainWindow::updateStepVisuals() {
  RoundButton *rb1 = static_cast<RoundButton *>(btnFlashFile);
  RoundButton *rb2 = static_cast<RoundButton *>(btnSelectTarget);
  RoundButton *rb3 = static_cast<RoundButton *>(btnFlash);
  RoundButton *rbUrl = static_cast<RoundButton *>(btnFlashUrl);
  RoundButton *rbClone = static_cast<RoundButton *>(btnCloneDrive);

  // Step 1
  if (!selectedImage.isEmpty()) {
    iconLabel1->show();
    iconLabel1->setPixmap(iconAdd);

    if (!m_selectedSourceDesc.isEmpty()) {
      rb1->setText(m_selectedSourceDesc.left(25));
    } else {
      TQFileInfo fi(selectedImage);
      rb1->setText(fi.fileName().left(25));
    }

    rb1->setIcon(TQPixmap());
    rb1->setTextOnly(true);
    rb1->setTextColor(TQColor("#ffffff"));
    rb1->setHoverTextColor(TQColor("#9b9b9b"));
    rb1->setClickCallback(onShowImageInfo, this);
    rb1->setEnabled(true);

    rbUrl->setText("Remove");
    rbUrl->setIcon(TQPixmap());
    rbUrl->setTextOnly(true);
    rbUrl->setTextColor(TQColor(TEXT_LINK));
    rbUrl->setHoverTextColor(TQColor("#9b9b9b"));
    rbUrl->setClickCallback(onRemoveImage, this);
    rbUrl->show();

    uint64_t displaySize = selectedImageSize;
    if (displaySize == 0)
      displaySize = TQFileInfo(selectedImage).size();
    double sizeMB = (double)displaySize / (1024.0 * 1024.0);
    rbClone->setText(TQString("%1 MB").arg(sizeMB, 0, 'f', 2));
    rbClone->setIcon(TQPixmap());
    rbClone->setTextOnly(true);
    rbClone->setTextColor(TQColor(TEXT_MUTED));
    rbClone->setEnabled(false);
    rbClone->show();

    stepLabel1->hide();
  } else {
    iconLabel1->show();
    iconLabel1->setPixmap(iconAdd);

    rb1->setText("Flash from file");
    rb1->setIcon(iconFile);
    rb1->setTextOnly(false);
    rb1->setFlat(true);
    rb1->setTextColor(TQColor(BTN_TEXT_COLOR));
    rb1->setHoverTextColor(TQColor("#ffffff"));
    rb1->setClickCallback(onFlashFile, this);

    rbUrl->setText("Flash from URL");
    rbUrl->setIcon(iconLink);
    rbUrl->setTextOnly(false);
    rbUrl->setFlat(true);
    rbUrl->setTextColor(TQColor(BTN_TEXT_COLOR));
    rbUrl->setHoverTextColor(TQColor("#ffffff"));
    rbUrl->setClickCallback(onFlashUrl, this);
    rbUrl->show();

    rbClone->setText("Clone drive");
    rbClone->setIcon(iconCopy);
    rbClone->setTextOnly(false);
    rbClone->setFlat(true);
    rbClone->setTextColor(TQColor(BTN_TEXT_COLOR));
    rbClone->setHoverTextColor(TQColor("#ffffff"));
    rbClone->setClickCallback(onCloneDrive, this);
    rbClone->setEnabled(true);
    rbClone->show();

    stepLabel1->hide();
  }

  // Step 2
  if (currentStep >= 2) {
    iconLabel2->setPixmap(iconDrive);
    rb2->setEnabled(true);
    if (!selectedDevice.isEmpty()) {
      rb2->setText(selectedDeviceDesc.left(25));
      rb2->setIcon(TQPixmap());
      rb2->setTextOnly(true);
      rb2->setTextColor(TQColor("#ffffff"));
      rb2->setHoverTextColor(TQColor("#9b9b9b"));
      rb2->setClickCallback(onSelectTarget, this);

      btnChangeTarget->setText("Change");
      btnChangeTarget->setIcon(TQPixmap());
      btnChangeTarget->setTextOnly(true);
      btnChangeTarget->setTextColor(TQColor(TEXT_LINK));
      btnChangeTarget->setHoverTextColor(TQColor("#9b9b9b"));
      btnChangeTarget->show();

      btnTargetSize->setText(selectedDeviceSizeStr + " (" + selectedDevice +
                             ")");
      btnTargetSize->setIcon(TQPixmap());
      btnTargetSize->setTextOnly(true);
      btnTargetSize->setTextColor(TQColor(TEXT_MUTED));
      btnTargetSize->show();
    } else {
      rb2->setBg(TQColor(BTN_ACTIVE_COLOR));
      rb2->setText("Select target");
      rb2->setIcon(TQPixmap());
      rb2->setTextOnly(false);
      rb2->setTextColor(TQColor(BTN_TEXT_COLOR));
      rb2->setHoverTextColor(TQColor("#ffffff"));
      rb2->setClickCallback(onSelectTarget, this);

      btnChangeTarget->hide();
      btnTargetSize->hide();
    }
  } else {
    iconLabel2->setPixmap(iconDriveGray);
    rb2->setBg(TQColor(BTN_INACTIVE_COLOR));
    rb2->setText("Select target");
    rb2->setIcon(TQPixmap());
    rb2->setTextOnly(false);
    rb2->setTextColor(TQColor(BTN_TEXT_COLOR));
    rb2->setEnabled(false);

    btnChangeTarget->hide();
    btnTargetSize->hide();
  }

  // Step 3
  if (currentStep >= 3 && !selectedDevice.isEmpty()) {
    iconLabel3->setPixmap(iconFlash);
    rb3->setBg(TQColor(BTN_ACTIVE_COLOR));
    rb3->setEnabled(true);
  } else {
    iconLabel3->setPixmap(iconFlashGray);
    rb3->setBg(TQColor(BTN_INACTIVE_COLOR));
    rb3->setEnabled(false);
  }
}

void MainWindow::setStep(int step) {
  currentStep = step;
  updateStepVisuals();
}

// ---- Step 1 actions ----

void MainWindow::slotFlashFromFile() {
  if (!selectedImage.isEmpty()) {
    // Already selected — clicking again re-opens
  }
  TQString path = TQFileDialog::getOpenFileName(
      TQString::null,
      "Image Files (*.img *.iso *.gz *.bz2 *.xz *.zst *.zip *.lz4);;"
      "All Files (*)",
      this, "open_image", "Select Image File");

  if (!path.isEmpty()) {
    selectedImage = path;
    selectedImageSize = TQFileInfo(path).size();
    m_selectedSourceDesc = "";
    m_urlUsername = "";
    m_urlPassword = "";
    if (!validateImage(path)) {
      // Warning shown — don't proceed yet (Continue callback will call setStep)
      return;
    }
    setStep(2);
  }
}

void MainWindow::slotRemoveImage() {
  selectedImage = "";
  setStep(1);
}

void MainWindow::slotFlashAnother() {
  selectedDevice = "";
  selectedDeviceDesc = "";
  selectedDeviceSizeStr = "";
  updateStepVisuals();
  btnDriveOk->setText("Select (0)");

  // Hide info panel if open
  imageInfoPanel->hide();
  progressPanel->hide();
  topLogoLabel->setPaletteBackgroundColor(TQColor(BG_COLOR));
  topTitleLabel->setPaletteBackgroundColor(TQColor(BG_COLOR));
  setStep(2);
}

void MainWindow::slotCancelFlash() {
  if (flashing && g_worker) { // Removed accidental rename
    g_worker->cancelled = true;
    statusLabel->setText("<font color='white' size='+1'>Cancelling...</font>");
  }
}

void MainWindow::slotShowImageInfo() {
  if (selectedImage.isEmpty())
    return;

  // If it's a URL, display it directly
  if (selectedImage.startsWith("http://") ||
      selectedImage.startsWith("https://")) {
    int lastSlash = selectedImage.findRev('/');
    TQString fileName = selectedImage;
    if (lastSlash != -1 && lastSlash < (int)selectedImage.length() - 1) {
      fileName = selectedImage.mid(lastSlash + 1);
    }
    // Remove any query params from filename display
    int queryIndex = fileName.find('?');
    if (queryIndex != -1)
      fileName = fileName.left(queryIndex);

    infoName->setText(TQString("<b>Name:</b> %1").arg(fileName));
    infoPath->setText(TQString("<b>URL:</b> %1").arg(selectedImage));
  } else {
    // Local file
    TQFileInfo fi(selectedImage);
    infoName->setText(TQString("<b>Name:</b> %1").arg(fi.fileName()));
    infoPath->setText(TQString("<b>Path:</b> %1").arg(fi.absFilePath()));
  }

  imageInfoPanel->show();
  imageInfoPanel->raise();
}

void MainWindow::slotHideImageInfo() { imageInfoPanel->hide(); }

void MainWindow::slotFlashFromUrl() {
  // Reset the URL fields
  urlTxtUrl->setText("");
  urlTxtUsername->setText("");
  urlTxtPassword->setText("");
  m_urlAuthVisible = false;
  urlAuthContainer->hide();

  urlInputPanel->raise();
  urlInputPanel->show();
}

void MainWindow::slotUrlCancel() { urlInputPanel->hide(); }

void MainWindow::slotUrlOk() {
  TQString url = urlTxtUrl->text().stripWhiteSpace();
  if (url.isEmpty())
    return;

  selectedImage = url;
  m_urlUsername = urlTxtUsername->text();
  m_urlPassword = urlTxtPassword->text();

  // Do a HEAD request to get Content-Length for display
  selectedImageSize = 0;
  CURL *head = curl_easy_init();
  if (head) {
    std::string urlStr = url.utf8().data();
    curl_easy_setopt(head, CURLOPT_URL, urlStr.c_str());
    curl_easy_setopt(head, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(head, CURLOPT_FOLLOWLOCATION, 1L);
    if (!m_urlUsername.isEmpty() || !m_urlPassword.isEmpty()) {
      std::string auth = std::string(m_urlUsername.utf8().data()) + ":" +
                         std::string(m_urlPassword.utf8().data());
      curl_easy_setopt(head, CURLOPT_USERPWD, auth.c_str());
    }
    if (curl_easy_perform(head) == CURLE_OK) {
      double cl = 0;
      curl_easy_getinfo(head, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &cl);
      if (cl > 0)
        selectedImageSize = static_cast<uint64_t>(cl);
    }
    curl_easy_cleanup(head);
  }

  urlInputPanel->hide();

  // Reset target selection and move to step 2
  selectedDevice = "";
  setStep(2);
}

void MainWindow::slotUrlToggleAuth() {
  m_urlAuthVisible = !m_urlAuthVisible;
  if (m_urlAuthVisible) {
    urlAuthContainer->show();
  } else {
    urlAuthContainer->hide();
    urlTxtUsername->setText("");
    urlTxtPassword->setText("");
  }
}

void MainWindow::slotCloneDrive() {
  m_selectingSourceDrive = true;
  drivePanel->show();
  drivePanel->raise();
  slotRefreshDrives();
}

// ---- Step 2 actions ----

void MainWindow::slotSelectTarget() { showDriveSelector(); }

void MainWindow::showDriveSelector() {
  m_selectingSourceDrive = false; // Ensure we are selecting a target
  drivePanel->show();
  drivePanel->raise();
  slotRefreshDrives();
}

void MainWindow::slotToggleHiddenDrives() {
  m_showHiddenDrives = !m_showHiddenDrives;
  slotRefreshDrives();
}

void MainWindow::slotRefreshDrives() {
  driveList->clear();

  if (m_selectingSourceDrive) {
    drivePanelTitle->setText(
        "<font color='#4d5057' size='+2'><b>Select source</b></font>");
  } else {
    drivePanelTitle->setText(
        "<font color='#4d5057' size='+2'><b>Select target</b></font>");
  }

  btnDriveOk->setEnabled(false);
  m_selectedDriveCount = 0;
  btnDriveOk->setText("Select (0)");
  btnDriveOk->setBg(TQColor(BTN_ACTIVE_COLOR));
  warningBanner->hide();

  std::vector<flasher::DriveInfo> drives = flasher::scanDrives();

  int hiddenCount = 0;
  for (const auto &d : drives) {
    if (d.readOnly)
      continue;

    bool isSystem = d.isSystem;
    if (isSystem && !m_showHiddenDrives) {
      hiddenCount++;
      continue;
    }

    // If we are selecting a target drive, filter out the source drive if it's a
    // device
    if (!m_selectingSourceDrive && !selectedImage.isEmpty() &&
        selectedImage.startsWith("/dev/")) {
      if (d.device == selectedImage.ascii()) {
        continue; // Skip the source drive
      }
    }

    TQString dev = TQString(d.device.c_str());
    TQString desc = TQString(d.description.c_str());
    double sizeGB = (double)d.size / (1024.0 * 1024.0 * 1024.0);
    TQString sizeStr = (sizeGB >= 1024.0)
                           ? TQString("%1 TB").arg(sizeGB / 1024.0, 0, 'f', 1)
                           : TQString("%1 GB").arg(sizeGB, 0, 'f', 1);

    TQString name = desc;
    if (isSystem)
      name += " (System drive)";
    if (m_selectingSourceDrive) { // If selecting source, mark it as such
      name += " (Source)";
    }

    TQListViewItem *item = new TQListViewItem(driveList, name, sizeStr, dev,
                                              TQString::number(d.size));
    if (isSystem) {
      TQImage iconImg = iconLogo.convertToImage();
      TQPixmap scaled;
      scaled.convertFromImage(iconImg.smoothScale(16, 16));
      item->setPixmap(0, scaled);
    }
  }

  btnShowHidden->setText(m_showHiddenDrives
                             ? "Hide system drives"
                             : TQString("Show %1 hidden").arg(hiddenCount));
  btnShowHidden->setEnabled(hiddenCount > 0 || m_showHiddenDrives);
  if (hiddenCount == 0 && !m_showHiddenDrives)
    btnShowHidden->hide();
  else
    btnShowHidden->show();
}

void MainWindow::slotDriveSelected(TQListViewItem *item) {
  if (!item) {
    btnDriveOk->setEnabled(false);
    btnDriveOk->setText("Select (0)");
    btnDriveOk->setBg(TQColor(BTN_ACTIVE_COLOR));
    warningBanner->hide();
    return;
  }

  selectedDevice = item->text(2);
  selectedDeviceDesc = item->text(0);
  selectedDeviceSizeStr = item->text(1);
  m_selectedDriveCount = 1;

  btnDriveOk->setEnabled(true);
  btnDriveOk->setText("Select (1)");

  if (selectedDeviceDesc.contains("(System drive)")) {
    btnDriveOk->setBg(TQColor(COLOR_WARNING));
    warningBanner->show();
    warningBanner->raise();
  } else {
    btnDriveOk->setBg(TQColor(BTN_ACTIVE_COLOR));
    warningBanner->hide();
  }
}

void MainWindow::slotDriveAccepted() {
  if (selectedDevice.isEmpty()) {
    drivePanel->hide();
    return;
  }

  // Find selected item to get raw size
  TQListViewItem *item = driveList->selectedItem();
  if (!item) {
    drivePanel->hide();
    return;
  }

  uint64_t driveSize = item->text(3).toULongLong();
  bool isSystemItem = selectedDeviceDesc.contains("(System drive)");

  // If we are selecting a source drive, we don't apply target drive
  // restrictions. We just select it, set it as the image, and move to step 2.
  if (m_selectingSourceDrive) {
    selectedImage = selectedDevice;
    selectedImageSize = driveSize;
    m_selectedSourceDesc = selectedDeviceDesc;
    m_selectingSourceDrive = false; // Reset flag
    selectedDevice = "";            // Clear target device
    selectedDeviceSizeStr = "";
    drivePanel->hide();
    setStep(2);
    return;
  }

  // 1. Not enough space check (Error)
  if (selectedImageSize > 0 &&
      driveSize < selectedImageSize) { // Only check if image size is known
    genericErrorTitleLabel->setText("Not enough space on the drive");
    genericErrorMsgLabel->setText("Please insert a larger one and try again.");
    genericErrorPanel->show();
    genericErrorPanel->raise();
    return; // block continue
  }

  // 2. System drive check (Warning)
  if (isSystemItem) {
    warningTitleLabel->setText("System drive");
    warningMsgLabel->setText(
        "Selecting your system drive is dangerous and will erase your "
        "drive!\n\nAre you sure you want to flash your system drive?");
    // Repurpose the warningCallback for drive warning rather than image warning
    btnWarningContinue->setClickCallback(
        [](void *w) {
          static_cast<MainWindow *>(w)->slotDriveWarningContinue();
        },
        this);
    btnWarningCancel->setClickCallback(
        [](void *w) { static_cast<MainWindow *>(w)->slotDriveWarningCancel(); },
        this);
    warningPanel->show();
    warningPanel->raise();
    return;
  }

  // 3. Large drive check (>128GB) (Warning)
  if (driveSize > 128ULL * 1024 * 1024 * 1024) {
    warningTitleLabel->setText("Large drive");
    warningMsgLabel->setText(
        "This is an unusually large drive! Make sure it doesn't contain files "
        "that you want to keep.\n\nAre you sure the selected drive is not a "
        "storage drive?");
    btnWarningContinue->setClickCallback(
        [](void *w) {
          static_cast<MainWindow *>(w)->slotDriveWarningContinue();
        },
        this);
    btnWarningCancel->setClickCallback(
        [](void *w) { static_cast<MainWindow *>(w)->slotDriveWarningCancel(); },
        this);
    warningPanel->show();
    warningPanel->raise();
    return;
  }

  drivePanel->hide();
  setStep(3);
}

void MainWindow::slotDriveCancelled() { drivePanel->hide(); }

// ---- Step 3: Flash ----

void MainWindow::slotFlash() {
  if (selectedImage.isEmpty() || selectedDevice.isEmpty())
    return;

  flashing = true;
  static_cast<RoundButton *>(btnFlashFile)->setEnabled(false);
  static_cast<RoundButton *>(btnSelectTarget)->setEnabled(false);
  static_cast<RoundButton *>(btnFlash)->setEnabled(false);

  // Show progress overlay
  progressPanel->show();
  progressPanel->raise();
  topLogoLabel->setPaletteBackgroundColor(TQColor("#414449"));
  topTitleLabel->setPaletteBackgroundColor(TQColor("#414449"));
  topLogoLabel->raise();
  topTitleLabel->raise();

  // Populate sidebar details
  if (!m_selectedSourceDesc.isEmpty()) {
    progImageLabel->setText(m_selectedSourceDesc.left(25));
  } else {
    TQFileInfo fi(selectedImage);
    progImageLabel->setText(fi.fileName().left(25));
  }
  progDriveLabel->setText(selectedDeviceDesc.left(25));

  progressBar->setProgress(0);
  statusLabel->setText("<font color='white' size='+1'>Flashing... </font>"
                       "<font color='#da60ff' size='+1'><b>0%</b></font>");
  speedLabel->setText("<font color='#9b9b9b'>0.00 MB/s</font>");
  etaLabel->setText("<font color='#9b9b9b'>ETA: 0m</font>");

  progressStack->raiseWidget(pageFlashing);

  // Create worker
  if (g_worker)
    delete g_worker;
  g_worker = new FlashWorkerData();
  g_worker->imagePath =
      selectedImage.isEmpty() ? "" : std::string(selectedImage.utf8().data());
  g_worker->devicePath =
      selectedDevice.isEmpty() ? "" : std::string(selectedDevice.utf8().data());
  g_worker->urlUsername =
      m_urlUsername.isEmpty() ? "" : std::string(m_urlUsername.utf8().data());
  g_worker->urlPassword =
      m_urlPassword.isEmpty() ? "" : std::string(m_urlPassword.utf8().data());
  g_worker->verify = true; // Hardcoded for now, could be an option
  g_worker->state = flasher::STATE_IDLE;
  g_worker->bytesProcessed = 0;
  g_worker->totalBytes = 0;
  g_worker->speed = 0;
  g_worker->percentDone = 0;
  g_worker->finished = false;
  g_worker->cancelled = false;
  g_worker->errorMsg[0] = '\0';

  pthread_create(&g_worker->thread, nullptr, flashThreadFunc, g_worker);
  progressTimer->start(100);
}

void MainWindow::slotPollProgress() {
  if (!g_worker)
    return;

  int pct = g_worker->percentDone;
  progressBar->setProgress(pct);

  double speedMB = g_worker->speed / (1024.0 * 1024.0);
  speedLabel->setText(TQString("<font color='#9b9b9b'>%1 MB/s</font>")
                          .arg(TQString::number(speedMB, 'f', 1)));

  if (g_worker->totalBytes > 0 && g_worker->speed > 0) {
    uint64_t remaining = g_worker->totalBytes - g_worker->bytesProcessed;
    int etaSec = (int)((double)remaining / g_worker->speed);
    int min = etaSec / 60;
    int sec = etaSec % 60;
    TQString minStr = TQString::number(min);
    TQString secStr = TQString::number(sec);
    if (sec < 10)
      secStr = "0" + secStr;
    etaLabel->setText(TQString("<font color='#9b9b9b'>ETA: %1m%2s</font>")
                          .arg(minStr)
                          .arg(secStr));
  }

  TQString stateText = "Flashing... ";
  if (g_worker->state == flasher::STATE_VERIFYING) {
    stateText = "Verifying... ";
  }

  statusLabel->setText(
      TQString("<font color='white' size='+1'>%1</font>"
               "<font color='#da60ff' size='+1'><b>%2%</b></font>")
          .arg(stateText)
          .arg(TQString::number(pct)));

  if (g_worker->finished) {
    progressTimer->stop();
    pthread_join(g_worker->thread, nullptr);

    if (g_worker->state == flasher::STATE_DONE) {
      progressBar->setProgress(100);
      statusLabel->setText(
          "<font color='#7dc343'><b>Flash completed!</b></font>");

      // Setup Finished Page dynamic info
      double speedMB = g_worker->speed / (1024.0 * 1024.0);
      progFinishedSpeedLabel->setText(
          TQString("Effective speed: %1 MB/s")
              .arg(TQString::number(speedMB, 'f', 1)));

      TQFileInfo fi(selectedImage);
      progFinishedNameLabel->setText(fi.fileName());

      // Show Finished Page instead of native dialog
      progressStack->raiseWidget(pageFinished);

      // Cleanup thread
      delete g_worker;
      g_worker = nullptr;
      flashing = false;
      return;
    } else if (g_worker->cancelled) {
      // Return to step 3 on cancellation
      flashing = false;
      delete g_worker;
      g_worker = nullptr;
      progressPanel->hide();
      topLogoLabel->setPaletteBackgroundColor(TQColor(BG_COLOR));
      topTitleLabel->setPaletteBackgroundColor(TQColor(BG_COLOR));
      static_cast<RoundButton *>(btnFlashFile)->setEnabled(true);
      static_cast<RoundButton *>(btnSelectTarget)->setEnabled(true);
      updateStepVisuals();
      return;
    } else {
      TQString errMsg = TQString::fromUtf8(g_worker->errorMsg);
      statusLabel->setText("<font color='red'><b>Error!</b></font>");

      if (errMsg.startsWith("Verification failed:")) {
        genericErrorTitleLabel->setText("Validation Error");
        genericErrorMsgLabel->setText(
            "The write has been completed successfully but TdeFlasher detected "
            "potential corruption issues when reading the image back from the "
            "drive.\n\n"
            "Please consider writing the image to a different drive.");
        genericErrorPanel->show();
        genericErrorPanel->raise();
      } else {
        // Populate standard error panel
        errorTargetLabel->setText(selectedDeviceDesc);
        errorLocationLabel->setText(selectedDevice);
        errorMsgLabel->setText(errMsg);

        errorPanel->show();
        errorPanel->raise();
      }
    }

    flashing = false;
    delete g_worker;
    g_worker = nullptr;

    progressPanel->hide();
    topLogoLabel->setPaletteBackgroundColor(TQColor(BG_COLOR));
    topTitleLabel->setPaletteBackgroundColor(TQColor(BG_COLOR));
    static_cast<RoundButton *>(btnFlashFile)->setEnabled(true);
    static_cast<RoundButton *>(btnSelectTarget)->setEnabled(true);
    updateStepVisuals();

    // Show error overlay
    errorPanel->show();
    errorPanel->raise();
  }
}

void MainWindow::slotErrorCancel() {
  errorPanel->hide();
  setStep(2);
}

void MainWindow::slotErrorRetry() {
  errorPanel->hide();
  slotFlash();
}

bool MainWindow::validateImage(const TQString &path) {
  TQFileInfo fi(path);
  TQString basename = fi.fileName().lower();

  // Check 1: Windows image filename
  if (basename.contains("windows") || basename.contains("win7") ||
      basename.contains("win8") || basename.contains("win10") ||
      basename.contains("win11") || basename.contains("winxp")) {
    warningTitleLabel->setText("Possible Windows image detected");
    warningMsgLabel->setText(
        "It looks like you are trying to burn a Windows image.\n\n"
        "Unlike other images, Windows images require special processing "
        "to be made bootable. We suggest you use a tool specially designed "
        "for this purpose, such as Rufus (Windows), WoeUSB (Linux), "
        "or Boot Camp Assistant (macOS).");
    warningPanel->show();
    warningPanel->raise();
    return false;
  }

  // Check 2: Missing partition table (MBR/GPT)
  // Only for non-compressed raw images
  TQString ext = fi.extension(false).lower();
  if (ext == "gz" || ext == "bz2" || ext == "xz" || ext == "zst" ||
      ext == "zip" || ext == "lz4") {
    return true; // Can't check compressed files
  }

  FILE *f = fopen(path.utf8().data(), "rb");
  if (!f)
    return true; // Can't open — let it fail later

  unsigned char header[1024];
  size_t nread = fread(header, 1, 1024, f);
  fclose(f);

  if (nread < 512)
    return true; // File too small to check

  // Check MBR signature: bytes 510-511 == 0x55, 0xAA
  bool hasMBR = (header[510] == 0x55 && header[511] == 0xAA);

  // Check GPT header: "EFI PART" at offset 512
  bool hasGPT = false;
  if (nread >= 520) {
    hasGPT = (memcmp(header + 512, "EFI PART", 8) == 0);
  }

  if (!hasMBR && !hasGPT) {
    warningTitleLabel->setText("Missing partition table");
    warningMsgLabel->setText(
        "It looks like this is not a bootable image.\n\n"
        "The image does not appear to contain a partition table, "
        "and might not be recognized or bootable by your device.");
    warningPanel->show();
    warningPanel->raise();
    return false;
  }

  return true;
}

void MainWindow::slotWarningContinue() {
  warningPanel->hide();
  setStep(2);
}

void MainWindow::slotWarningCancel() {
  warningPanel->hide();
  selectedImage = "";
  setStep(1);
}

void MainWindow::slotDriveWarningContinue() {
  warningPanel->hide();
  // Restore normal image warning callbacks for next time
  btnWarningContinue->setClickCallback(
      [](void *w) { static_cast<MainWindow *>(w)->slotWarningContinue(); },
      this);
  btnWarningCancel->setClickCallback(
      [](void *w) { static_cast<MainWindow *>(w)->slotWarningCancel(); }, this);

  drivePanel->hide();
  setStep(3);
}

void MainWindow::slotDriveWarningCancel() {
  warningPanel->hide();
  // Restore normal image warning callbacks for next time
  btnWarningContinue->setClickCallback(
      [](void *w) { static_cast<MainWindow *>(w)->slotWarningContinue(); },
      this);
  btnWarningCancel->setClickCallback(
      [](void *w) { static_cast<MainWindow *>(w)->slotWarningCancel(); }, this);
  // Do not hide drivePanel, let user select another drive
}

void MainWindow::slotGenericErrorOk() { genericErrorPanel->hide(); }

void MainWindow::slotShowAboutDialog() {
  TQDialog *dlg = new TQDialog(this, "about", true);
  dlg->setCaption("About TdeFlasher");
  dlg->setMinimumWidth(350);

  TQVBoxLayout *layout = new TQVBoxLayout(dlg, 15, 10);

  TQLabel *lbl = new TQLabel("<br><p align='center'>A balenaEtcher clone in "
                             "tqt3 for Trinity Desktop<br>By Seb3773</p><br>",
                             dlg);
  lbl->setAlignment(TQt::AlignCenter);
  layout->addWidget(lbl);

  TQHBoxLayout *hlay = new TQHBoxLayout(layout);
  hlay->addStretch(1);
  TQPushButton *btn = new TQPushButton("OK", dlg);
  btn->setFixedWidth(80);
  dlg->connect(btn, SIGNAL(clicked()), dlg, SLOT(accept()));
  hlay->addWidget(btn);
  hlay->addStretch(1);

  dlg->exec();
  delete dlg;
}

void MainWindow::closeEvent(TQCloseEvent *e) {
  if (flashing) {
    int response = TQMessageBox::warning(
        this, "Attention",
        "<p><b>Are you sure you want to close TdeFlasher?</b></p>"
        "<p>You are currently flashing a drive. Closing TdeFlasher may leave "
        "your drive in an unusable state.</p>",
        "Yes, quit", "Annuler", TQString::null, 1, 1);

    if (response == 0) {
      // User clicked "Yes, quit", let window close
      e->accept();
    } else {
      // User clicked "Annuler", ignore close event
      e->ignore();
    }
  } else {
    // Normal close
    e->accept();
  }
}
