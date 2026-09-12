# Tarkka

A Qt 6 / QML desktop app that pulls your purchases from every bank and card you
use into one place, and shows where the money actually went.

*Tarkka* is Finnish for precise — and, said of a person, careful with money.

<img src="assets/icons/tarkka-128.png" width="72" align="right" alt="">

![Dashboard](docs/dashboard.png)

## Getting your real data

This is the part worth reading before anything else, because the obvious route
does not exist.

**Banks will not give you direct API access to your own personal accounts.**
An Open Banking production API is gated behind a licensed PSD2 AISP
registration with an eIDAS certificate, which an individual cannot obtain — the
sandboxes are open to anyone, but only serve fake data. Some banks have no
personal-account API at all; where a business API exists it wants a business
account on a paid plan, and the Open Banking API is for regulated third
parties. Nordigen/GoCardless, which used to be the free workaround, closed to
new signups in 2025.

So the app takes the two routes that actually work for a private individual:

### 1. Live sync via Enable Banking (recommended)

[Enable Banking](https://enablebanking.com) is a licensed account-information
provider with self-serve signup. Its **restricted production** mode is designed
for exactly this case: you link *your own* accounts to your own application and
read them live. Every account goes through this single code path.

Nothing in that path is bank-specific: the app fetches the supported bank list
from `/aspsps` at runtime, so **any ASPSP Enable Banking covers works with no
code change**, in any of the ~30 countries they reach. In Finland that includes
S-Pankki, OP, Danske Bank, Aktia, Säästöpankki and Nordea.

**Accounts in several countries** work too. The country box next to *Load banks*
picks which country's list to browse, so linking a Swedish or German account is
the same flow run again — each consent is stored as its own session and a sync
covers all of them.

**The UK is covered, but thinly.** Enable Banking reaches it despite the UK
sitting outside PSD2 since Brexit, though only six institutions:
**Barclays, HSBC UK, NatWest** (incl. Mettle and Ulster Bank), **RBS, Coutts**
and **ABN AMRO UK** — all with production AISP and personal-account support.
Everything else British, Lloyds and the neobanks included, has to come in as a
statement file. S-Pankki authenticates through S-mobiili, which
is an ordinary redirect flow and needs no special handling.

Several banks can be linked at once — each consent is stored as its own session
and a sync fans out across every account in all of them. That matters when a
bank exposes its credit cards as a separate entry from its current accounts:
just run the connect flow twice.

**Credit cards** are handled as first-class accounts. A card has no IBAN, so it
is identified by the masked PAN the bank reports under `account_id.other`, and
`cash_account_type: CARD` is used to label it in the account list.

1. Register an application in the Enable Banking control panel.
2. Generate an RSA key pair and upload the public half:
   ```sh
   openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out enablebanking.pem
   openssl rsa -in enablebanking.pem -pubout -out enablebanking.pub
   ```
   Keep `enablebanking.pem` private — the app only stores its *path*.
3. Whitelist the redirect URL the Settings screen shows you
   (`http://127.0.0.1:8765/callback` by default).
4. Activate the application in restricted mode by linking your own accounts.
5. In **Settings**, enter the application ID, point at the private key, set your
   country, and save.
6. In **Banks**, load the bank list, pick your bank, complete the consent flow
   in the browser, then repeat for each account you want. Hit **Sync now**.

Consent lasts up to 90 days, after which the bank asks again.

### 2. Trading 212 (its own API)

Trading 212 is a broker, not a bank. It has no PSD2 interface — it *consumes*
open banking for deposits rather than exposing accounts — so the aggregation
route above cannot reach it. It does have a free
[public API](https://docs.trading212.com/api): generate a key pair in the app
under **Settings → API** with history access, then paste **both the key and the
secret** into **Settings → Trading 212**. The API authenticates with HTTP Basic,
taking the key as the username and the secret as the password. The pair is
written to an owner-only file rather than into the settings.

Card spending is fetched the long way round, and deliberately so. The API's
`/history/transactions` endpoint reports card payments with an opaque UUID
instead of a merchant name, which is useless for budgeting. The CSV the export
endpoint produces *does* carry `Merchant name` and `Merchant category`, so the
app requests an export, polls until it is built, downloads it and parses it.

The export endpoints are heavily rate limited — one request per 30 seconds to
ask for a report and one per minute to check on it — so a Trading 212 sync
takes a minute or two rather than the moment a bank sync takes. Polling any
faster just spends the budget on `429`s.

Two things about the export are worth knowing:

- **Share trades are dropped.** A market buy moves cash into holdings; it is not
  money leaving your pocket, and counting it would make investing look like
  spending.
- **Deposits and withdrawals are transfers**, because the other leg is already
  on the account that funded them.

### 3. Statement import (works immediately, no credentials)

Export a CSV from your bank, or from Trading 212's own history export, and use
**Banks → Import a statement**. This is the only route for a UK bank outside
the six above, so the UK layouts are handled explicitly: the **Debit
Amount / Credit Amount** column pair that Lloyds, Halifax and Bank of Scotland
use instead of a signed amount, Starling's `Amount (GBP)` heading, NatWest's
`Value`, and Barclays' `Memo`. Dates are read day-first.

Most UK exports name no currency anywhere, so the import row has a currency box
alongside the bank name — without it sterling would be filed as euros and
swallowed into the euro totals. A Trading 212 file is recognised from its
header and gets the same treatment as an API-fetched one, including the same
row ids, so importing a period you already synced changes nothing. Columns are
matched by header name, covering the export layouts the parser carries
signatures for, the Finnish, Swedish and English variants of the domestic
exports, and the shared Finnish vocabulary most banks use (`Maksupäivä`,
`Summa`, `Saajan nimi`, `Selite` and so on). Rows already pulled from the API
are not duplicated.

Only a couple of export layouts can be identified from their headers alone —
most banks share too much column vocabulary to tell apart, and guessing would
silently mislabel rows and break the per-bank filter. Leave the **From**
dropdown on *Detect automatically*, and name the bank yourself for any file it
does not recognise.

If a file does not import, the error message quotes the header row it found —
that is usually enough to see which column alias is missing.

## Appearance

Dark or light, following the system by default, with **Settings → Appearance**
to pin it either way. The choice survives a restart, and switching repaints the
open window rather than asking to be reopened.

Qt reads the system's colour scheme from whatever the desktop exposes: the
`org.freedesktop.appearance` portal on GNOME, Cinnamon, XFCE, MATE and the
rest, kdeglobals on KDE, and the device setting on Android. Both routes carry a
change signal, so flipping the system theme is picked up live.

Not every environment exposes one. LXQt and the tiling window managers (sway,
i3, Hyprland) report *Unknown*, and the app stays dark there — pick a palette
yourself, or start it with a portal-aware platform theme:

```sh
QT_QPA_PLATFORMTHEME=xdgdesktopportal
```

Both palettes live in `qml/Theme.qml` and nothing hard-codes a colour outside
it. The window palette is bound to the same singleton, so the controls Qt draws
for itself — spin box steppers, combo box popups, dialog chrome — change with
everything else instead of keeping the desktop's own scheme.

## Language

The interface is in English or Finnish, and follows the system by default: it
takes the first of the platform's preferred languages it has a catalogue for,
and falls back to English. **Settings → Language** overrides that, and the
choice survives a restart. Either way the switch applies immediately — the
window is retranslated in place rather than asking to be reopened.

Dates and decimal separators follow the same choice, so Finnish reads *syyskuu
2026* and `0,00` instead of borrowing whatever the desktop is set to. Where the
chosen language already matches the platform's, the platform's *region* is
kept, so an en_GB desktop still gets 31/12 rather than 12/31.

Two things stay in the language they arrived in. **Categories** are stored in
the database and used as filter keys, so translating them would orphan every
categorisation made so far; and anything the bank itself wrote is reproduced as
sent.

English is the source language and has no catalogue of its own —
`i18n/tarkka_fi.ts` is the only one. After adding or changing a `qsTr()` or
`tr()` string, re-extract with:

```sh
cmake --build build --target update_translations
```

## The icon

`assets/logo.svg` is the source of truth: a geometric T, the crossbar reading as
a datum line and the stem as the single figure resolved beneath it.

Everything raster is generated from it by `tools/make-icons.sh` — desktop window
icons and the Android mipmaps — so the mark cannot drift between platforms. The
in-app version is drawn in QML from the same proportions rather than loaded as
an image.

Two things the design had to survive, both found by looking at it rather than
reasoning about it: a **24px render**, which killed the variants with segmented
crossbars and ruler ticks, and Android's **circular mask**, which clipped the
first foreground layer's crossbar. The adaptive foreground is therefore framed
separately from the square tile, at 44% of the canvas.

## Building

`tools/build.sh` covers both platforms and knows where everything lives:

```sh
tools/build.sh                                  # desktop release
tools/build.sh --test                           # desktop, then the test suite
tools/build.sh -r                               # desktop, then launch it
tools/build.sh -p android -a arm64-v8a,x86_64   # a phone and an emulator
tools/build.sh -p android -a x86_64 -r          # build, install, launch
tools/build.sh --help                           # every option
```

It picks a JDK the Android Gradle plugin accepts, locates the SDK, NDK and the
right per-ABI Qt kit, and signs the release APK — which Gradle leaves unsigned
and therefore uninstallable. `--run` checks for an attached device before
building rather than after.

For an emulator, start it with `tools/emulator.sh`. The GPU flag matters more
than it looks: booting with the software rasteriser
(`-gpu swiftshader_indirect`) shreds Qt Quick's rounded rectangles, so buttons
and cards come out with a diagonal across them and half the shape unfilled.
That is the rasteriser, not the app — the identical APK is pixel-clean under
`-gpu host`. If a host has no usable GL, `TARKKA_EMULATOR_GPU=swiftshader_indirect`
will still boot it, but do not judge the UI from what it draws.

### Desktop, by hand

Requires Qt 6.5+ (built against 6.11), CMake 3.21+, OpenSSL and a C++20
compiler.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/tarkka
ctest --test-dir build --output-on-failure
```

### Android, by hand

Needs the Qt for Android kits, an Android SDK with build-tools and a platform,
and NDK 27. The UI switches to a bottom navigation bar and single-column cards
below 760px, so the same code drives both.

```sh
export JAVA_HOME=/usr/lib/jvm/java-21-openjdk
export ANDROID_SDK_ROOT=$HOME/Android/Sdk
export ANDROID_NDK_ROOT=$ANDROID_SDK_ROOT/ndk/27.0.12077973

~/Qt/6.11.1/android_arm64_v8a/bin/qt-cmake -S . -B build-android -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DQT_ANDROID_ABIS="arm64-v8a;x86_64"
cmake --build build-android --target apk
```

The release APK comes out unsigned; sign it before installing:

```sh
BT=$ANDROID_SDK_ROOT/build-tools/35.0.0
APK=build-android/android-build/build/outputs/apk/release/android-build-release-unsigned.apk
$BT/zipalign -f -p 4 $APK aligned.apk
$BT/apksigner sign --ks ~/.android/debug.keystore --ks-pass pass:android \
    --ks-key-alias androiddebugkey --out tarkka.apk aligned.apk
```

Two Android-specific notes:

- **Crypto.** Qt for Android ships no OpenSSL. Qt Network still needs it for
  HTTPS, so the build pulls in KDAB's prebuilts and bundles them; point
  `TARKKA_ANDROID_OPENSSL_DIR` at a local clone to build offline. RS256 token
  signing does *not* use them — it goes through Android's own JCA, so there is
  no second crypto implementation to keep patched.
- **Java 21.** Newer JDKs are not accepted by the Gradle plugin Qt generates.

## How it is put together

```
src/
  core/        Transaction value type, integer money, merchant categorisation
  data/        SQLite store + the models QML binds to
  net/         RS256 JWT signing (OpenSSL) and a small JSON HTTP client
  auth/        Loopback redirect listener, persisted settings
  i18n/        Language resolution and catalogue switching
  providers/   EnableBankingProvider (banks), Trading212Provider (broker API)
               and CsvProvider (import), behind one Provider interface
  app/         AppController — the single object QML talks to
qml/           Main window, four views, reusable components
tests/         Unit tests for money, CSV, categorisation, storage, JWT and
               the enumerated settings
```

Providers share one `Provider` interface and emit normalised `Transaction`
rows, so the live API and a CSV file are indistinguishable downstream. The
SQLite store is the source of truth: the UI reads only from it, which is why
the app stays usable offline and why a sync that fails halfway never leaves a
half-populated screen.

A few decisions worth knowing about:

- **Money is never a float.** Amounts are signed integer minor units end to
  end; parsing and formatting are the only places a decimal exists.
- **Rows are deduplicated on a stable id** — the bank's own reference when it
  supplies one, otherwise a hash of date, amount, description and an ordinal so
  two identical coffees on the same day stay two rows.
- **A category you set by hand outranks the categoriser** and survives
  re-syncing.
- **Pending rows are shown as pending** and are overwritten in place when the
  bank books them, including a corrected amount.
- **Transfers between your own accounts are left out of the totals.** Paying a
  credit card bill, or moving money from one account to another, appears once
  on each side; counting either leg would inflate both spending and income. The
  net card says how much was excluded.
- **Currencies are never added together.** Minor units only mean anything
  within one currency, so the dashboard shows exactly one at a time, defaulting
  to whichever has the most rows, with a picker when there is more than one.
  There is no FX conversion: an invented exchange rate would be a worse answer
  than an honest per-currency total.
- **Portfolio activity is not spending.** Share trades are excluded; dividends,
  interest and cashback are counted as income.
- **Transaction direction follows the bank.** `credit_debit_indicator` decides
  it when present, and the amount's own sign when it is absent, which is what
  the Berlin Group schema specifies.

## Where your data lives

| What | Where |
|---|---|
| Transactions | `~/.local/share/tarkka/Tarkka/transactions.sqlite` |
| Category rules | `~/.local/share/tarkka/Tarkka/categories.json` |
| Application ID, key path, sessions | `~/.config/tarkka/Tarkka.conf` |
| Trading 212 key and secret | `~/.local/share/tarkka/Tarkka/trading212.key` (owner-only) |

On Android the same files live inside the app's private storage.

Nothing is sent anywhere except to the Enable Banking API, and the private key
is read from the path you configure rather than copied into the app's own
storage.

Edit `categories.json` to change how purchases are grouped — it is a plain
`{"Category": ["regex", ...]}` map, applied in order, first match wins.
