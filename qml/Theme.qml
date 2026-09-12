pragma Singleton

import QtQuick
import Tarkka

QtObject {
    id: theme

    /*!
        Whether the dark palette is in use.

        The setting wins where it names a palette; otherwise the platform's
        colour scheme decides, which on Android tracks the device's dark mode
        and on the desktop the platform theme. colorScheme notifies, so
        flipping the system theme repaints the running app rather than waiting
        for a restart. Platforms that expose no preference report Unknown
        rather than guessing, and dark is what this app shipped with, so only
        an explicit Light turns the light palette on.
    */
    readonly property bool dark: {
        if (AppController.appearance === "light")
            return false;
        if (AppController.appearance === "dark")
            return true;
        return Application.styleHints.colorScheme !== Qt.Light;
    }

    readonly property color background: dark ? "#0e1015" : "#eef1f6"
    readonly property color surface: dark ? "#161a22" : "#ffffff"
    readonly property color surfaceRaised: dark ? "#1d222c" : "#f4f6fa"
    readonly property color surfaceHover: dark ? "#232935" : "#e7ebf3"
    readonly property color border: dark ? "#282e3a" : "#dde2ec"
    readonly property color borderStrong: dark ? "#39414f" : "#aeb7c8"

    readonly property color text: dark ? "#e9ecf3" : "#11141a"
    readonly property color textMuted: dark ? "#8d96ab" : "#555f73"
    readonly property color textFaint: dark ? "#5d6578" : "#79839a"

    readonly property color accent: dark ? "#7c9cff" : "#2f57d8"
    readonly property color accentSoft: dark ? "#1e2740" : "#e6ebfd"
    // Labels and glyphs drawn on top of an accent fill. Not named onAccent:
    // QML reads a leading "on" before a capital as a signal handler, so such a
    // property is declared but never readable.
    readonly property color textOnAccent: dark ? "#0e1015" : "#ffffff"
    readonly property color positive: dark ? "#4ade80" : "#166534"
    readonly property color negative: dark ? "#fb7185" : "#9f1239"
    readonly property color warning: dark ? "#fbbf24" : "#854d0e"

    // Assigned to categories by index so the same category keeps its colour
    // between the bar chart and the transaction list. The two sets run in the
    // same hue order, so a category keeps its hue across a theme switch; the
    // light one is darker because these double as label colours, and a pastel
    // on white is unreadable.
    readonly property var categoryColors: dark
        ? ["#7c9cff", "#4ade80", "#fbbf24", "#fb7185", "#22d3ee",
           "#c084fc", "#f97316", "#2dd4bf", "#a3e635", "#f472b6"]
        : ["#2f57d8", "#166534", "#7c4a0a", "#be123c", "#0b5c72",
           "#7e22ce", "#9a3412", "#0d5f59", "#3f650c", "#be185d"]

    readonly property int radius: 10
    readonly property int radiusSmall: 6
    readonly property int spacing: 12
    readonly property int spacingLarge: 20
    readonly property int contentMargin: 24

    readonly property int fontSizeSmall: 12
    readonly property int fontSizeBody: 14
    readonly property int fontSizeTitle: 18
    readonly property int fontSizeDisplay: 30

    function colorForCategory(name) {
        if (!name)
            return theme.textFaint;
        // A stable hash keeps a category's colour identical across restarts.
        let hash = 0;
        for (let i = 0; i < name.length; ++i)
            hash = (hash * 31 + name.charCodeAt(i)) & 0x7fffffff;
        return theme.categoryColors[hash % theme.categoryColors.length];
    }
}
