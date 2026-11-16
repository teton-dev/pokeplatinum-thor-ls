package org.pokeplatinum.port;

import android.app.AlertDialog;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.graphics.drawable.StateListDrawable;
import android.os.SystemClock;
import android.text.InputType;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ContextThemeWrapper;
import android.view.ViewParent;
import android.view.WindowInsets;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.GridLayout;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.Switch;
import android.widget.TextView;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicReference;

import org.libsdl.app.SDLActivity;

/**
 * The settings in Android's own views. libntr draws each settings page as a recording of its widgets (Toggle,
 * Choice, Stepper, Button, Card... in sim_ui.cpp) and sends it here as JSON whenever it changes
 * (libntr_native_menus.patch); a tap goes back as an event, which the widget it is for answers as if it had been
 * tapped in libntr's own menus. The pages themselves stay in C++, shared with the desktop's menus.
 *
 * <p>Events (nativeEvent): "page N", "list", "back", "close", "classic", "toggle ID 0|1", "choice ID N", "step ID N",
 * "number ID N", "tap ID", "pair ID 1|2".
 *
 * <p>The pad: the D-pad (or the left stick) moves between rows, A chooses, B goes back, L/R change the page, R3
 * closes.
 */
public class NativeSettings extends FrameLayout {
    static native void nativeEvent(String event);

    // Colors (the accent is the app's own, R.color.settings_accent)
    private static final int BACKDROP = 0xFF0D1116;
    private static final int RAIL = 0xFF121820;
    private static final int CARD = 0xFF1B222C;
    private static final int CARD_FOCUSED = 0xFF263140;
    private static final int CARD_PRESSED = 0xFF2E3A4C;
    private static final int TEXT = 0xFFECF0F7;
    private static final int TEXT_DIM = 0xFF96A2B4;
    private static final int TEXT_OFF = 0xFF5C6678;
    private static final int ERROR = 0xFFF27468;
    private static final int GOOD = 0xFF70D08E;
    private static final int ON_ACCENT = 0xFF10161E;
    private static final int SEGMENT = 0xFF11161D;

    private final PlatinumActivity activity;
    private final int accent;
    private final float density;

    private final LinearLayout rail;
    private final LinearLayout railList;
    private final ScrollView railScroll;
    private final LinearLayout pane;
    private final TextView pageTitle;
    private final TextView pageSummary;
    private final TextView backArrow;
    private final ScrollView pageScroll;
    private final LinearLayout column;

    private final AtomicReference<String> pendingState = new AtomicReference<>();
    private boolean open;
    private JSONObject state;
    private String pagesKey = "";
    private String itemsKey = "";
    private int gen = -1;
    private int page = -1;
    private boolean listMode;
    private boolean capturing;
    private final List<Binder> binders = new ArrayList<>();
    private final Map<Long, View> focusables = new HashMap<>();
    private final List<View> railRows = new ArrayList<>();
    private long lastPageEventAt;

    // The left stick as a D-pad
    private int stickDirection;
    private long stickNextAt;

    /** Updates a row's views from a newer state of the same item. */
    private interface Binder {
        void bind(JSONObject item) throws JSONException;
    }

    public NativeSettings(PlatinumActivity activity) {
        // Material's switches and dialogs (the activity's own theme is the old full-screen one)
        super(new ContextThemeWrapper(activity, android.R.style.Theme_DeviceDefault));
        this.activity = activity;
        this.density = getResources().getDisplayMetrics().density;
        this.accent = getResources().getColor(R.color.settings_accent, null);
        setBackgroundColor(BACKDROP);
        setClickable(true); // touches stop here, not on the game below
        setVisibility(GONE);

        Context themed = getContext();
        LinearLayout root = new LinearLayout(themed);
        root.setOrientation(LinearLayout.HORIZONTAL);
        addView(root, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        // Clear of the system bars and cutouts
        setOnApplyWindowInsetsListener((v, insets) -> {
            root.setPadding(insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(),
                insets.getSystemWindowInsetRight(), insets.getSystemWindowInsetBottom());
            return insets;
        });

        // The rail: the app's name and the pages
        rail = new LinearLayout(themed);
        rail.setOrientation(LinearLayout.VERTICAL);
        rail.setBackgroundColor(RAIL);
        rail.setPadding(dp(16), dp(22), dp(16), dp(12));
        TextView app = text(activity.getString(R.string.app_name), 24, TEXT, true);
        rail.addView(app);
        TextView sub = text("Settings", 14, accent, true);
        sub.setPadding(0, 0, 0, dp(14));
        rail.addView(sub);
        railScroll = new ScrollView(themed);
        railScroll.setVerticalScrollBarEnabled(false);
        railList = new LinearLayout(themed);
        railList.setOrientation(LinearLayout.VERTICAL);
        railScroll.addView(railList);
        rail.addView(railScroll, new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, 0, 1));
        TextView hint = text("L / R: pages   B: back   R3: close", 12, TEXT_OFF, false);
        hint.setPadding(dp(4), dp(10), 0, 0);
        rail.addView(hint);
        root.addView(rail, new LinearLayout.LayoutParams(dp(280), LayoutParams.MATCH_PARENT));

        // The page: its title, then its rows in a column of readable width
        pane = new LinearLayout(themed);
        pane.setOrientation(LinearLayout.VERTICAL);
        LinearLayout header = new LinearLayout(themed);
        header.setOrientation(LinearLayout.HORIZONTAL);
        header.setGravity(Gravity.CENTER_VERTICAL);
        header.setPadding(dp(28), dp(20), dp(16), dp(8));
        backArrow = text("‹", 34, TEXT, false);
        backArrow.setPadding(dp(4), 0, dp(16), dp(4));
        backArrow.setOnClickListener(v -> nativeEvent("back"));
        header.addView(backArrow);
        LinearLayout titles = new LinearLayout(themed);
        titles.setOrientation(LinearLayout.VERTICAL);
        pageTitle = text("", 28, TEXT, true);
        pageSummary = text("", 14, TEXT_DIM, false);
        pageSummary.setSingleLine(true);
        pageSummary.setEllipsize(TextUtils.TruncateAt.END);
        titles.addView(pageTitle);
        titles.addView(pageSummary);
        header.addView(titles, new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
        TextView close = text("✕", 22, TEXT_DIM, false);
        close.setGravity(Gravity.CENTER);
        close.setBackground(rowBackground(0x00000000, dp(24)));
        close.setOnClickListener(v -> nativeEvent("close"));
        header.addView(close, new LinearLayout.LayoutParams(dp(48), dp(48)));
        pane.addView(header);

        pageScroll = new ScrollView(themed);
        pageScroll.setFillViewport(true);
        pageScroll.setClipToPadding(false);
        column = new LinearLayout(themed);
        column.setOrientation(LinearLayout.VERTICAL);
        column.setPadding(dp(28), dp(4), dp(28), dp(40));
        pageScroll.addView(column);
        // A column of readable width, in the middle of a wide pane
        pageScroll.addOnLayoutChangeListener((v, l, t, r, b, ol, ot, or, ob) -> {
            int side = Math.max(dp(28), (r - l - dp(760)) / 2);
            if (column.getPaddingLeft() != side) {
                column.setPadding(side, dp(4), side, dp(40));
            }
        });
        pane.addView(pageScroll, new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, 0, 1));
        root.addView(pane, new LinearLayout.LayoutParams(0, LayoutParams.MATCH_PARENT, 1));
    }

