/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/fxtf/web-animations/#idl-def-TimedItem
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface TimedItem : EventTarget {

    /*
    // Playback state
    readonly attribute double?             localTime;
    readonly attribute unsigned long?      currentIteration;

    */

    // Specified timing
    [Constant]
    readonly attribute Timing              specified;

    /*
    // Calculated timing
    readonly attribute double              startTime;
    readonly attribute unrestricted double iterationDuration;
    readonly attribute unrestricted double activeDuration;
    readonly attribute unrestricted double endTime;
    */

    // Timing hierarchy
    readonly attribute TimingGroup?        parent;
    readonly attribute TimedItem?          previousSibling;
    readonly attribute TimedItem?          nextSibling;
    /*void before (TimedItem... items);
    void after (TimedItem... items);
    void replace (TimedItem... items);
    void remove ();

    // Associated player
    readonly attribute Player?             player;

    // Event callbacks
             attribute EventHandler        onstart;
             attribute EventHandler        oniteration;
             attribute EventHandler        onend;
             attribute EventHandler        oncancel;

    */
};
