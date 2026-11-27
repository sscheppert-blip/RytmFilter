# Dev-Log

Wie RytmFilter entstanden ist. Entwickelt von sscheppert-blip im Pair-Programming mit Claude (Anthropic) als
Co-Autor. [← zurück zur README](../README.md)

---

## Tag 1 – Vom Stimmgerät zum Filter

### Start: „HzToMidi“

Ausgangspunkt war eine ganz andere Idee: ein Messgerät-Plugin, das für eine eingehende Frequenz die MIDI-Note,
den Notennamen und die Abweichung in Cent anzeigt. Gebaut auf JUCE mit YIN-Tonhöhenerkennung.

Unterwegs gelernt:
- **Oktavfehler bei hohen Tönen** – YIN rastete auf Subharmonische ein. Gelöst mit einer Prüfung auf
  Subharmonische mit gebrochenem Lag.
- **Wackelige tiefe Töne** – Suche über die ganze Talregion der Differenzfunktion statt nur des ersten Minimums.
- **UI zu klein** – Wunsch nach frei skalierbarer, vektorbasierter Oberfläche („als SVG, damit es gezoomt hübsch
  bleibt“). Daraus entstand das Skalierungskonzept: Layout in fester Basisgröße, Skalierung per
  `AffineTransform`, alles als Vektorpfade gezeichnet, Zoomstufe im Plugin-State gespeichert.
- **Standard-Drehregler hässlich** – eigene `LookAndFeel` mit flachen, von oben gesehenen Encodern.

### Kurswechsel

Das Messgerät wurde verworfen. Neues Ziel: ein Filter-Plugin, das sich anfühlt wie die FLTR-Seite eines
schwedischen Drumcomputers – Punktmatrix-Display, mattschwarzes Gehäuse, mattschwarze Encoder, alle Werte
0–127. „Keine 1:1-Kopie, aber nah am Original.“ Zusätzlich gewünscht: eine Hz-Anzeige, wo der Filter gerade
greift.

Entscheidungen:
- **Name:** RytmFilter
- **Umfang:** nur die Filterseite (kein Overdrive, kein LFO)
- **Hüllkurven-Auslöser:** wählbar – MIDI, Transienten im Audio oder Host-Tempo – auf einer eigenen TRIG-Seite
- **Aufräumen:** alter Code komplett raus, nur das CMake/JUCE-Gerüst bleibt

---

## Architektur

| Baustein | Umsetzung |
|---|---|
| Filter | Zustandsvariablenfilter (Trapez-Integration, Cytomic-Topologie), 2× Oversampling |
| Filtertypen | LP2/HP2/BP direkt aus dem SVF; LP1 = LP+BP, HP1 = HP+BP (bei RES 0 exakt 1-polig); BS = x − k·BP; PK als Glocke |
| Resonanz | RES 0–127 → Q 0,5 … 30 (exponentiell) |
| Frequenz | FRQ 0–127 → 20 Hz … 20 kHz (exponentiell) |
| Hüllkurve | ADSR, linearer Attack, exponentielles Decay/Release, Retrigger ab aktuellem Pegel; Zeiten 0,5 ms … 10 s |
| Modulation | Koeffizienten pro Sample → keine Zipper-Geräusche bei schnellen Hüllkurven |
| Transienten | schnelle vs. langsame Hüllkurve mit Sperrzeit, Empfindlichkeit regelbar |
| Display | eigener 128×64-1-Bit-Framebuffer mit eigener 5×7-Pixelschrift; jedes Pixel ein Vektor-Rechteck |
| UI | feste Basisgröße 1000×420, skaliert 60–250 %, fixes Seitenverhältnis |

Die Herstelleranleitung nennt keine Hz- oder ms-Werte – die Kennlinien für FRQ, RES und die Hüllkurvenzeiten
sind deshalb eigene, ans Gehör angelehnte Annahmen.

---

## Bugs & Fixes

| Problem | Ursache | Lösung |
|---|---|---|
| Resonanzspitze 1,4 dB zu niedrig | `tanh`-Sättigung der Integratoren dämpfte schon bei normalem Pegel | Begrenzer, der bis ±2 exakt linear ist und erst darüber weich abknickt |
| Fußzeile im Display abgeschnitten („TRG:MTDT“) | Text startete bei y = 58 von 64 | Alle Zeilen neu verteilt |
| Inaktive Werte unlesbar | Dither-Muster über Text | gepunktete Rahmen statt Dither |
| Kopfzeilen-Text klebt am Rahmen | feste Boxbreite | Box passt sich der Textbreite an |
| Schmale Notch im Display unsichtbar | nur ein Stützpunkt pro Spalte | 9-fache Abtastung pro Spalte, vertikale Spanne zeichnen |
| Fenster startete in falscher Größe | gespeicherte Zoomstufe wurde nach `setResizeLimits` gelesen | Reihenfolge getauscht, Guard gegen frühe Resize-Callbacks |

### Tests

`Tests/FilterTest.cpp` prüft automatisch:
- Mappings (FRQ↔Hz, Zeiten)
- analytische Flankensteilheit aller Typen (LP1 −3 dB an der Grenzfrequenz, 6/12 dB pro Oktave, BS-Kerbe,
  BP 0/+12 dB, PK +24 dB)
- diskreter Filter vs. analytischer Frequenzgang (Abweichung < 0,5 dB bei 96 kHz)
- Stabilität bei RES 127 mit Rauschen und schnellem Sweep
- Hüllkurvenzeiten
- Transienten-Erkennung (8 von 8 Schlägen)

