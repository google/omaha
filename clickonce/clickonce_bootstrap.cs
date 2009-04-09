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

// C# stub that allows a ClickOnce install of an Omaha application.

using System;
using System.Deployment.Application;
using System.Diagnostics;
using System.IO;
using System.Security;
using System.Security.Permissions;
using System.Windows.Forms;
using System.Web;

namespace ClickOnceBootstrap {
  static class ClickOnceEntry {
    [STAThread]
    static void Main() {
      try {
        // Try to get FullTrust. Will throw if we cannot get it.
        new PermissionSet(PermissionState.Unrestricted).Demand();

        if (!ApplicationDeployment.IsNetworkDeployed) {
          // Only support running via ClickOnce.
          return;
        }

        string query_string =
            ApplicationDeployment.CurrentDeployment.ActivationUri.Query;
        if (query_string.Length < 2) {
          // Query string will be of the form "?xyz=abc". Should have atleast
          // a question mark and atleast a single character to qualify as a
          // valid query string. Hence the check against "2".
          return;
        }
        // Remove the '?' prefix.
        query_string = query_string.Substring(1);
        query_string = HttpUtility.UrlDecode(query_string);
        string setup_path = Path.Combine(Application.StartupPath,
                                          "GoogleUpdateSetup.exe");

        ProcessStartInfo psi = new ProcessStartInfo();
        psi.FileName = setup_path;
        psi.Verb = "open";
        psi.Arguments = "/installsource clickonce /install \"";
        psi.Arguments += query_string;
        psi.Arguments += "\"";
        Process.Start(psi);
      } catch(Exception e) {
        MessageBox.Show(e.ToString());
        return;
      }

      return;
    }
  }
}

