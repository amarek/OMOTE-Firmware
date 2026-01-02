#include "gui_configStatus.h"
#include <lvgl.h>
#include "guiBase.h"

static lv_obj_t* configStatusModal = nullptr;

static void configStatus_close_cb(lv_event_t* e) {
    configStatus_close();
}

void configStatus_close() {
    if (configStatusModal != nullptr) {
        lv_msgbox_close(configStatusModal);
        configStatusModal = nullptr;
    }
}

void configStatus_showDownloading() {
    configStatus_close();

    configStatusModal = lv_msgbox_create(NULL, "Config Download", "Downloading...", NULL, false);
    lv_obj_center(configStatusModal);
    lv_obj_set_width(configStatusModal, 200);
}

static std::string stageToString(config::ConfigError::Stage stage) {
    switch (stage) {
        case config::ConfigError::YAML_PARSE: return "YAML";
        case config::ConfigError::JSON_PARSE: return "JSON";
        case config::ConfigError::CONFIG_VALIDATION: return "Config";
        default: return "Error";
    }
}

void configStatus_showResult(const config::ConfigLoadResult& result) {
    configStatus_close();

    std::string message;

    if (result.success && result.errors.empty()) {
        message = "Config loaded successfully!";

        if (!result.warnings.empty()) {
            message += "\n\nWarnings:";
            for (const auto& warn : result.warnings) {
                message += "\n- ";
                if (warn.line > 0) {
                    message += "Line " + std::to_string(warn.line) + ": ";
                }
                message += warn.message;
            }
        }

        static const char* btns[] = {"OK", ""};
        configStatusModal = lv_msgbox_create(NULL, "Success", message.c_str(), btns, false);
    } else {
        message = "Config load failed!";

        if (!result.errors.empty()) {
            message += "\n\nErrors:";
            for (const auto& err : result.errors) {
                message += "\n[" + stageToString(err.stage) + "] ";
                if (err.line > 0) {
                    message += "Line " + std::to_string(err.line) + ": ";
                }
                message += err.message;
            }
        }

        if (!result.warnings.empty()) {
            message += "\n\nWarnings:";
            for (const auto& warn : result.warnings) {
                message += "\n- ";
                if (warn.line > 0) {
                    message += "Line " + std::to_string(warn.line) + ": ";
                }
                message += warn.message;
            }
        }

        static const char* btns[] = {"OK", ""};
        configStatusModal = lv_msgbox_create(NULL, "Error", message.c_str(), btns, false);
    }

    lv_obj_center(configStatusModal);
    lv_obj_set_width(configStatusModal, 220);
    lv_obj_add_event_cb(configStatusModal, configStatus_close_cb, LV_EVENT_VALUE_CHANGED, NULL);
}
