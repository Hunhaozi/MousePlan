QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
android: QT += androidextras

CONFIG += c++11

PRECOMPILED_HEADER =

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    appdata.cpp \
    modules/common/agreement/agreement_text_loader.cpp \
    modules/common/ui/ui_tuning.cpp \
    modules/common/ui/runtime_dialog_helpers.cpp \
    modules/common/ui/common_ui_pages.cpp \
    modules/ui/common/mainwindow_bootstrap.cpp \
    modules/ui/common/mainwindow_session_flow.cpp \
    modules/ui/profile/mainwindow_profile_actions.cpp \
    modules/ui/theme/mainwindow_theme_actions.cpp \
    modules/common/theme/theme_strategy_factory.cpp \
    modules/common/theme/theme_feature_gate.cpp \
    modules/common/update/update_client_helper.cpp \
    modules/themes/fitness/calendar/fitness_calendar_mark_builder.cpp \
    modules/themes/fitness/data/fitness_data_repository.cpp \
    modules/themes/fitness/network/fitness_online_api.cpp \
    modules/themes/fitness/plan/mainwindow_fitness_plan_manager.cpp \
    modules/themes/fitness/plan/fitness_plan_flow_helper.cpp \
    modules/themes/fitness/plan/fitness_plan_item_actions.cpp \
    modules/themes/fitness/plan/fitness_plan_runtime.cpp \
    modules/themes/fitness/plan/fitness_training_record_actions.cpp \
    modules/ui/login/login_register_flow.cpp \
    modules/ui/profile/profile_interaction_helper.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    appdata.h \
    modules/common/agreement/agreement_text_loader.h \
    modules/common/ui/ui_tuning.h \
    modules/common/ui/runtime_dialog_helpers.h \
    modules/common/theme/theme_strategy_factory.h \
    modules/common/config/network_config.h \
    modules/common/theme/theme_feature_gate.h \
    modules/common/update/update_client_helper.h \
    modules/themes/fitness/calendar/fitness_calendar_mark_builder.h \
    modules/themes/fitness/data/fitness_data_models.h \
    modules/themes/fitness/data/fitness_data_repository.h \
    modules/themes/fitness/network/fitness_online_api.h \
    modules/themes/fitness/plan/fitness_plan_flow_helper.h \
    modules/ui/login/login_register_flow.h \
    modules/ui/profile/profile_interaction_helper.h \
    mainwindow.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    android-sources/AndroidManifest.xml \
    android/AndroidManifest.xml \
    android/build.gradle \
    android/gradle/wrapper/gradle-wrapper.jar \
    android/gradle/wrapper/gradle-wrapper.properties \
    android/gradlew \
    android/gradlew.bat \
    android/res/values/libs.xml \
    config/agreement.md \
    config/theme.json \
    config/ui_config.json

contains(ANDROID_TARGET_ARCH,arm64-v8a) {
    ANDROID_PACKAGE_SOURCE_DIR = \
        $$PWD/android
}

RESOURCES += \
    img.qrc

# 添加头文件路径
INCLUDEPATH += $$PWD/modules/common/config
DEPENDPATH += $$PWD/modules/common/config
