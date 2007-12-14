/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if HAVE_NVPERFSDK_H

#include <stdbool.h>
#include "src/stats.h"
#include "src/utils.h"
#include "src/filters.h"
#include "src/objects.h"
#include "src/tracker.h"
#include "src/log.h"
#include "src/glutils.h"
#include <NVPerfSDK.h>
#include <ltdl.h>
#include <stdlib.h>
#include <string.h>
#include "xalloc.h"
#include "xvasprintf.h"

typedef struct
{
    UINT index;
    bool use_cycles;
    bool accumulate;
    bool experiment;
    char *name;
} stats_signal_nv;

static linked_list stats_nv_active;
static linked_list stats_nv_registered;
static lt_dlhandle stats_nv_dl = NULL;
static bool stats_nv_experiment_mode = false; /* True if using simplified experiments */
static int stats_nv_num_passes = -1, stats_nv_pass = -1;
static bool stats_nv_flush = false;

static NVPMRESULT (*fNVPMInit)(void);
static NVPMRESULT (*fNVPMShutdown)(void);
static NVPMRESULT (*fNVPMEnumCounters)(NVPMEnumFunc);
static NVPMRESULT (*fNVPMSample)(NVPMSampleValue *, UINT *);
static NVPMRESULT (*fNVPMBeginExperiment)(int *);
static NVPMRESULT (*fNVPMEndExperiment)(void);
static NVPMRESULT (*fNVPMBeginPass)(int);
static NVPMRESULT (*fNVPMEndPass)(int);
static NVPMRESULT (*fNVPMAddCounter)(UINT);
static NVPMRESULT (*fNVPMRemoveAllCounters)(void);
static NVPMRESULT (*fNVPMGetCounterValue)(UINT, int, UINT64 *, UINT64 *);
static NVPMRESULT (*fNVPMGetCounterAttribute)(UINT, NVPMATTRIBUTE, UINT64 *);

static NVPMRESULT check_nvpm(NVPMRESULT status, const char *file, int line)
{
    if (status != NVPM_OK)
    {
        bugle_log_printf("stats_nv", "nvpm", BUGLE_LOG_ERROR,
                         "NVPM error %d at %s:%d",
                         (int) status, file, line);
        exit(1);
    }
    return status;
}

#define CHECK_NVPM(x) (check_nvpm((x), __FILE__, __LINE__))

static bool stats_nv_glXSwapBuffers(function_call *call, const callback_data *data)
{
    linked_list_node *i;
    stats_signal *si;
    stats_signal_nv *nv;
    UINT samples;
    UINT64 value, cycles;
    bool experiment_done = false;

    if (bugle_list_head(&stats_nv_active))
    {
        if (stats_nv_experiment_mode)
        {
            if (stats_nv_pass >= 0)
            {
                CHECK_NVPM(fNVPMEndPass(stats_nv_pass));
                if (stats_nv_flush) CALL_glFinish();
                if (stats_nv_pass + 1 == stats_nv_num_passes)
                {
                    CHECK_NVPM(fNVPMEndExperiment());
                    experiment_done = true;
                    stats_nv_pass = -1;    /* tag as not-in-experiment */
                }
            }
        }

        if (stats_nv_flush) CALL_glFinish();
        CHECK_NVPM(fNVPMSample(NULL, &samples));
        for (i = bugle_list_head(&stats_nv_active); i; i = bugle_list_next(i))
        {
            si = (stats_signal *) bugle_list_data(i);
            nv = (stats_signal_nv *) si->user_data;
            if (nv->experiment && !experiment_done) continue;

            CHECK_NVPM(fNVPMGetCounterValue(nv->index, 0, &value, &cycles));
            if (nv->use_cycles) value = cycles;
            if (nv->accumulate) bugle_stats_signal_add(si, value);
            else bugle_stats_signal_update(si, value);
        }
    }
    return true;
}

static bool stats_nv_post_glXSwapBuffers(function_call *call, const callback_data *data)
{
    if (stats_nv_experiment_mode)
    {
        stats_nv_pass++;
        if (stats_nv_pass == 0)
            CHECK_NVPM(fNVPMBeginExperiment(&stats_nv_num_passes));
        CHECK_NVPM(fNVPMBeginPass(stats_nv_pass));
    }
    return true;
}

static bool stats_nv_signal_activate(stats_signal *st)
{
    stats_signal_nv *nv;

    nv = (stats_signal_nv *) st->user_data;
    if (fNVPMAddCounter(nv->index) != NVPM_OK)
    {
        bugle_log_printf("stats_nv", "nvpm", BUGLE_LOG_ERROR,
                         "failed to add NVPM counter %s", nv->name);
        return false;
    }
    if (nv->experiment) stats_nv_experiment_mode = true;
    bugle_list_append(&stats_nv_active, st);
    return true;
}

static int stats_nv_enumerate(UINT index, char *name)
{
    char *stat_name;
    stats_signal *si;
    stats_signal_nv *nv;
    UINT64 counter_type;
    int accum, cycles;

    fNVPMGetCounterAttribute(index, NVPMA_COUNTER_TYPE, &counter_type);
    for (accum = 0; accum < 2; accum++)
        for (cycles = 0; cycles < 2; cycles++)
        {
            stat_name = xasprintf("nv%s%s:%s",
                                  accum ? ":accum" : "",
                                  cycles ? ":cycles" : "", name);
            nv = XMALLOC(stats_signal_nv);
            nv->index = index;
            nv->accumulate = (accum == 1);
            nv->use_cycles = (cycles == 1);
            nv->experiment = (counter_type == NVPM_CT_SIMEXP);
            nv->name = xstrdup(name);
            si = bugle_stats_signal_register(stat_name, nv,
                                             stats_nv_signal_activate);
            bugle_list_append(&stats_nv_registered, si);
            free(stat_name);
        }
    return NVPM_OK;
}