Zusätzlich: pluginval (Strenge 7) und `auval` ohne Fehler.

---

## Tag 1, später – Open Source & CLAP

- **CLAP-Support:** JUCE 8 spricht CLAP nicht von Haus aus. Eingebunden über
  [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions) (per CMake `FetchContent`,
  auf einen Commit festgenagelt).
- **clap-validator fand einen echten Bug:** Der Anzeigetext von FRQ rundete den Wert, rechnete die Hz aber aus
  dem ungerundeten Wert – Text → Wert → Text war dadurch nicht stabil (`1 (21.4453 Hz)` vs. `1 (21.118 Hz)`).
  Fix: Hz aus dem gerundeten Wert berechnen. Danach 0 Fehler.
- **Build ohne lokales JUCE:** JUCE 8.0.12 wird automatisch per `FetchContent` geladen; ein lokaler Checkout ist
  per `-DJUCE_DIR` weiterhin möglich.
- **AU nur auf macOS**, VST3 + CLAP überall.
- **Windows:** statische C++-Laufzeit → kein Visual-C++-Redistributable nötig.
- **CI:** GitHub Actions baut macOS (Apple Silicon), Windows x64 und Linux x64, führt die DSP-Tests aus und
  hängt die Pakete bei Tags (`v*`) an ein Release.
- **Lizenz:** AGPL-3.0 (passend zur Open-Source-Lizenz von JUCE 8). Die Herstelleranleitung liegt bewusst nicht
  im Repository.

---

## Version 1.1 – Näher am Original

Recherche-Material (Handbuch, Notizen, ein SysEx-File) lokal gesammelt. Das SysEx-File stellte sich als
**Firmware-Update** heraus, nicht als Sound-Dump – und weil der Filter im Original analog ist, steckt sein Klang
ohnehin nicht in der Firmware. Aus fremder Firmware Daten zu übernehmen, kam für ein AGPL-Projekt auch rechtlich
nicht infrage. Recherche-Ordner `resources/` ist per `.gitignore` ausgeschlossen.

| Änderung | Umsetzung |
|---|---|
| Nur 128 Stufen | Alle Parameter mit Schrittweite 1 (`NormalisableRange` mit Intervall) – auch für Host-Automation. Filter-Modulation bleibt intern stufenlos |
| Encoder | Schlanke, glatte Endlos-Encoder ohne Positionsmarke – der Wert steht im Display |
| FRQ in Halbtönen | `f = 440 · 2^((FRQ − 60) / 12)` → 13,75 Hz … 21,1 kHz (Annahme aus der Recherche, nicht am Gerät gemessen) |
| Alte Projekte | v1.0-Zustände tragen kein `frqCurve`-Attribut → FRQ wird beim Laden auf den nächstgelegenen Halbton derselben Frequenz umgerechnet (max. ½ Halbton Abweichung) |
| Selbstoszillation | Dämpfung `k` sinkt ab RES 88 linear weiter und wird ab RES 96 negativ. Pegelabhängige Zusatzdämpfung `k + limit · (bp² + lp²)` (Van-der-Pol-artig) hält die Schwingung als sauberen Sinus bei ca. −6 dBFS. Winziges Rauschen (−100 dBFS) stößt sie ohne Eingangssignal an |
| TRIG-IN-LED | Wurde beim Blinken an der falschen Stelle neu gezeichnet → Repaint-Bereich korrigiert |
| Transienten-Erkennung verschluckte Schläge | Alter Detektor verglich eine schnelle mit einer langsamen Hüllkurve (150 ms Abfall). Bei 16teln hing die langsame noch vom vorigen Schlag oben → bei einem Muster wie 0-1-1-1 kam nur der erste Schlag durch. Neu: Pegelsprung gegenüber dem Pegel von vor 5 ms, Hörsignal mit Höhenanhebung ab 150 Hz (Bass verdeckt weniger), Wiederscharfschaltung erst nach abgeklungenem Sprung, frühestens alle 25 ms. Getestet bis 16tel @174 BPM und 32tel @150 BPM, über Dauer-Basston, ohne Doppel-Trigger bei langen 45-Hz-Kicks |

Kurz ausprobiert und wieder verworfen: eine AMP-Seite mit Overdrive und Lautstärke wie am Gerät sowie Tooltips.
RytmFilter bleibt bewusst **nur ein Filter** mit schlanker Oberfläche.

**MIDI-Trigger hinter einem Instrument:** Steht RytmFilter auf derselben Spur hinter einem Instrument, verbraucht
das Instrument die MIDI-Noten – beim Plugin kommen keine an. Das ist DAW-Routing, kein Plugin-Fehler: Die Noten
müssen von einer eigenen MIDI-Spur an RytmFilter geschickt werden (siehe Anleitung) oder TRNS/SYNC nutzen.

Tests ergänzt: Halbton-Kennlinie, Umrechnung alter Projekte, Einsatzpunkt/Pegel/Tonhöhe der Selbstoszillation,
Transienten bei schnellen Tempi, Oversampling-Latenz (< 5 Samples). pluginval (Strenge 7) ohne Fehler.

---

## Ideen für später

- LFO
- Kennlinien von FRQ/RES/Hüllkurve am echten Gerät nachmessen und angleichen
- Signierte/notarisierte macOS-Builds
- Presets
