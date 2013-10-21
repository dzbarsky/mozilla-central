/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/fxtf/web-animations/#idl-def-TimingInput
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

enum FillMode { "none", "forwards", "backwards", "both", "auto" };

enum PlaybackDirection { "normal", "reverse", "alternate", "alternate-reverse" };

dictionary TimingInput {
  double                             delay = 0;
  FillMode                           fill = "auto";
  double                             iterationStart = 0.0;
  unrestricted double                iterations = 1.0;
  unrestricted double duration = 1.0;
  unrestricted double activeDuration = 1.0;
  //(unrestricted double or DOMString) duration = "auto";
  //(unrestricted double or DOMString) activeDuration = "auto";
  double                             playbackRate = 1.0;
  PlaybackDirection                  direction = "normal";
  DOMString                          easing = "linear";
  //EasingTimesInput easingTimes = "distribute";
};
