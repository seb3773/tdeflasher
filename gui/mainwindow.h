#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <cstdint>
#include <ntqmainwindow.h>
#include <ntqpixmap.h>
#include <ntqpushbutton.h>
#include <tqwidgetstack.h>

class TQLabel;
class TQTextEdit;
class TQPushButton;
class TQListView;
class TQTimer;
class TQListViewItem;
class TQLineEdit;

namespace flasher {
struct DriveInfo;
struct FlashProgress;
} // namespace flasher

// ---- Custom Progress Bar ----
class CustomProgressBar : public TQWidget {
  TQ_OBJECT
public:
  CustomProgressBar(TQWidget *parent = 0);
  void setProgress(int percent);

protected:
  void paintEvent(TQPaintEvent *);

private:
  int m_percent;
};

// ---- RoundButton ----
class RoundButton : public TQWidget {
  TQ_OBJECT
public:
  RoundButton(const TQString &text, const TQColor &bg, TQWidget *parent = 0);

  void setClickCallback(void (*cb)(void *), void *data) {
    m_callback = cb;
    m_callbackData = data;
  }
  void setBg(const TQColor &bg) {
    m_bgColor = bg;
    repaint();
  }
  void setText(const TQString &text) {
    m_text = text;
    repaint();
  }
  void setIcon(const TQPixmap &icon) {
    m_icon = icon;
    repaint();
  }
  void setEnabled(bool enabled) {
    m_enabled = enabled;
    repaint();
  }
  bool isEnabled() const { return m_enabled; }
  void setFlat(bool flat) {
    m_flat = flat;
    repaint();
  }
  void setTextOnly(bool textOnly) {
    m_textOnly = textOnly;
    repaint();
  }
  void setTextColor(const TQColor &color) {
    m_textColor = color;
    repaint();
  }
  void setHoverTextColor(const TQColor &color) {
    m_hoverTextColor = color;
    repaint();
  }

protected:
  void paintEvent(TQPaintEvent *);
  void enterEvent(TQEvent *) {
    m_hover = true;
    repaint();
  }
  void leaveEvent(TQEvent *) {
    m_hover = false;
    repaint();
  }
  void mousePressEvent(TQMouseEvent *e);
  void mouseReleaseEvent(TQMouseEvent *e);

private:
  TQString m_text;
  TQPixmap m_icon;
  TQColor m_bgColor;
  TQColor m_textColor;
  TQColor m_hoverTextColor;
  bool m_hover;
  bool m_enabled;
  bool m_flat;
  bool m_textOnly;
  void (*m_callback)(void *);
  void *m_callbackData;
};

class MainWindow : public TQMainWindow {
  TQ_OBJECT
public:
  MainWindow(TQWidget *parent = 0, const char *name = 0);
  ~MainWindow();

public slots:
  void slotFlashFromFile();
  void slotFlashFromUrl();
  void slotCloneDrive();
  void slotSelectTarget();
  void slotFlash();
  void slotCancelFlash();
  void slotPollProgress();
  void slotRefreshDrives();
  void slotRemoveImage();
  void slotFlashAnother();

  void slotDriveSelected(TQListViewItem *item);
  void slotDriveAccepted();
  void slotDriveCancelled();
  void slotShowImageInfo();
  void slotHideImageInfo();
  void slotToggleHiddenDrives();
  void slotErrorCancel();
  void slotErrorRetry();
  void slotWarningContinue();
  void slotWarningCancel();
  void slotDriveWarningContinue();
  void slotDriveWarningCancel();
  void slotGenericErrorOk();
  void slotShowAboutDialog();
  void slotUrlOk();
  void slotUrlCancel();
  void slotUrlToggleAuth();

protected:
  void closeEvent(TQCloseEvent *e) override;

private:
  void setStep(int step);
  void updateStepVisuals();
  void loadEmbeddedIcons();
  void showDriveSelector();
  bool validateImage(const TQString &path);

