/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/fxtf/web-animations/#idl-def-Timing
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

enum SpacingMode { "distribute", "align" };

typedef (SpacingMode or double) EasingTimesInput;
//typedef (SpacingMode or sequence<double>) EasingTimesInput;

interface Timing {
  attribute double                             delay;
  attribute FillMode                           fill;
  attribute double                             iterationStart;
  attribute unrestricted double                iterations;
  attribute (unrestricted double or DOMString) duration;
  attribute (unrestricted double or DOMString) activeDuration;
  attribute double                             playbackRate;
  attribute PlaybackDirection                  direction;
  attribute DOMString                          easing;
  EasingTimesInput getEasingTimes();
  void             setEasingTimes(EasingTimesInput easingTimes);
};
