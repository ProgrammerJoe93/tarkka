pragma Singleton

import QtQuick

QtObject {
    id: theme

    readonly property color background: "#0e1015"
    readonly property color surface: "#161a22"
    readonly property color surfaceRaised: "#1d222c"
    readonly property color surfaceHover: "#232935"
    readonly property color border: "#282e3a"
    readonly property color borderStrong: "#39414f"

    readonly property color text: "#e9ecf3"
    readonly property color textMuted: "#8d96ab"
    readonly property color textFaint: "#5d6578"

    readonly property color accent: "#7c9cff"
    readonly property color accentSoft: "#1e2740"
    readonly property color positive: "#4ade80"
    readonly property color negative: "#fb7185"
    readonly property color warning: "#fbbf24"

    // Assigned to categories by index so the same category keeps its colour
    // between the bar chart and the transaction list.
    readonly property var categoryColors: [
        "#7c9cff", "#4ade80", "#fbbf24", "#fb7185", "#22d3ee",
        "#c084fc", "#f97316", "#2dd4bf", "#a3e635", "#f472b6"
    ]

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
