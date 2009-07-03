// Copyright 2007-2009 Google Inc.
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

// This is a description on UI elements for different states of the UI.
// We have only one dialog which changes between different UI states. Some
// controls are hidded, some controls are disabled, etc...

#ifndef OMAHA_WORKER_UI_CTLS_H__
#define OMAHA_WORKER_UI_CTLS_H__

#include "omaha/worker/ui.h"

namespace omaha {

const ProgressWnd::ControlState ProgressWnd::ctls_[] = {
    // The struct values are:
    // is_visible, is_enabled, is_button, is_default
  { IDC_PROGRESS,
    { { true,  true,  false, false },     // STATE_INIT
      { true,  true,  false, false },     // STATE_CHECKING_FOR_UPDATE
      { true,  true,  false, false },     // STATE_WAITING_TO_DOWNLOAD
      { true,  true,  false, false },     // STATE_DOWNLOADING
      { true,  true,  false, false },     // STATE_WAITING_TO_INSTALL
      { true,  true,  false, false },     // STATE_INSTALLING
      { true,  true,  false, false },     // STATE_PAUSED
      { false, false, false, false },     // STATE_COMPLETE_SUCCESS
      { false, false, false, false },     // STATE_COMPLETE_ERROR
      { false, false, false, false },     // STATE_COMPLETE_RESTART_BROWSER
      { false, false, false, false },     // STATE_COMPLETE_RESTART_ALL_BROWSERS
      { false, false, false, false },     // STATE_COMPLETE_REBOOT
      { false, false, false, false },     // STATE_END
    },
  },
  { IDC_PAUSE_RESUME_TEXT,
    { { false, false, false, false },     // STATE_INIT
      { false, false, false, false },     // STATE_CHECKING_FOR_UPDATE
      { false, false, false, false },     // STATE_WAITING_TO_DOWNLOAD
      { false,  true, false, false },     // STATE_DOWNLOADING
      { false, false, false, false },     // STATE_WAITING_TO_INSTALL
      { false, false, false, false },     // STATE_INSTALLING
      { false,  true, false, false },     // STATE_PAUSED
      { false, false, false, false },     // STATE_COMPLETE_SUCCESS
      { false, false, false, false },     // STATE_COMPLETE_ERROR
      { false, false, false, false },     // STATE_COMPLETE_RESTART_BROWSER
      { false, false, false, false },     // STATE_COMPLETE_RESTART_ALL_BROWSERS
      { false, false, false, false },     // STATE_COMPLETE_REBOOT
      { false, false, false, false },     // STATE_END
    },
  },
  { IDC_INFO_TEXT,
    { { false, false, false, false },     // STATE_INIT
      { false, false, false, false },     // STATE_CHECKING_FOR_UPDATE
      { false, false, false, false },     // STATE_WAITING_TO_DOWNLOAD
      { false, true,  false, false },     // STATE_DOWNLOADING
      { false, false, false, false },     // STATE_WAITING_TO_INSTALL
      { false, false, false, false },     // STATE_INSTALLING
      { false, false, false, false },     // STATE_PAUSED
      { false, false, false, false },     // STATE_COMPLETE_SUCCESS
      { false, false, false, false },     // STATE_COMPLETE_ERROR
      { false, false, false, false },     // STATE_COMPLETE_RESTART_BROWSER
      { false, false, false, false },     // STATE_COMPLETE_RESTART_ALL_BROWSERS
      { false, false, false, false },     // STATE_COMPLETE_REBOOT
      { false, false, false, false },     // STATE_END
    },
  },
  { IDC_INSTALLER_STATE_TEXT,
    { { true,  true,  false, false },     // STATE_INIT
      { true,  true,  false, false },     // STATE_CHECKING_FOR_UPDATE
      { true,  true,  false, false },     // STATE_WAITING_TO_DOWNLOAD
      { true,  true,  false, false },     // STATE_DOWNLOADING
      { true,  true,  false, false },     // STATE_WAITING_TO_INSTALL
      { true,  true,  false, false },     // STATE_INSTALLING
      { true,  true,  false, false },     // STATE_PAUSED
      { false, false, false, false },     // STATE_COMPLETE_SUCCESS
      { false, false, false, false },     // STATE_COMPLETE_ERROR
      { false, false, false, false },     // STATE_COMPLETE_RESTART_BROWSER
      { false, false, false, false },     // STATE_COMPLETE_RESTART_ALL_BROWSERS
      { false, false, false, false },     // STATE_COMPLETE_REBOOT
      { false, false, false, false },     // STATE_END
    },
  },
  { IDC_COMPLETE_TEXT,
    { { false, false, false, false },     // STATE_INIT
      { false, false, false, false },     // STATE_CHECKING_FOR_UPDATE
      { false, false, false, false },     // STATE_WAITING_TO_DOWNLOAD
      { false, false, false, false },     // STATE_DOWNLOADING
      { false, false, false, false },     // STATE_WAITING_TO_INSTALL
      { false, false, false, false },     // STATE_INSTALLING
      { false, false, false, false },     // STATE_PAUSED
      { true,  true,  false, false },     // STATE_COMPLETE_SUCCESS
      { false, false, false, false },     // STATE_COMPLETE_ERROR
      { true,  true,  false, false },     // STATE_COMPLETE_RESTART_BROWSER
      { true,  true,  false, false },     // STATE_COMPLETE_RESTART_ALL_BROWSERS
      { true,  true,  false, false },     // STATE_COMPLETE_REBOOT
      { false, false, false, false },     // STATE_END
    },
  },
  { IDC_ERROR_TEXT,
    { { false, false, false, false },     // STATE_INIT
      { false, false, false, false },     // STATE_CHECKING_FOR_UPDATE
      { false, false, false, false },     // STATE_WAITING_TO_DOWNLOAD
      { false, false, false, false },     // STATE_DOWNLOADING
      { false, false, false, false },     // STATE_WAITING_TO_INSTALL
      { false, false, false, false },     // STATE_INSTALLING
      { false, false, false, false },     // STATE_PAUSED
      { false, false, false, false },     // STATE_COMPLETE_SUCCESS
      { true,  true,  false, false },     // STATE_COMPLETE_ERROR
      { false, false, false, false },     // STATE_COMPLETE_RESTART_BROWSER
      { false, false, false, false },     // STATE_COMPLETE_RESTART_ALL_BROWSERS
      { false, false, false, false },     // STATE_COMPLETE_REBOOT
      { false, false, false, false },     // STATE_END
    },
  },
  { IDC_GET_HELP_TEXT,
    { { false, false, false, false },     // STATE_INIT
      { false, false, false, false },     // STATE_CHECKING_FOR_UPDATE
      { false, false, false, false },     // STATE_WAITING_TO_DOWNLOAD
      { false, false, false, false },     // STATE_DOWNLOADING
      { false, false, false, false },     // STATE_WAITING_TO_INSTALL
      { false, false, false, false },     // STATE_INSTALLING
      { false, false, false, false },     // STATE_PAUSED
      { false, false, false, false },     // STATE_COMPLETE_SUCCESS
      { true,  true,  false, false },     // STATE_COMPLETE_ERROR
      { false, false, false, false },     // STATE_COMPLETE_RESTART_BROWSER
      { false, false, false, false },     // STATE_COMPLETE_RESTART_ALL_BROWSERS
      { false, false, false, false },     // STATE_COMPLETE_REBOOT
      { false, false, false, false },     // STATE_END
    },
  },
  { IDC_BUTTON1,
    { { false, false, true, false },     // STATE_INIT
      { false, false, true, false },     // STATE_CHECKING_FOR_UPDATE
      { false, false, true, false },     // STATE_WAITING_TO_DOWNLOAD
      { false, false, true, false },     // STATE_DOWNLOADING
      { false, false, true, false },     // STATE_WAITING_TO_INSTALL
      { false, false, true, false },     // STATE_INSTALLING
      { false, false, true, false },     // STATE_PAUSED
      { false, false, true, false },     // STATE_COMPLETE_SUCCESS
      { false, false, true, false },     // STATE_COMPLETE_ERROR
      { true,  true,  true, true  },     // STATE_COMPLETE_RESTART_BROWSER
      { true,  true,  true, true  },     // STATE_COMPLETE_RESTART_ALL_BROWSERS
      { true,  true,  true, true  },     // STATE_COMPLETE_REBOOT
      { false, false, true, false },     // STATE_END
    },
  },
  { IDC_BUTTON2,
    { { false, false, true, false },     // STATE_INIT
      { false, false, true, false },     // STATE_CHECKING_FOR_UPDATE
      { false, false, true, false },     // STATE_WAITING_TO_DOWNLOAD
      { false, false, true, false },     // STATE_DOWNLOADING
      { false, false, true, false },     // STATE_WAITING_TO_INSTALL
      { false, false, true, false },     // STATE_INSTALLING
      { false, false, true, false },     // STATE_PAUSED
      { false, false, true, false },     // STATE_COMPLETE_SUCCESS
      { false, false, true, false },     // STATE_COMPLETE_ERROR
      { true,  true,  true, false },     // STATE_COMPLETE_RESTART_BROWSER
      { true,  true,  true, false },     // STATE_COMPLETE_RESTART_ALL_BROWSERS
      { true,  true,  true, false },     // STATE_COMPLETE_REBOOT
      { false, false, true, false },     // STATE_END
    },
  },
  { IDC_CLOSE,
    { { false, false, true, false },     // STATE_INIT
      { false, false, true, false },     // STATE_CHECKING_FOR_UPDATE
      { false, false, true, false },     // STATE_WAITING_TO_DOWNLOAD
      { false, false, true, false },     // STATE_DOWNLOADING
      { false, false, true, false },     // STATE_WAITING_TO_INSTALL
      { false, false, true, false },     // STATE_INSTALLING
      { false, false, true, false },     // STATE_PAUSED
      { true,  true,  true, true  },     // STATE_COMPLETE_SUCCESS
      { true,  true,  true, true  },     // STATE_COMPLETE_ERROR
      { false, false, true, false },     // STATE_COMPLETE_RESTART_BROWSER
      { false, false, true, false },     // STATE_COMPLETE_RESTART_ALL_BROWSERS
      { false, false, true, false },     // STATE_COMPLETE_REBOOT
      { false, false, true, false },     // STATE_END
    },
  },
  { IDC_IMAGE,
    { { false, false, false, false },     // STATE_INIT
      { false, false, false, false },     // STATE_CHECKING_FOR_UPDATE
      { false, false, false, false },     // STATE_WAITING_TO_DOWNLOAD
      { false, false, false, false },     // STATE_DOWNLOADING
      { false, false, false, false },     // STATE_WAITING_TO_INSTALL
      { false, false, false, false },     // STATE_INSTALLING
      { false, false, false, false },     // STATE_PAUSED
      { true,  false, false, false },     // STATE_COMPLETE_SUCCESS
      { false, false, false, false },     // STATE_COMPLETE_ERROR
      { true,  false, false, false },     // STATE_COMPLETE_RESTART_BROWSER
      { true,  false, false, false },     // STATE_COMPLETE_RESTART_ALL_BROWSERS
      { true,  false, false, false },     // STATE_COMPLETE_REBOOT
      { false, false, false, false },     // STATE_END
    },
  },
};

}  // namespace omaha

#endif  // OMAHA_WORKER_UI_CTLS_H__

