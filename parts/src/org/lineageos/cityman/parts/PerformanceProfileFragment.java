/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.cityman.parts;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.PowerManager;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import com.android.settingslib.widget.SelectorWithWidgetPreference;

public class PerformanceProfileFragment extends PreferenceFragmentCompat
        implements SelectorWithWidgetPreference.OnClickListener {

    private static final String[] KEYS = {
        "profile_power_save", "profile_balanced", "profile_performance",
    };

    private final SelectorWithWidgetPreference[] mProfiles =
            new SelectorWithWidgetPreference[KEYS.length];
    private Preference mBatterySaverNote;

    private final BroadcastReceiver mPowerSaveReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            update();
        }
    };

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.performance_profile, rootKey);
        for (int i = 0; i < KEYS.length; i++) {
            mProfiles[i] = findPreference(KEYS[i]);
            mProfiles[i].setOnClickListener(this);
        }
        mBatterySaverNote = findPreference("battery_saver_note");
    }

    @Override
    public void onStart() {
        super.onStart();
        requireContext().registerReceiver(mPowerSaveReceiver,
                new IntentFilter(PowerManager.ACTION_POWER_SAVE_MODE_CHANGED));
        update();
    }

    @Override
    public void onStop() {
        requireContext().unregisterReceiver(mPowerSaveReceiver);
        super.onStop();
    }

    @Override
    public void onRadioButtonClicked(SelectorWithWidgetPreference clicked) {
        for (int i = 0; i < KEYS.length; i++) {
            if (mProfiles[i] == clicked) {
                PerformanceProfile.set(requireContext(), i);
            }
        }
        update();
    }

    private void update() {
        int profile = PerformanceProfile.get();
        boolean forced = PerformanceProfile.isForcedByBatterySaver(requireContext());
        for (int i = 0; i < KEYS.length; i++) {
            mProfiles[i].setChecked(i == profile);
            // The choice is kept, and used again when Battery Saver turns off.
            mProfiles[i].setEnabled(!forced);
        }
        mBatterySaverNote.setVisible(forced);
    }
}