    // --- from the game (any thread) ---------------------------------------------------------------

    /** A new state: shows the settings if they were closed. Only the latest of several waiting is drawn. */
    public void postState(String json) {
        if (pendingState.getAndSet(json) == null) {
            post(() -> {
                String latest = pendingState.getAndSet(null);
                if (latest != null) {
                    show(latest);
                }
            });
        }
    }

    public void close() {
        post(this::hide);
    }

    public boolean isOpen() {
        return open;
    }

    // --- drawing --------------------------------------------------------------------------------

    private void show(String json) {
        JSONObject next;
        try {
            next = new JSONObject(json);
        } catch (JSONException e) {
            return;
        }
        if (!open) {
            open = true;
            setVisibility(VISIBLE);
            setAlpha(0f);
            animate().alpha(1f).setDuration(120).start();
            gen = -1;
        }
        try {
            render(next);
        } catch (JSONException e) {
            android.util.Log.w("NativeSettings", "Bad state", e);
        }
    }

    private void hide() {
        pendingState.set(null);
        if (!open) {
            return;
        }
        open = false;
        capturing = false;
        animate().alpha(0f).setDuration(90).withEndAction(() -> {
            if (!open) {
                setVisibility(GONE);
            }
        }).start();
        activity.nativeSettingsHidden();
    }

    private boolean wide() {
        return getWidth() == 0 || getWidth() >= dp(640);
    }

    private void render(JSONObject next) throws JSONException {
        state = next;
        JSONArray pages = next.getJSONArray("pages");
        int newPage = next.getInt("page");
        boolean newList = next.getBoolean("list");
        int newGen = next.getInt("gen");
        capturing = next.optBoolean("capturing");
        boolean wide = wide();

        // A wide screen always shows a page beside the list: out of a page, the cursor goes to the list
        if (newList && wide && pages.length() > 0) {
            boolean focusInPage = isInside(findFocus(), column);
            nativeEvent("page " + newPage);
            if (focusInPage || findFocus() == null) {
                focusRail(newPage);
            }
        }

        String newPagesKey = pages.toString();
        if (!newPagesKey.equals(pagesKey)) {
            pagesKey = newPagesKey;
            buildRail(pages);
        }
        if (newPage != page || newList != listMode) {
            for (int i = 0; i < railRows.size(); i++) {
                railRows.get(i).setSelected(i == newPage);
            }
        }
        rail.setVisibility(wide || newList ? VISIBLE : GONE);
        pane.setVisibility(wide || !newList ? VISIBLE : GONE);
        int railWidth = !wide && newList ? LayoutParams.MATCH_PARENT : dp(280);
        if (rail.getLayoutParams().width != railWidth) {
            rail.getLayoutParams().width = railWidth;
            rail.requestLayout();
        }
        backArrow.setVisibility(wide ? GONE : VISIBLE);

        if (newPage >= 0 && newPage < pages.length()) {
            JSONObject p = pages.getJSONObject(newPage);
            pageTitle.setText(p.optString("name"));
            String summary = p.optString("summary");
            pageSummary.setText(summary);
            pageSummary.setVisibility(summary.isEmpty() ? GONE : VISIBLE);
        }

        JSONArray items = next.getJSONArray("items");
        String newItemsKey = structure(items);
        boolean newView = newGen != gen || newPage != page;
        if (newList && wide) {
            // Keep what's shown until the page's own state arrives
        } else if (!newItemsKey.equals(itemsKey) || newView) {
            long focused = focusedId();
            buildItems(items);
            itemsKey = newItemsKey;
            if (newView) {
                pageScroll.scrollTo(0, 0);
                // Into the page's default row, unless the pad is going down the list of pages
                if (!newList && !isInside(findFocus(), rail)) {
                    focusDefault(next.optLong("focus"));
                }
            } else if (focused != 0 && focusables.containsKey(focused)) {
                focusables.get(focused).requestFocus();
            }
        } else {
            for (int i = 0; i < binders.size(); i++) {
                Binder binder = binders.get(i);
                if (binder != null) {
                    binder.bind(items.getJSONObject(i));
                }
            }
        }
        if (newList && !wide && findFocus() == null) {
            focusRail(newPage);
        }
        if (gen == -1 && findFocus() == null) {
            focusRail(newPage);
        }
        gen = newGen;
        page = newPage;
        listMode = newList;
    }

