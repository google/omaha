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

#ifndef OMAHA_UI_UI_CTLS_H__
#define OMAHA_UI_UI_CTLS_H__

#include "omaha/ui/progress_wnd.h"

namespace omaha {

const ProgressWnd::ControlState ProgressWnd::ctls_[] = {
    // The struct values are:
    // is_ignore_entry, is_visible, is_enabled, is_button, is_default
  { IDC_PROGRESS,
    { { false, false, false, false, false },  // STATE_INIT
      { false, false, false, false, false },  // STATE_CHECKING_FOR_UPDATE
      { false, false, false, false, false },  // STATE_WAITING_TO_DOWNLOAD
      { true,  false, false, false, false },  // STATE_DOWNLOADING
      { false, false, false, false, false },  // STATE_WAITING_TO_INSTALL
      { false, false, false, false, false },  // STATE_INSTALLING
      { false, false, false, false, false },  // STATE_PAUSED
      { false, false, false, false, false },  // STATE_COMPLETE_SUCCESS
      { false, false, false, false, false },  // STATE_COMPLETE_ERROR
      { false, false, false, false, false },  // STATE_COMPLETE_RESTART_BROWSER
      { false, false, false, false, false },  // COMPLETE_RESTART_ALL_BROWSERS
      { false, false, false, false, false },  // STATE_COMPLETE_REBOOT
      { false, false, false, false, false },  // STATE_END
    },
  },
  { IDC_MARQUEE,
    { { false, true,  false, false, false },  // STATE_INIT
      { false, true,  false, false, false },  // STATE_CHECKING_FOR_UPDATE
      { false, true,  false, false, false },  // STATE_WAITING_TO_DOWNLOAD
      { true,  false, false, false, false },  // STATE_DOWNLOADING
      { false, true,  false, false, false },  // STATE_WAITING_TO_INSTALL
      { true,  false, false, false, false },  // STATE_INSTALLING
      { false, false, false, false, false },  // STATE_PAUSED
      { false, false, false, false, false },  // STATE_COMPLETE_SUCCESS
      { false, false, false, false, false },  // STATE_COMPLETE_ERROR
      { false, false, false, false, false },  // STATE_COMPLETE_RESTART_BROWSER
      { false, false, false, false, false },  // COMPLETE_RESTART_ALL_BROWSERS
      { false, false, false, false, false },  // STATE_COMPLETE_REBOOT
      { false, false, false, false, false },  // STATE_END
    },
  },
  { IDC_PAUSE_RESUME_TEXT,
    { { false, false, false, false, false },  // STATE_INIT
      { false, false, false, false, false },  // STATE_CHECKING_FOR_UPDATE
      { false, false, false, false, false },  // STATE_WAITING_TO_DOWNLOAD
      { false, false, true,  false, false },  // STATE_DOWNLOADING
      { false, false, false, false, false },  // STATE_WAITING_TO_INSTALL
      { false, false, false, false, false },  // STATE_INSTALLING
      { false, false, true,  false, false },  // STATE_PAUSED
      { false, false, false, false, false },  // STATE_COMPLETE_SUCCESS
      { false, false, false, false, false },  // STATE_COMPLETE_ERROR
      { false, false, false, false, false },  // STATE_COMPLETE_RESTART_BROWSER
      { false, false, false, false, false },  // COMPLETE_RESTART_ALL_BROWSERS
      { false, false, false, false, false },  // STATE_COMPLETE_REBOOT
      { false, false, false, false, false },  // STATE_END
    },
  },
  { IDC_INFO_TEXT,
    { { false, false, false, false, false },  // STATE_INIT
      { false, false, false, false, false },  // STATE_CHECKING_FOR_UPDATE
      { false, false, false, false, false },  // STATE_WAITING_TO_DOWNLOAD
      { false, false, true,  false, false },  // STATE_DOWNLOADING
      { false, false, false, false, false },  // STATE_WAITING_TO_INSTALL
      { false, false, false, false, false },  // STATE_INSTALLING
      { false, false, false, false, false },  // STATE_PAUSED
      { false, false, false, false, false },  // STATE_COMPLETE_SUCCESS
      { false, false, false, false, false },  // STATE_COMPLETE_ERROR
      { false, false, false, false, false },  // STATE_COMPLETE_RESTART_BROWSER
      { false, false, false, false, false },  // COMPLETE_RESTART_ALL_BROWSERS
      { false, false, false, false, false },  // STATE_COMPLETE_REBOOT
      { false, false, false, false, false },  // STATE_END
    },
  },
  { IDC_INSTALLER_STATE_TEXT,
    { { false, true,  true,  false, false },  // STATE_INIT
      { false, true,  true,  false, false },  // STATE_CHECKING_FOR_UPDATE
      { false, true,  true,  false, false },  // STATE_WAITING_TO_DOWNLOAD
      { false, true,  true,  false, false },  // STATE_DOWNLOADING
      { false, true,  true,  false, false },  // STATE_WAITING_TO_INSTALL
      { false, true,  true,  false, false },  // STATE_INSTALLING
      { false, true,  true,  false, false },  // STATE_PAUSED
      { false, false, false, false, false },  // STATE_COMPLETE_SUCCESS
      { false, false, false, false, false },  // STATE_COMPLETE_ERROR
      { false, false, false, false, false },  // STATE_COMPLETE_RESTART_BROWSER
      { false, false, false, false, false },  // COMPLETE_RESTART_ALL_BROWSERS
      { false, false, false, false, false },  // STATE_COMPLETE_REBOOT
      { false, false, false, false, false },  // STATE_END
    },
  },
  { IDC_COMPLETE_TEXT,
    { { false, false, false, false, false },  // STATE_INIT
      { false, false, false, false, false },  // STATE_CHECKING_FOR_UPDATE
      { false, false, false, false, false },  // STATE_WAITING_TO_DOWNLOAD
      { false, false, false, false, false },  // STATE_DOWNLOADING
      { false, false, false, false, false },  // STATE_WAITING_TO_INSTALL
      { false, false, false, false, false },  // STATE_INSTALLING
      { false, false, false, false, false },  // STATE_PAUSED
      { false, true,  true,  false, false },  // STATE_COMPLETE_SUCCESS
      { false, false, false, false, false },  // STATE_COMPLETE_ERROR
      { false, true,  true,  false, false },  // STATE_COMPLETE_RESTART_BROWSER
      { false, true,  true,  false, false },  // COMPLETE_RESTART_ALL_BROWSERS
      { false, true,  true,  false, false },  // STATE_COMPLETE_REBOOT
      { false, false, false, false, false },  // STATE_END
    },
  },
  { IDC_ERROR_TEXT,
    { { false, false, false, false, false },  // STATE_INIT
      { false, false, false, false, false },  // STATE_CHECKING_FOR_UPDATE
      { false, false, false, false, false },  // STATE_WAITING_TO_DOWNLOAD
      { false, false, false, false, false },  // STATE_DOWNLOADING
      { false, false, false, false, false },  // STATE_WAITING_TO_INSTALL
      { false, false, false, false, false },  // STATE_INSTALLING
      { false, false, false, false, false },  // STATE_PAUSED
      { false, false, false, false, false },  // STATE_COMPLETE_SUCCESS
      { false, true,  true,  false, false },  // STATE_COMPLETE_ERROR
      { false, false, false, false, false },  // STATE_COMPLETE_RESTART_BROWSER
      { false, false, false, false, false },  // COMPLETE_RESTART_ALL_BROWSERS
      { false, false, false, false, false },  // STATE_COMPLETE_REBOOT
      { false, false, false, false, false },  // STATE_END
    },
  },
  { IDC_ERROR_ILLUSTRATION,
    { { false, false, false, false, false },  // STATE_INIT
      { false, false, false, false, false },  // STATE_CHECKING_FOR_UPDATE
      { false, false, false, false, false },  // STATE_WAITING_TO_DOWNLOAD
      { false, false, false, false, false },  // STATE_DOWNLOADING
      { false, false, false, false, false },  // STATE_WAITING_TO_INSTALL
      { false, false, false, false, false },  // STATE_INSTALLING
      { false, false, false, false, false },  // STATE_PAUSED
      { false, false, false, false, false },  // STATE_COMPLETE_SUCCESS
      { false, true,  true,  false, false },  // STATE_COMPLETE_ERROR
      { false, false, false, false, false },  // STATE_COMPLETE_RESTART_BROWSER
      { false, false, false, false, false },  // COMPLETE_RESTART_ALL_BROWSERS
      { false, false, false, false, false },  // STATE_COMPLETE_REBOOT
      { false, false, false, false, false },  // STATE_END
    },
  },
  { IDC_GET_HELP,
    { { false, false, false, true, false },  // STATE_INIT
      { false, false, false, true, false },  // STATE_CHECKING_FOR_UPDATE
      { false, false, false, true, false },  // STATE_WAITING_TO_DOWNLOAD
      { false, false, false, true, false },  // STATE_DOWNLOADING
      { false, false, false, true, false },  // STATE_WAITING_TO_INSTALL
      { false, false, false, true, false },  // STATE_INSTALLING
      { false, false, false, true, false },  // STATE_PAUSED
      { false, false, false, true, false },  // STATE_COMPLETE_SUCCESS
      { false, true,  true,  true, false },  // STATE_COMPLETE_ERROR
      { false, false, false, true, false },  // STATE_COMPLETE_RESTART_BROWSER
      { false, false, false, true, false },  // COMPLETE_RESTART_ALL_BROWSERS
      { false, false, false, true, false },  // STATE_COMPLETE_REBOOT
      { false, false, false, true, false },  // STATE_END
    },
  },
  { IDC_BUTTON1,
    { { false, false, false, true, false },  // STATE_INIT
      { false, false, false, true, false },  // STATE_CHECKING_FOR_UPDATE
      { false, false, false, true, false },  // STATE_WAITING_TO_DOWNLOAD
      { false, false, false, true, false },  // STATE_DOWNLOADING
      { false, false, false, true, false },  // STATE_WAITING_TO_INSTALL
      { false, false, false, true, false },  // STATE_INSTALLING
      { false, false, false, true, false },  // STATE_PAUSED
      { false, false, false, true, false },  // STATE_COMPLETE_SUCCESS
      { false, false, false, true, false },  // STATE_COMPLETE_ERROR
      { false, true,  true,  true, true  },  // STATE_COMPLETE_RESTART_BROWSER
      { false, true,  true,  true, true  },  // COMPLETE_RESTART_ALL_BROWSERS
      { false, true,  true,  true, true  },  // STATE_COMPLETE_REBOOT
      { false, false, false, true, false },  // STATE_END
    },
  },
  { IDC_BUTTON2,
    { { false, false, false, true, false },  // STATE_INIT
      { false, false, false, true, false },  // STATE_CHECKING_FOR_UPDATE
      { false, false, false, true, false },  // STATE_WAITING_TO_DOWNLOAD
      { false, false, false, true, false },  // STATE_DOWNLOADING
      { false, false, false, true, false },  // STATE_WAITING_TO_INSTALL
      { false, false, false, true, false },  // STATE_INSTALLING
      { false, false, false, true, false },  // STATE_PAUSED
      { false, false, false, true, false },  // STATE_COMPLETE_SUCCESS
      { false, false, false, true, false },  // STATE_COMPLETE_ERROR
      { false, true,  true,  true, false },  // STATE_COMPLETE_RESTART_BROWSER
      { false, true,  true,  true, false },  // COMPLETE_RESTART_ALL_BROWSERS
      { false, true,  true,  true, false },  // STATE_COMPLETE_REBOOT
      { false, false, false, true, false },  // STATE_END
    },
  },
  { IDC_CLOSE,
    { { false, false, false, true, false },  // STATE_INIT
      { false, false, false, true, false },  // STATE_CHECKING_FOR_UPDATE
      { false, false, false, true, false },  // STATE_WAITING_TO_DOWNLOAD
      { false, false, false, true, false },  // STATE_DOWNLOADING
      { false, false, false, true, false },  // STATE_WAITING_TO_INSTALL
      { false, false, false, true, false },  // STATE_INSTALLING
      { false, false, false, true, false },  // STATE_PAUSED
      { false, true,  true,  true, true  },  // STATE_COMPLETE_SUCCESS
      { false, true,  true,  true, true  },  // STATE_COMPLETE_ERROR
      { false, false, false, true, false },  // STATE_COMPLETE_RESTART_BROWSER
      { false, false, false, true, false },  // COMPLETE_RESTART_ALL_BROWSERS
      { false, false, false, true, false },  // STATE_COMPLETE_REBOOT
      { false, false, false, true, false },  // STATE_END
    },
  },
  { IDC_APP_BITMAP,
    { { false, true,  false, false, false },  // STATE_INIT
      { false, true,  false, false, false },  // STATE_CHECKING_FOR_UPDATE
      { false, true,  false, false, false },  // STATE_WAITING_TO_DOWNLOAD
      { false, true,  false, false, false },  // STATE_DOWNLOADING
      { false, true,  false, false, false },  // STATE_WAITING_TO_INSTALL
      { false, true,  false, false, false },  // STATE_INSTALLING
      { false, true,  false, false, false },  // STATE_PAUSED
      { false, true,  false, false, false },  // STATE_COMPLETE_SUCCESS
      { false, false, false, false, false },  // STATE_COMPLETE_ERROR
      { false, true,  false, false, false },  // STATE_COMPLETE_RESTART_BROWSER
      { false, true,  false, false, false },  // COMPLETE_RESTART_ALL_BROWSERS
      { false, true,  false, false, false },  // STATE_COMPLETE_REBOOT
      { false, true,  false, false, false },  // STATE_END
    },
  },
};

}  // namespace omaha

#endif  // OMAHA_UI_UI_CTLS_H__

