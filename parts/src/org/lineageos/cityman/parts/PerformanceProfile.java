/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.cityman.parts;

import android.content.Context;
import android.net.Uri;
import android.os.PowerManager;
import android.os.SystemProperties;

/**
 * Performance profile shared with the power HAL (power/README.md): the HAL
 * follows persist.vendor.power.profile and turns it into perf locks.
 */
public final class PerformanceProfile {

    /* Same numbers as the profiles of the power HAL. */
    public static final int POWER_SAVE = 0;
    public static final int BALANCED = 1;
    public static final int PERFORMANCE = 2;

    private static final String PROP_PROFILE = "persist.vendor.power.profile";

    /* Dynamic summary of the Settings > Battery entry (SummaryProvider). */
    public static final String SUMMARY_AUTHORITY = "org.lineageos.cityman.parts.summary";
    public static final Uri SUMMARY_URI =
            Uri.parse("content://" + SUMMARY_AUTHORITY + "/getDynamicSummary");

    private PerformanceProfile() {}

    public static int get() {
        int profile = SystemProperties.getInt(PROP_PROFILE, BALANCED);
        return profile >= POWER_SAVE && profile <= PERFORMANCE ? profile : BALANCED;
    }

    public static void set(Context context, int profile) {
        SystemProperties.set(PROP_PROFILE, Integer.toString(profile));
        context.getContentResolver().notifyChange(SUMMARY_URI, null);
    }

    /** Battery Saver makes the power HAL use the power save profile. */
    public static boolean isForcedByBatterySaver(Context context) {
        PowerManager pm = context.getSystemService(PowerManager.class);
        return pm != null && pm.isPowerSaveMode();
    }

    public static int next(int profile) {
        return (profile + 1) % (PERFORMANCE + 1);
    }

    public static String name(Context context, int profile) {
        switch (profile) {
            case POWER_SAVE:
                return context.getString(R.string.profile_power_save);
            case PERFORMANCE:
                return context.getString(R.string.profile_performance);
            default:
                return context.getString(R.string.profile_balanced);
        }
    }

    /** What the system is using now, as a one-line summary. */
    public static String summary(Context context) {
        if (isForcedByBatterySaver(context)) {
            return context.getString(R.string.profile_battery_saver_summary,
                    name(context, POWER_SAVE));
        }
        return name(context, get());
    }
}
