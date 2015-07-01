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


#include "omaha/ui/ui_metrics.h"

namespace omaha {

DEFINE_METRIC_timing(worker_ui_cancel_ms);
DEFINE_METRIC_count(worker_ui_cancels);

DEFINE_METRIC_count(worker_ui_click_x);

DEFINE_METRIC_count(worker_ui_esc_key_total);

DEFINE_METRIC_count(worker_ui_restart_browser_buttons_displayed);
DEFINE_METRIC_count(worker_ui_restart_browser_now_click);
DEFINE_METRIC_count(worker_ui_restart_all_browsers_buttons_displayed);
DEFINE_METRIC_count(worker_ui_restart_all_browsers_now_click);
DEFINE_METRIC_count(worker_ui_reboot_buttons_displayed);
DEFINE_METRIC_count(worker_ui_reboot_now_click);

DEFINE_METRIC_count(worker_ui_get_help_displayed);
DEFINE_METRIC_count(worker_ui_get_help_click);


}  // namespace omaha
