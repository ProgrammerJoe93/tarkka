#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

/*!
 * Carries a set of bank connections from one device to another.
 *
 * A session belongs to the Enable Banking *application* -- the JWT's \c kid --
 * rather than to the machine that created it, so any client holding that
 * application's private key can read the accounts behind it. That is what makes
 * this possible at all: a bank connected on the desktop, where the consent
 * redirect can be served, syncs afterwards from a phone that could never have
 * completed the consent flow itself.
 *
 * The private key is deliberately **not** part of the file. Session ids are
 * useless without it, which keeps a file destined for cloud storage from being
 * enough on its own to reach someone's accounts; the key is moved across
 * separately and picked in Settings.
 */
namespace ConnectionFile {

struct Contents
{
    QString applicationId;
    QString country;
    QStringList sessionIds;

    //! An application id is the minimum that makes the file worth applying.
    bool isValid() const { return !applicationId.isEmpty(); }
};

//! Renders \a contents as the JSON written to disk.
QByteArray serialise(const Contents &contents);

/*!
 * Reads a file written by serialise().
 *
 * Returns invalid Contents and sets \a errorMessage when \a json is not one of
 * these files, rather than quietly applying whatever fields it happened to
 * recognise.
 */
Contents parse(const QByteArray &json, QString *errorMessage);

} // namespace ConnectionFile
