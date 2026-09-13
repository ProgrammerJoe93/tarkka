import QtQuick
import Tarkka

/*!
    A month-on-month change, coloured by which way it went.

    Spending more is the warning, spending less is the good news -- the opposite
    of what the positive/negative colours mean on an amount, where a credit is
    the welcome one. Hence its own component rather than reusing those directly:
    the mapping is a deliberate decision and belongs in one place.

    Shows nothing at all when \l direction is 0, which covers both "no earlier
    month to compare with" and "the same as last month". Neither deserves a
    colour, and inventing one would make the first month a user ever syncs look
    like a verdict.
*/
Text {
    id: badge

    //! -1 spending fell, 0 no meaningful change, +1 spending rose.
    required property int direction

    visible: direction !== 0 && text.length > 0
    color: direction > 0 ? Theme.negative : Theme.positive
    font.pixelSize: Theme.fontSizeSmall
    font.weight: Font.DemiBold
    horizontalAlignment: Text.AlignRight
    elide: Text.ElideRight
}
