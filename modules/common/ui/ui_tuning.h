#ifndef MOUSEPLAN_UI_TUNING_H
#define MOUSEPLAN_UI_TUNING_H

#include <QString>

struct UiTuning {
    int dialogBaseFont = 20;
    int dialogControlHeight = 56;
    int dialogButtonHeight = 60;
    int dialogCornerRadius = 14;
    int dialogListItemHeight = 76;
    int planManagerListFont = 24;
    int planManagerActionFont = 24;
    int planManagerActionHeight = 78;
    int dialogTopRatio = 1;
    int dialogContentRatio = 5;
    int dialogBottomRatio = 1;

    int loginLogoMinFont = 46;
    int loginCaptionMinFont = 18;
    int loginInputMinFont = 18;
    int loginButtonMinFont = 24;
    int loginLinkMinFont = 20;
    int loginCardMinWidthPortrait = 940;

    int mainBodyMinFont = 18;
    int mainTopRatio = 1;
    int mainContentRatio = 12;
    int mainBottomRatio = 1;
    int calendarDayMinFont = 22;
    int calendarNavMinFont = 20;
    int navTextMinFont = 20;
    int navIconMinSize = 44;
    int mainActionButtonMinFont = 24;
    int mainActionButtonMinHeight = 72;
    int planItemTitleMinFont = 24;
    int planItemMetaMinFont = 18;
    int planItemButtonMinFont = 18;
    int planItemMinHeight = 200;

    QString dialogBg = "#f5f7f4";
};

extern UiTuning gUiTuning;

#endif