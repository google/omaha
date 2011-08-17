// Copyright 2008-2009 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ========================================================================

// Declares the usage metrics used by the worker module.

#ifndef OMAHA_UI_UI_METRICS_H__
#define OMAHA_UI_UI_METRICS_H__

#include "omaha/statsreport/metrics.h"

namespace omaha {

// Time (ms) until the user canceled.
DECLARE_METRIC_timing(worker_ui_cancel_ms);
// How many times the user canceled. Only includes confirmed cancels.
DECLARE_METRIC_count(worker_ui_cancels);

// How many times the user has clicked on the x button of the UI.
DECLARE_METRIC_count(worker_ui_click_x);

// How many times the user has hit the esc key.
DECLARE_METRIC_count(worker_ui_esc_key_total);

// How many times the "Restart Browser Now/Later" buttons were displayed.
DECLARE_METRIC_count(worker_ui_restart_browser_buttons_displayed);
// How many times the user clicked "Restart Browser Now".
DECLARE_METRIC_count(worker_ui_restart_browser_now_click);
// How many times the "Restart Browsers Now/Later" buttons were displayed.
DECLARE_METRIC_count(worker_ui_restart_all_browsers_buttons_displayed);
// How many times the user clicked "Restart Browsers Now".
DECLARE_METRIC_count(worker_ui_restart_all_browsers_now_click);
// How many times the "Restart Now/Later" (reboot) buttons were displayed.
DECLARE_METRIC_count(worker_ui_reboot_buttons_displayed);
// How many times the user clicked "Restart Now" (reboot).
DECLARE_METRIC_count(worker_ui_reboot_now_click);

// How many times the user was offered the "Help Me Fix This" link.
DECLARE_METRIC_count(worker_ui_get_help_displayed);
// How many times the user clicked the "Help Me Fix This" link.
DECLARE_METRIC_count(worker_ui_get_help_click);

}  // namespace omaha

#endif  // OMAHA_UI_UI_METRICS_H__