    private void buildRail(JSONArray pages) throws JSONException {
        railList.removeAllViews();
        railRows.clear();
        for (int i = 0; i < pages.length(); i++) {
            JSONObject p = pages.getJSONObject(i);
            final int index = i;
            LinearLayout row = new LinearLayout(getContext());
            row.setOrientation(LinearLayout.VERTICAL);
            row.setPadding(dp(16), dp(10), dp(12), dp(10));
            row.setBackground(railBackground());
            row.setFocusable(true);
            TextView name = text(p.optString("name"), 17, TEXT, true);
            row.addView(name);
            row.setOnClickListener(v -> {
                nativeEvent("page " + index);
                if (!wide()) {
                    pageScroll.requestFocus();
                }
            });
            // With the pad, the page follows the cursor (wide screens)
            row.setOnFocusChangeListener((v, has) -> {
                if (has && wide() && index != page) {
                    lastPageEventAt = SystemClock.uptimeMillis();
                    nativeEvent("page " + index);
                }
            });
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT,
                LayoutParams.WRAP_CONTENT);
            lp.bottomMargin = dp(4);
            railList.addView(row, lp);
            railRows.add(row);
        }
    }

    /** The items' identities: when they're the same, the rows are updated in place (focus and scroll stay). */
    private static String structure(JSONArray items) throws JSONException {
        StringBuilder key = new StringBuilder();
        for (int i = 0; i < items.length(); i++) {
            JSONObject item = items.getJSONObject(i);
            key.append(item.optString("t")).append(':').append(item.optLong("id")).append(';');
            JSONArray options = item.optJSONArray("options");
            if (options != null) {
                key.append(options.length()).append(';');
            }
            JSONArray tiles = item.optJSONArray("items");
            if (tiles != null) {
                for (int j = 0; j < tiles.length(); j++) {
                    key.append(tiles.getJSONObject(j).optLong("id")).append(',');
                }
            }
        }
        return key.toString();
    }

    private void buildItems(JSONArray items) throws JSONException {
        column.removeAllViews();
        binders.clear();
        focusables.clear();
        for (int i = 0; i < items.length(); i++) {
            JSONObject item = items.getJSONObject(i);
            binders.add(addItem(item));
        }
    }

    private Binder addItem(JSONObject item) throws JSONException {
        switch (item.optString("t")) {
        case "section": return addSection(item);
        case "note": return addNote(item);
        case "status": return addStatus(item);
        case "progress": return addProgress(item);
        case "toggle": return addToggle(item);
        case "choice": return addChoice(item);
        case "stepper": return addStepper(item, false);
        case "number": return addStepper(item, true);
        case "button": return addButton(item);
        case "pair": return addPair(item);
        case "card": return addCard(item, false);
        case "item": return addCard(item, true);
        case "back": return addBack(item);
        case "panel": return addPanel(item);
        case "tiles": return addTiles(item);
        default: return null;
        }
    }

    // --- rows -----------------------------------------------------------------------------------

    private Binder addSection(JSONObject item) {
        TextView view = text("", 13, accent, true);
        view.setAllCaps(true);
        view.setLetterSpacing(0.08f);
        view.setPadding(dp(6), dp(22), 0, dp(8));
        column.addView(view);
        Binder b = it -> view.setText(it.optString("text"));
        bindNow(b, item);
        return b;
    }

    private Binder addNote(JSONObject item) {
        TextView view = text("", 14, TEXT_DIM, false);
        view.setLineSpacing(0, 1.15f);
        view.setPadding(dp(6), dp(2), dp(6), dp(10));
        column.addView(view);
        Binder b = it -> view.setText(it.optString("text"));
        bindNow(b, item);
        return b;
    }

    private Binder addStatus(JSONObject item) {
        TextView view = text("", 15, GOOD, true);
        view.setPadding(dp(6), dp(2), dp(6), dp(10));
        column.addView(view);
        Binder b = it -> {
            view.setText(it.optString("text"));
            view.setTextColor(it.optBoolean("error") ? ERROR : GOOD);
        };
        bindNow(b, item);
        return b;
    }

    private Binder addProgress(JSONObject item) {
        LinearLayout card = card(false);
        TextView label = text("", 16, TEXT, true);
        card.addView(label);
        ProgressBar bar = new ProgressBar(getContext(), null, android.R.attr.progressBarStyleHorizontal);
        bar.setMax(1000);
        bar.setProgressTintList(ColorStateList.valueOf(accent));
        bar.setIndeterminateTintList(ColorStateList.valueOf(accent));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, dp(10));
        lp.topMargin = dp(10);
        card.addView(bar, lp);
        addRow(card);
        Binder b = it -> {
            label.setText(it.optString("text"));
            int permille = it.optInt("permille", -1);
            bar.setIndeterminate(permille < 0);
            if (permille >= 0) {
                bar.setProgress(permille);
            }
        };
        bindNow(b, item);
        return b;
    }

    private Binder addToggle(JSONObject item) {
        LinearLayout row = card(true);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        LinearLayout texts = texts();
        TextView label = (TextView) texts.getChildAt(0);
        TextView help = (TextView) texts.getChildAt(1);
        row.addView(texts, new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
        Switch sw = new Switch(getContext());
        sw.setFocusable(false);
        sw.setClickable(false);
        sw.setThumbTintList(new ColorStateList(new int[][] {
            new int[] { android.R.attr.state_checked }, new int[] {} }, new int[] { 0xFFFFFFFF, 0xFFC9D0DA }));
        sw.setTrackTintList(new ColorStateList(new int[][] {
            new int[] { android.R.attr.state_checked }, new int[] {} }, new int[] { accent, 0xFF8A96AA }));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT,
            LayoutParams.WRAP_CONTENT);
        lp.leftMargin = dp(16);
        row.addView(sw, lp);
        final long id = item.optLong("id");
        row.setOnClickListener(v -> {
            boolean on = !sw.isChecked();
            sw.setChecked(on); // at once; the game's state follows
            nativeEvent("toggle " + id + " " + (on ? 1 : 0));
        });
        addRow(row);
        focusables.put(id, row);
        Binder b = it -> {
            setTexts(label, help, it.optString("label"), it.optString("help", null), it.optBoolean("en", true));
            sw.setChecked(it.optBoolean("on"));
            sw.setEnabled(it.optBoolean("en", true));
            setUsable(row, it.optBoolean("en", true));
        };
        bindNow(b, item);
        return b;
    }

    private Binder addChoice(JSONObject item) throws JSONException {
        LinearLayout card = card(false);
        LinearLayout texts = texts();
        TextView label = (TextView) texts.getChildAt(0);
        TextView help = (TextView) texts.getChildAt(1);
        card.addView(texts);
        JSONArray options = item.getJSONArray("options");
        boolean across = options.length() <= 4;
        for (int i = 0; i < options.length(); i++) {
            across &= options.getString(i).length() <= 16;
        }
        LinearLayout segments = new LinearLayout(getContext());
        segments.setOrientation(across ? LinearLayout.HORIZONTAL : LinearLayout.VERTICAL);
        LinearLayout.LayoutParams slp = new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT,
            LayoutParams.WRAP_CONTENT);
        slp.topMargin = dp(12);
        card.addView(segments, slp);
        final long id = item.optLong("id");
        final List<TextView> pills = new ArrayList<>();
        for (int i = 0; i < options.length(); i++) {
            final int index = i;
            TextView pill = text(options.getString(i), 15, TEXT, true);
            pill.setGravity(Gravity.CENTER);
            pill.setMinHeight(dp(46));
            pill.setPadding(dp(10), dp(6), dp(10), dp(6));
            pill.setFocusable(true);
            pill.setBackground(segmentBackground());
            pill.setOnClickListener(v -> {
                for (int j = 0; j < pills.size(); j++) {
                    pills.get(j).setSelected(j == index);
                    pills.get(j).setTextColor(j == index ? ON_ACCENT : TEXT);
                }
                nativeEvent("choice " + id + " " + index);
            });
            LinearLayout.LayoutParams lp = across
                ? new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1)
                : new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
            if (i > 0) {
                if (across) {
                    lp.leftMargin = dp(6);
                } else {
                    lp.topMargin = dp(6);
                }
            }
            segments.addView(pill, lp);
            pills.add(pill);
        }
        addRow(card);
        if (!pills.isEmpty()) {
            focusables.put(id, pills.get(Math.max(0, Math.min(pills.size() - 1, item.optInt("value")))));
        }
        Binder b = it -> {
            boolean enabled = it.optBoolean("en", true);
            setTexts(label, help, it.optString("label"), it.optString("help", null), enabled);
            int value = it.optInt("value");
            for (int j = 0; j < pills.size(); j++) {
                TextView pill = pills.get(j);
                pill.setSelected(j == value);
                pill.setTextColor(j == value ? ON_ACCENT : (enabled ? TEXT : TEXT_OFF));
                setUsable(pill, enabled);
            }
        };
        bindNow(b, item);
        return b;
    }

    private Binder addStepper(JSONObject item, boolean number) {
        LinearLayout row = card(false);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        LinearLayout texts = texts();
        TextView label = (TextView) texts.getChildAt(0);
        TextView help = (TextView) texts.getChildAt(1);
        row.addView(texts, new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
        TextView minus = stepButton("−");
        TextView value = text("", 18, TEXT, true);
        value.setGravity(Gravity.CENTER);
        value.setMinWidth(dp(number ? 96 : 84));
        TextView plus = stepButton("+");
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(dp(52), dp(46));
        lp.leftMargin = dp(12);
        row.addView(minus, lp);
        row.addView(value, new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT, dp(46)));
        row.addView(plus, new LinearLayout.LayoutParams(dp(52), dp(46)));
        final long id = item.optLong("id");
        final int[] current = new int[4]; // value, min, max, step
        final String kind = number ? "number" : "step";
        minus.setOnClickListener(v -> nativeEvent(kind + " " + id + " " + Math.max(current[1], current[0] - current[3])));
        plus.setOnClickListener(v -> nativeEvent(kind + " " + id + " " + Math.min(current[2], current[0] + current[3])));
        if (number) {
            value.setFocusable(true);
            value.setBackground(segmentBackground());
            value.setOnClickListener(v -> askNumber(label.getText().toString(), id, current[0], current[1], current[2]));
        }
        addRow(row);
        focusables.put(id, plus);
        Binder b = it -> {
            boolean enabled = it.optBoolean("en", true);
            setTexts(label, help, it.optString("label"), it.optString("help", null), enabled);
            current[0] = it.optInt("value");
            current[1] = it.optInt("min");
            current[2] = it.optInt("max");
            current[3] = Math.max(1, it.optInt("step", 1));
            value.setText(number ? String.valueOf(current[0]) : it.optString("text"));
            value.setTextColor(enabled ? TEXT : TEXT_OFF);
            setUsable(minus, enabled && current[0] > current[1]);
            setUsable(plus, enabled && current[0] < current[2]);
            if (number) {
                setUsable(value, enabled);
            }
        };
        bindNow(b, item);
        return b;
    }

    private Binder addButton(JSONObject item) {
        LinearLayout row = card(true);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        LinearLayout texts = texts();
        TextView label = (TextView) texts.getChildAt(0);
        TextView help = (TextView) texts.getChildAt(1);
        row.addView(texts, new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
        TextView chevron = text("›", 26, TEXT_DIM, false);
        chevron.setPadding(dp(12), 0, dp(2), dp(3));
        row.addView(chevron);
        final long id = item.optLong("id");
        row.setOnClickListener(v -> nativeEvent("tap " + id));
        addRow(row);
        focusables.put(id, row);
        final boolean[] primary = { !item.optBoolean("primary") };
        Binder b = it -> {
            boolean enabled = it.optBoolean("en", true);
            boolean isPrimary = it.optBoolean("primary");
            if (isPrimary != primary[0]) {
                primary[0] = isPrimary;
                row.setBackground(isPrimary ? accentBackground() : rowBackground(CARD, dp(14)));
            }
            setTexts(label, help, it.optString("label"), it.optString("help", null), enabled);
            if (isPrimary && enabled) {
                label.setTextColor(ON_ACCENT);
                help.setTextColor(0xCC10161E);
            }
            chevron.setTextColor(isPrimary ? ON_ACCENT : TEXT_DIM);
            setUsable(row, enabled);
        };
        bindNow(b, item);
        return b;
    }

    private Binder addPair(JSONObject item) {
        LinearLayout row = new LinearLayout(getContext());
        row.setOrientation(LinearLayout.HORIZONTAL);
        final long id = item.optLong("id");
        TextView left = pairButton();
        TextView right = pairButton();
        left.setOnClickListener(v -> nativeEvent("pair " + id + " 1"));
        right.setOnClickListener(v -> nativeEvent("pair " + id + " 2"));
        row.addView(left, new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1);
        lp.leftMargin = dp(10);
        row.addView(right, lp);
        addRow(row);
        focusables.put(id, left);
        Binder b = it -> {
            left.setText(it.optString("left"));
            right.setText(it.optString("right"));
            boolean primary = it.optBoolean("primary");
            left.setBackground(primary ? accentBackground() : rowBackground(CARD, dp(14)));
            left.setTextColor(primary ? ON_ACCENT : TEXT);
            setUsable(left, it.optBoolean("len", true));
            setUsable(right, it.optBoolean("ren", true));
        };
        bindNow(b, item);
        return b;
    }

    private Binder addCard(JSONObject item, boolean compact) {
        LinearLayout row = card(true);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        View bar = new View(getContext());
        GradientDrawable barShape = new GradientDrawable();
        barShape.setColor(accent);
        barShape.setCornerRadius(dp(2));
        bar.setBackground(barShape);
        LinearLayout.LayoutParams blp = new LinearLayout.LayoutParams(dp(4), dp(compact ? 22 : 30));
        blp.rightMargin = dp(12);
        row.addView(bar, blp);
        LinearLayout texts = texts();
        TextView title = (TextView) texts.getChildAt(0);
        TextView sub = (TextView) texts.getChildAt(1);
        row.addView(texts, new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
        final long id = item.optLong("id");
        row.setOnClickListener(v -> nativeEvent("tap " + id));
        addRow(row);
        focusables.put(id, row);
        Binder b = it -> {
            boolean enabled = it.optBoolean("en", true);
            boolean selected = it.optBoolean("sel");
            setTexts(title, sub, compact ? it.optString("label") : it.optString("title"), it.optString("sub", null),
                enabled);
            bar.setVisibility(selected ? VISIBLE : INVISIBLE);
            row.setSelected(selected);
            setUsable(row, enabled);
        };
        bindNow(b, item);
        return b;
    }

    private Binder addBack(JSONObject item) {
        TextView view = text("", 16, TEXT, true);
        view.setPadding(dp(14), dp(10), dp(18), dp(10));
        view.setFocusable(true);
        view.setBackground(rowBackground(CARD, dp(12)));
        final long id = item.optLong("id");
        view.setOnClickListener(v -> nativeEvent("tap " + id));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT,
            LayoutParams.WRAP_CONTENT);
        lp.bottomMargin = dp(10);
        column.addView(view, lp);
        Binder b = it -> view.setText("‹  " + it.optString("label"));
        bindNow(b, item);
        return b;
    }

    private Binder addPanel(JSONObject item) {
        LinearLayout card = card(false);
        LinearLayout texts = texts();
        TextView title = (TextView) texts.getChildAt(0);
        TextView body = (TextView) texts.getChildAt(1);
        body.setTextColor(TEXT);
        body.setLineSpacing(0, 1.15f);
        card.addView(texts);
        addRow(card);
        Binder b = it -> {
            title.setText(it.optString("title"));
            body.setText(it.optString("text"));
        };
        bindNow(b, item);
        return b;
    }

    private Binder addTiles(JSONObject item) throws JSONException {
        GridLayout grid = new GridLayout(getContext());
        final int per = Math.max(1, item.optInt("per", 3));
        grid.setColumnCount(per);
        grid.setUseDefaultMargins(false);
        JSONArray tiles = item.getJSONArray("items");
        final List<TextView> views = new ArrayList<>();
        for (int i = 0; i < tiles.length(); i++) {
            JSONObject tile = tiles.getJSONObject(i);
            final long id = tile.optLong("id");
            TextView view = text("", 15, TEXT, true);
            view.setGravity(Gravity.CENTER);
            view.setMinHeight(dp(54));
            view.setPadding(dp(6), dp(6), dp(6), dp(6));
            view.setFocusable(true);
            view.setBackground(segmentBackground());
            view.setOnClickListener(v -> nativeEvent("tap " + id));
            GridLayout.LayoutParams lp = new GridLayout.LayoutParams(GridLayout.spec(i / per),
                GridLayout.spec(i % per, 1f));
            lp.width = 0;
            lp.setMargins(dp(3), dp(3), dp(3), dp(3));
            grid.addView(view, lp);
            views.add(view);
            focusables.put(id, view);
        }
        LinearLayout.LayoutParams glp = new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT,
            LayoutParams.WRAP_CONTENT);
        glp.bottomMargin = dp(10);
        column.addView(grid, glp);
        Binder b = it -> {
            JSONArray list = it.getJSONArray("items");
            for (int i = 0; i < views.size() && i < list.length(); i++) {
                JSONObject tile = list.getJSONObject(i);
                TextView view = views.get(i);
                String sub = tile.optString("sub", "");
                view.setText(sub.isEmpty() ? tile.optString("label") : tile.optString("label") + "\n" + sub);
                boolean selected = tile.optBoolean("sel");
                boolean enabled = tile.optBoolean("en", true);
                view.setSelected(selected);
                view.setTextColor(selected ? ON_ACCENT : (enabled ? TEXT : TEXT_OFF));
                setUsable(view, enabled);
            }
        };
        bindNow(b, item);
        return b;
    }

    private void askNumber(String title, long id, int value, int min, int max) {
        EditText input = new EditText(getContext());
        input.setInputType(InputType.TYPE_CLASS_NUMBER | (min < 0 ? InputType.TYPE_NUMBER_FLAG_SIGNED : 0));
        input.setText(String.valueOf(value));
        input.setSelectAllOnFocus(true);
        new AlertDialog.Builder(getContext(), android.R.style.Theme_DeviceDefault_Dialog_Alert)
            .setTitle(title)
            .setMessage(min + " to " + max)
            .setView(input)
            .setPositiveButton("OK", (d, w) -> {
                try {
                    int v = Integer.parseInt(input.getText().toString().trim());
                    nativeEvent("number " + id + " " + Math.max(min, Math.min(max, v)));
                } catch (NumberFormatException ignored) {
                }
            })
            .setNegativeButton("Cancel", null)
            .show();
    }

    // --- the pad --------------------------------------------------------------------------------

    /** A key while the settings are open; FALSE lets Android move the focus (the D-pad). */
    public boolean handleKey(KeyEvent event) {
        int code = event.getKeyCode();
        if (capturing) {
            // A control is waiting for its button: the game's input gets it
            SDLActivity.handleKeyEvent(null, code, event, null);
            return true;
        }
        boolean down = event.getAction() == KeyEvent.ACTION_DOWN;
        switch (code) {
        case KeyEvent.KEYCODE_BUTTON_A:
        case KeyEvent.KEYCODE_DPAD_CENTER:
        case KeyEvent.KEYCODE_ENTER:
        case KeyEvent.KEYCODE_NUMPAD_ENTER:
            if (down && event.getRepeatCount() == 0) {
                View focused = findFocus();
                if (focused != null) {
                    focused.performClick();
                }
            }
            return true;
        case KeyEvent.KEYCODE_BUTTON_B:
        case KeyEvent.KEYCODE_BACK:
        case KeyEvent.KEYCODE_ESCAPE:
            if (down && event.getRepeatCount() == 0) {
                goBack();
            }
            return true;
        case KeyEvent.KEYCODE_BUTTON_L1:
        case KeyEvent.KEYCODE_BUTTON_R1:
            if (down && event.getRepeatCount() == 0 && state != null) {
                int count = state.optJSONArray("pages") != null ? state.optJSONArray("pages").length() : 0;
                int next = page + (code == KeyEvent.KEYCODE_BUTTON_L1 ? -1 : 1);
                if (next >= 0 && next < count) {
                    nativeEvent("page " + next);
                    if (wide()) {
                        focusRail(next);
                    }
                }
            }
            return true;
        case KeyEvent.KEYCODE_BUTTON_THUMBR:
        case KeyEvent.KEYCODE_BUTTON_MODE:
            if (down && event.getRepeatCount() == 0) {
                nativeEvent("close");
            }
            return true;
        case KeyEvent.KEYCODE_DPAD_UP:
        case KeyEvent.KEYCODE_DPAD_DOWN:
        case KeyEvent.KEYCODE_DPAD_LEFT:
        case KeyEvent.KEYCODE_DPAD_RIGHT:
            if (findFocus() == null && down) {
                focusRail(page);
                return true;
            }
            return false;
        default:
            // Start, Select, X, Y... do nothing here, and don't reach the game
            return (event.getSource() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD;
        }
    }

    private void goBack() {
        View focused = findFocus();
        boolean inRail = isInside(focused, rail);
        if (listMode || (wide() && inRail)) {
            nativeEvent("close");
        } else {
            nativeEvent("back"); // the page may go back inside itself; otherwise libntr leaves it for the list
        }
    }

    /** The left stick moves the focus like the D-pad. */
    public boolean handleMotion(MotionEvent event) {
        if ((event.getSource() & InputDevice.SOURCE_JOYSTICK) != InputDevice.SOURCE_JOYSTICK
                || event.getAction() != MotionEvent.ACTION_MOVE) {
            return false;
        }
        if (capturing) {
            return false; // axes can be chosen for a control: SDL gets them
        }
        float x = event.getAxisValue(MotionEvent.AXIS_X);
        float y = event.getAxisValue(MotionEvent.AXIS_Y);
        int direction = 0;
        if (Math.max(Math.abs(x), Math.abs(y)) > 0.6f) {
            if (Math.abs(x) > Math.abs(y)) {
                direction = x > 0 ? View.FOCUS_RIGHT : View.FOCUS_LEFT;
            } else {
                direction = y > 0 ? View.FOCUS_DOWN : View.FOCUS_UP;
            }
        }
        long now = SystemClock.uptimeMillis();
        if (direction == 0) {
            stickDirection = 0;
        } else if (direction != stickDirection || now >= stickNextAt) {
            stickNextAt = now + (direction != stickDirection ? 320 : 110);
            stickDirection = direction;
            View focused = findFocus();
            if (focused == null) {
                focusRail(page);
            } else {
                View next = focused.focusSearch(direction);
                if (next != null) {
                    next.requestFocus(direction);
                }
            }
        }
        return true;
    }

    // --- focus ----------------------------------------------------------------------------------

    private void focusRail(int index) {
        if (index >= 0 && index < railRows.size() && rail.getVisibility() == VISIBLE) {
            railRows.get(index).requestFocus();
        }
    }

    private void focusDefault(long id) {
        View target = id != 0 ? focusables.get(id) : null;
        if (target == null || !target.isFocusable()) {
            target = firstFocusable(column);
        }
        if (target != null) {
            target.requestFocus();
        }
    }

    private static View firstFocusable(ViewGroup group) {
        for (int i = 0; i < group.getChildCount(); i++) {
            View child = group.getChildAt(i);
            if (child.getVisibility() != VISIBLE) {
                continue;
            }
            if (child.isFocusable() && child.isEnabled()) {
                return child;
            }
            if (child instanceof ViewGroup) {
                View inner = firstFocusable((ViewGroup) child);
                if (inner != null) {
                    return inner;
                }
            }
        }
        return null;
    }

    private long focusedId() {
        View focused = findFocus();
        for (Map.Entry<Long, View> entry : focusables.entrySet()) {
            if (entry.getValue() == focused) {
                return entry.getKey();
            }
        }
        return 0;
    }

    private static boolean isInside(View view, View ancestor) {
        while (view != null) {
            if (view == ancestor) {
                return true;
            }
            ViewParent parent = view.getParent();
            view = parent instanceof View ? (View) parent : null;
        }
        return false;
    }

    // --- building blocks ------------------------------------------------------------------------

    private int dp(float value) {
        return Math.round(value * density);
    }

    private TextView text(String value, float sp, int color, boolean bold) {
        TextView view = new TextView(getContext());
        view.setText(value);
        view.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        view.setTextColor(color);
        view.setTypeface(Typeface.create("sans-serif", bold ? Typeface.BOLD : Typeface.NORMAL));
        return view;
    }

    private LinearLayout texts() {
        LinearLayout box = new LinearLayout(getContext());
        box.setOrientation(LinearLayout.VERTICAL);
        TextView label = text("", 17, TEXT, true);
        TextView help = text("", 13.5f, TEXT_DIM, false);
        help.setLineSpacing(0, 1.12f);
        help.setPadding(0, dp(3), 0, 0);
        box.addView(label);
        box.addView(help);
        return box;
    }

    private void setTexts(TextView label, TextView help, String title, String sub, boolean enabled) {
        label.setText(title);
        label.setTextColor(enabled ? TEXT : TEXT_OFF);
        boolean hasSub = sub != null && !sub.isEmpty();
        help.setVisibility(hasSub ? VISIBLE : GONE);
        if (hasSub) {
            help.setText(sub);
        }
        help.setTextColor(enabled ? TEXT_DIM : TEXT_OFF);
    }

    private void setUsable(View view, boolean usable) {
        view.setEnabled(usable);
        view.setFocusable(usable);
        view.setAlpha(usable ? 1f : 0.55f);
    }

    private LinearLayout card(boolean focusable) {
        LinearLayout card = new LinearLayout(getContext());
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(18), dp(14), dp(18), dp(14));
        card.setMinimumHeight(dp(60));
        card.setBackground(focusable ? rowBackground(CARD, dp(14)) : plain(CARD, dp(14)));
        card.setFocusable(focusable);
        return card;
    }

    private void addRow(View row) {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT,
            LayoutParams.WRAP_CONTENT);
        lp.bottomMargin = dp(8);
        column.addView(row, lp);
    }

    private TextView stepButton(String sign) {
        TextView view = text(sign, 24, TEXT, true);
        view.setGravity(Gravity.CENTER);
        view.setFocusable(true);
        view.setBackground(segmentBackground());
        return view;
    }

    private TextView pairButton() {
        TextView view = text("", 16, TEXT, true);
        view.setGravity(Gravity.CENTER);
        view.setMinHeight(dp(54));
        view.setPadding(dp(10), dp(8), dp(10), dp(8));
        view.setFocusable(true);
        view.setBackground(rowBackground(CARD, dp(14)));
        return view;
    }

    private void bindNow(Binder binder, JSONObject item) {
        try {
            binder.bind(item);
        } catch (JSONException e) {
            android.util.Log.w("NativeSettings", "Bad item", e);
        }
    }

    private static GradientDrawable shape(int color, int radius) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(color);
        d.setCornerRadius(radius);
        return d;
    }

    private Drawable plain(int color, int radius) {
        return shape(color, radius);
    }

    /** A row: its color, lighter when pressed, with the pad's focus ring; a ripple for touches. */
    private Drawable rowBackground(int color, int radius) {
        StateListDrawable states = new StateListDrawable();
        GradientDrawable focused = shape(color == 0 ? 0x22FFFFFF : CARD_FOCUSED, radius);
        focused.setStroke(dp(2.5f), accent);
        states.addState(new int[] { android.R.attr.state_focused }, focused);
        states.addState(new int[] { android.R.attr.state_selected }, shape(color == 0 ? 0 : CARD_PRESSED, radius));
        states.addState(new int[] {}, shape(color, radius));
        return new RippleDrawable(ColorStateList.valueOf(0x33FFFFFF), states, shape(0xFFFFFFFF, radius));
    }

    private Drawable railBackground() {
        StateListDrawable states = new StateListDrawable();
        GradientDrawable focused = shape(CARD_FOCUSED, dp(12));
        focused.setStroke(dp(2.5f), accent);
        states.addState(new int[] { android.R.attr.state_focused }, focused);
        GradientDrawable selected = shape(CARD, dp(12));
        states.addState(new int[] { android.R.attr.state_selected }, selected);
        states.addState(new int[] {}, new ColorDrawable(0));
        return new RippleDrawable(ColorStateList.valueOf(0x33FFFFFF), states, shape(0xFFFFFFFF, dp(12)));
    }

    /** A segment, tile or small button: accent when selected, ring when focused. */
    private Drawable segmentBackground() {
        StateListDrawable states = new StateListDrawable();
        GradientDrawable selectedFocused = shape(accent, dp(11));
        selectedFocused.setStroke(dp(2.5f), 0xFFFFFFFF);
        states.addState(new int[] { android.R.attr.state_selected, android.R.attr.state_focused }, selectedFocused);
        states.addState(new int[] { android.R.attr.state_selected }, shape(accent, dp(11)));
        GradientDrawable focused = shape(CARD_FOCUSED, dp(11));
        focused.setStroke(dp(2.5f), accent);
        states.addState(new int[] { android.R.attr.state_focused }, focused);
        states.addState(new int[] {}, shape(SEGMENT, dp(11)));
        return new RippleDrawable(ColorStateList.valueOf(0x33FFFFFF), states, shape(0xFFFFFFFF, dp(11)));
    }

    private Drawable accentBackground() {
        StateListDrawable states = new StateListDrawable();
        GradientDrawable focused = shape(accent, dp(14));
        focused.setStroke(dp(2.5f), 0xFFFFFFFF);
        states.addState(new int[] { android.R.attr.state_focused }, focused);
        states.addState(new int[] {}, shape(accent, dp(14)));
        return new RippleDrawable(ColorStateList.valueOf(0x44FFFFFF), states, shape(0xFFFFFFFF, dp(14)));
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        if (open && state != null) {
            post(() -> {
                try {
                    itemsKey = "";
                    render(state);
                } catch (JSONException ignored) {
                }
            });
        }
    }
}
