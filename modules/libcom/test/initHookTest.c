/*************************************************************************\
* Copyright (c) 2021 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdlib.h>
#include <string.h>

#include <testMain.h>
#include <epicsUnitTest.h>

#include <initHooks.h>

static
void testHookNames(void)
{
    const char* s;

    s = initHookName(initHookAfterNtpTimeSync);
    testOk(strcmp(s, "initHookAfterNtpTimeSync")==0, "'%s' == 'initHookAfterNtpTimeSync'", s);

    s = initHookName(initHookAtEnd);
    testOk(strcmp(s, "initHookAtEnd")==0, "'%s' == 'initHookAtEnd'", s);
}

MAIN(initHookTest)
{
    testPlan(2);
    testHookNames();
    return testDone();
}