  // Step state
  int currentStep; // 1, 2, 3
  bool flashing;
  TQString selectedImage;
  TQString m_urlUsername;
  TQString m_urlPassword;
  bool m_selectingSourceDrive; // State to track if drive panel is for source
  uint64_t selectedImageSize;
  TQString m_selectedSourceDesc; // For Clone Drive mode
  TQString selectedDevice;
  TQString selectedDeviceDesc;
  TQString selectedDeviceSizeStr;

  // Embedded icons
  TQPixmap iconAdd, iconDrive, iconFlash, iconLogo;
  TQPixmap iconAddGray, iconDriveGray, iconFlashGray;
  TQPixmap iconFile, iconLink, iconCopy, iconTde;
  TQPixmap iconComplete, iconDot, iconHelp;

  TQWidget *mainPage;
  TQLabel *topLogoLabel, *topTitleLabel;
  TQLabel *iconLabel1, *iconLabel2, *iconLabel3;
  TQLabel *line1, *line2;
  RoundButton *btnFlashFile, *btnSelectTarget, *btnFlash;
  RoundButton *btnFlashUrl, *btnCloneDrive;
  RoundButton *btnChangeTarget, *btnTargetSize;
  TQLabel *stepLabel1, *stepLabel3;

  // Drive selector (overlay)
  TQWidget *drivePanel;
  TQLabel *drivePanelTitle;
  TQListView *driveList;
  RoundButton *btnDriveOk;
  TQPushButton *btnDriveCancel, *btnDriveRefresh;

  // Progress overlay (overhauled)
  TQWidget *progressPanel;
  TQWidget *progressMain;
  TQWidgetStack *progressStack;
  TQWidget *pageFlashing;
  TQWidget *pageFinished;

  // Flashing page components
  TQLabel *progImageLabel, *progDriveLabel;
  TQLabel *progFlashIcon;
  TQLabel *progPercentLabel;
  CustomProgressBar *progressBar;
  TQLabel *statusLabel, *speedLabel, *etaLabel;
  RoundButton *progCancelBtn;
  TQLabel *progLogoLabel;

  // Finished page components
  TQLabel *progFinishedIcon;
  TQLabel *progFinishedNameLabel;
  TQLabel *progFinishedTitleLabel;
  TQLabel *progFinishedStatusLabel;
  TQLabel *progFinishedSpeedLabel;
  RoundButton *btnFlashAnother;

  // Image info overlay
  TQWidget *imageInfoPanel;
  TQLabel *infoTitle, *infoName, *infoPath;
  RoundButton *btnInfoOk;

  // Drive selector additional
  TQWidget *warningBanner;
  TQLabel *warningLabel;
  TQPushButton *btnShowHidden;
  bool m_showHiddenDrives;
  int m_selectedDriveCount;

  // Worker timer
  TQTimer *progressTimer;

  // Error overlay
  TQWidget *errorPanel;
  TQLabel *errorTargetLabel, *errorLocationLabel, *errorMsgLabel;
  RoundButton *btnErrorCancel, *btnErrorRetry;

  // Warning overlay (partition table / Windows image)
  TQWidget *warningPanel;
  TQLabel *warningTitleLabel, *warningMsgLabel;
  RoundButton *btnWarningContinue, *btnWarningCancel;

  // Generic Error overlay (e.g. not enough space)
  TQWidget *genericErrorPanel;
  TQLabel *genericErrorTitleLabel, *genericErrorMsgLabel;
  RoundButton *btnGenericErrorOk;

  // URL Input overlay
  TQWidget *urlInputPanel;
  TQLineEdit *urlTxtUrl, *urlTxtUsername, *urlTxtPassword;
  TQWidget *urlAuthContainer;
  TQLabel *urlAuthLabel;
  bool m_urlAuthVisible;
  RoundButton *btnUrlOk;
};

#endif
