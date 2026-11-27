# RytmFilter

**Ein Filter-Plugin im Stil eines Hardware-Drumcomputers – mit Punktmatrix-Display, mattschwarzen Drehreglern und
einer Anzeige, die dir live verrät, bei welcher Frequenz der Filter gerade arbeitet.**

![RytmFilter](docs/screenshot.png)

## Was ist das?

RytmFilter ist ein Audio-Effekt für deine Musiksoftware (DAW). Er formt den Klang wie die Filtersektion eines
klassischen Drumcomputers: Höhen wegnehmen, Bässe ausdünnen, eine Frequenz herausheben oder aushöhlen – und
das Ganze per Hüllkurve rhythmisch „atmen“ lassen.

Die Idee: Der schnelle Workflow einer Hardware-Kiste, aber direkt im Rechner. Acht Endlos-Regler, ein Display,
alle Werte in ganzen Schritten von 0 bis 127 – kein Menü-Labyrinth.

## Was kann es?

- **7 Filtertypen** – Tiefpass (zwei Varianten), Bandpass, Hochpass (zwei Varianten), Kerbfilter und Glocke
- **Resonanz** – vom sanften Abrunden bis zur Selbstoszillation: ab RES 96 pfeift der Filter von selbst
- **Frequenz in Halbtönen** – wie am Original: FRQ 60 = 440 Hz, 12 Schritte = eine Oktave
- **Hüllkurve** – lässt den Filter auf- und zugehen, ausgelöst durch MIDI-Noten, durch Schläge im Audiosignal
  oder automatisch im Songtempo
- **Live-Frequenzanzeige** – oben im Display steht, wo der Filter gerade greift (z. B. `F:2.14KHZ`),
  daneben ein Bild der Filterkurve, das sich mitbewegt
- **Frei skalierbar** – das Fenster lässt sich stufenlos vergrößern und bleibt dabei gestochen scharf.
  Gut für müde Augen.

![Trigger-Seite](docs/screenshot-trig.png)

## Wo läuft es?

| System | Formate | Paket |
|---|---|---|
| macOS 11 oder neuer, Apple Silicon (M1 …) | VST3 · AU · CLAP | `RytmFilter-macOS-arm64.zip` |
| Windows 10 / 11, 64 Bit (x64) | VST3 · CLAP | `RytmFilter-Windows-x64.zip` |
| Linux 64 Bit (amd64), z. B. Ubuntu 22.04 oder neuer | VST3 · CLAP | `RytmFilter-Linux-x64.zip` |

Unter Windows wird keine zusätzliche Laufzeit (Visual-C++-Redistributable) benötigt.

## Neu in Version 1.1

- Alle Regler in 128 ganzen Schritten wie am Gerät – auch bei Automation in der DAW
- FRQ in Halbtönen (FRQ 60 = 440 Hz), Projekte aus 1.0 werden beim Laden automatisch umgerechnet
- Echte Selbstoszillation ab RES 96 – der Filter wird zum stimmbaren Sinus-Oszillator
- Transienten-Erkennung (TRNS) zuverlässiger, auch bei schnellen 16teln und 32teln
- Schlankere Endlos-Encoder ohne Markierung, korrigierte TRIG-IN-LED

## Herunterladen & loslegen

1. Unter **[Releases](../../releases)** das Paket für dein System laden.
2. Entpacken und die Plugin-Dateien in den Plugin-Ordner kopieren.
3. DAW neu starten bzw. Plugins neu einlesen lassen.

Wie genau das geht und was jeder Regler macht, steht in der **[Anleitung](ANLEITUNG.md)**.

## Gut zu wissen

- Das Projekt ist kostenlos und quelloffen (Lizenz: [AGPL-3.0](LICENSE)).
- Die Plugins sind nicht von Apple/Microsoft signiert. Beim ersten Start meckert das System eventuell – in der
  Anleitung steht, wie man das freigibt.
- RytmFilter ist ein Hobbyprojekt und klingt *inspiriert vom* Original, nicht identisch. Es ist kein offizielles
  Produkt und steht in keiner Verbindung zu Elektron. Alle Markennamen gehören ihren jeweiligen Inhabern.

## Wer steckt dahinter?

Gebaut von **[sscheppert-blip](https://github.com/sscheppert-blip)** zusammen mit **Claude** (KI-Assistent von
Anthropic) als Co-Autor, der den Großteil des Codes geschrieben hat.[^devlog]

[^devlog]: Für Neugierige: Wie das Plugin entstanden ist, welche Entscheidungen getroffen wurden und welche Bugs
    unterwegs aufgetaucht sind, steht im [Dev-Log](docs/DEVLOG.md).