static void stats_nv_free(stats_signal *si)
{
    stats_signal_nv *nv;

    nv = (stats_signal_nv *) si->user_data;
    free(nv->name);
    free(nv);
}

static bool stats_nv_initialise(filter_set *handle)
{
    filter *f;

    bugle_list_init(&stats_nv_active, NULL);
    bugle_list_init(&stats_nv_registered, (void (*)(void *)) stats_nv_free);
    stats_nv_dl = lt_dlopenext("libNVPerfSDK");
    if (stats_nv_dl == NULL) return true;

    fNVPMInit = (NVPMRESULT (*)(void)) lt_dlsym(stats_nv_dl, "NVPMInit");
    fNVPMShutdown = (NVPMRESULT (*)(void)) lt_dlsym(stats_nv_dl, "NVPMShutdown");
    fNVPMEnumCounters = (NVPMRESULT (*)(NVPMEnumFunc)) lt_dlsym(stats_nv_dl, "NVPMEnumCounters");
    fNVPMSample = (NVPMRESULT (*)(NVPMSampleValue *, UINT *)) lt_dlsym(stats_nv_dl, "NVPMSample");
    fNVPMBeginExperiment = (NVPMRESULT (*)(int *)) lt_dlsym(stats_nv_dl, "NVPMBeginExperiment");
    fNVPMEndExperiment = (NVPMRESULT (*)(void)) lt_dlsym(stats_nv_dl, "NVPMEndExperiment");
    fNVPMBeginPass = (NVPMRESULT (*)(int)) lt_dlsym(stats_nv_dl, "NVPMBeginPass");
    fNVPMEndPass = (NVPMRESULT (*)(int)) lt_dlsym(stats_nv_dl, "NVPMEndPass");
    fNVPMRemoveAllCounters = (NVPMRESULT (*)(void)) lt_dlsym(stats_nv_dl, "NVPMRemoveAllCounters");
    fNVPMAddCounter = (NVPMRESULT (*)(UINT)) lt_dlsym(stats_nv_dl, "NVPMAddCounter");
    fNVPMGetCounterValue = (NVPMRESULT (*)(UINT, int, UINT64 *, UINT64 *)) lt_dlsym(stats_nv_dl, "NVPMGetCounterValue");
    fNVPMGetCounterAttribute = (NVPMRESULT (*)(UINT, NVPMATTRIBUTE, UINT64 *)) lt_dlsym(stats_nv_dl, "NVPMGetCounterAttribute");

    if (!fNVPMInit
        || !fNVPMShutdown
        || !fNVPMEnumCounters
        || !fNVPMSample
        || !fNVPMBeginExperiment || !fNVPMEndExperiment
        || !fNVPMBeginPass || !fNVPMEndPass
        || !fNVPMAddCounter || !fNVPMRemoveAllCounters
        || !fNVPMGetCounterValue
        || !fNVPMGetCounterAttribute)
    {
        bugle_log("stats_nv", "ltdl", BUGLE_LOG_ERROR,
                  "Failed to load symbols for NVPerfSDK");
        goto cancel1;
    }

    if (fNVPMInit() != NVPM_OK)
    {
        bugle_log("stats_nv", "nvpm", BUGLE_LOG_ERROR,
                  "Failed to initialise NVPM");
        goto cancel1;
    }
    if (fNVPMEnumCounters(stats_nv_enumerate) != NVPM_OK)
    {
        bugle_log("stats_nv", "nvpm", BUGLE_LOG_ERROR,
                  "Failed to enumerate NVPM counters");
        goto cancel2;
    }

    f = bugle_filter_register(handle, "stats_nv");
    bugle_filter_catches(f, GROUP_glXSwapBuffers, false, stats_nv_glXSwapBuffers);
    bugle_filter_order("stats_nv", "invoke");
    bugle_filter_order("stats_nv", "stats");

    f = bugle_filter_register(handle, "stats_nv_post");
    bugle_filter_catches(f, GROUP_glXSwapBuffers, false, stats_nv_post_glXSwapBuffers);
    bugle_filter_order("invoke", "stats_nv_post");
    return true;

cancel2:
    fNVPMShutdown();
cancel1:
    lt_dlclose(stats_nv_dl);
    stats_nv_dl = NULL;
    return false;
}

static void stats_nv_destroy(filter_set *handle)
{
    linked_list_node *i;
    stats_signal *si;
    stats_signal_nv *nv;

    if (stats_nv_dl == NULL) return; /* NVPerfSDK not installed */

    fNVPMRemoveAllCounters();
    fNVPMShutdown();
    lt_dlclose(stats_nv_dl);
    bugle_list_clear(&stats_nv_registered);
    bugle_list_clear(&stats_nv_active);
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_variable_info stats_nv_variables[] =
    {
        { "flush", "flush OpenGL after each frame (more accurate but slower)", FILTER_SET_VARIABLE_BOOL, &stats_nv_flush, NULL },
        { NULL, NULL, 0, NULL, NULL }
    };

    static const filter_set_info stats_nv_info =
    {
        "stats_nv",
        stats_nv_initialise,
        stats_nv_destroy,
        NULL,
        NULL,
        stats_nv_variables,
        "stats module: get counters from NVPerfSDK"
    };

    bugle_filter_set_register(&stats_nv_info);
    bugle_filter_set_depends("stats_nv", "stats_basic");
    bugle_filter_set_stats_generator("stats_nv");
}

#else /* HAVE_NVPERFSDK_H */

void bugle_initialise_filter_library(void)
{
}

#endif /* !HAVE_NVPERFSDK_H */
