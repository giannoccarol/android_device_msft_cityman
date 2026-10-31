/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.cityman.parts;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;

/**
 * Dynamic summary of the Settings > Battery entry: Settings calls
 * "getDynamicSummary" on the com.android.settings.summary_uri of the tile.
 */
public class SummaryProvider extends ContentProvider {

    private static final String METHOD_GET_DYNAMIC_SUMMARY = "getDynamicSummary";
    private static final String KEY_SUMMARY = "com.android.settings.summary";

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public Bundle call(String method, String uri, Bundle extras) {
        if (!METHOD_GET_DYNAMIC_SUMMARY.equals(method)) {
            return null;
        }
        Bundle bundle = new Bundle();
        bundle.putString(KEY_SUMMARY, PerformanceProfile.summary(getContext()));
        return bundle;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
            String sortOrder) {
        return null;
    }

    @Override
    public String getType(Uri uri) {
        return null;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        return 0;
    }
}
