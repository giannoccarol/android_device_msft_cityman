/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.cityman.parts;

import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;

/**
 * Quick Settings tile: a tap moves to the next profile (power save, balanced,
 * performance); a long press opens the Settings page.
 */
public class PerformanceProfileTileService extends TileService {

    @Override
    public void onStartListening() {
        super.onStartListening();
        update();
    }

    @Override
    public void onClick() {
        super.onClick();
        if (PerformanceProfile.isForcedByBatterySaver(this)) {
            return;
        }
        PerformanceProfile.set(this, PerformanceProfile.next(PerformanceProfile.get()));
        update();
    }

    private void update() {
        Tile tile = getQsTile();
        if (tile == null) {
            return;
        }
        boolean forced = PerformanceProfile.isForcedByBatterySaver(this);
        int profile = forced ? PerformanceProfile.POWER_SAVE : PerformanceProfile.get();
        tile.setSubtitle(PerformanceProfile.summary(this));
        if (forced) {
            tile.setState(Tile.STATE_UNAVAILABLE);
        } else {
            // Highlighted when the profile is not the default one.
            tile.setState(profile == PerformanceProfile.BALANCED
                    ? Tile.STATE_INACTIVE : Tile.STATE_ACTIVE);
        }
        tile.updateTile();
    }
}
