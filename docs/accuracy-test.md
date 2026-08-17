# Accuracy Test — Characterising the Antenna Phase-Centre Offset

The rover reports σ values of a few millimetres. Those describe the RTK engine's confidence in
its own solution; they say nothing about where the antenna's electrical phase centre actually
sits relative to the hardware you measure to. This dome has no ANTEX calibration, so that
offset is unknown — see
[README → Antenna phase centre](../README.md#antenna-phase-centre--the-uncharacterised-part).

This document is the plan to measure it.

- [What this test can and cannot resolve](#what-this-test-can-and-cannot-resolve)
- [Candidate control points](#candidate-control-points)
- [Prerequisites](#prerequisites)
- [Test A — absolute height (finds the offset)](#test-a--absolute-height-finds-the-offset)
- [Test B — height difference (validates the setup)](#test-b--height-difference-validates-the-setup)
- [Reduction](#reduction)
- [Sanity assertions](#sanity-assertions)
- [Interpreting the result](#interpreting-the-result)
- [Results](#results)
- [Data sources and attribution](#data-sources-and-attribution)

---

## What this test can and cannot resolve

Control points in Baden-Württemberg publish **normal heights in DHHN2016 (NHN)**. GNSS measures
**ellipsoidal height**. Converting between them requires the GCG2016 quasigeoid, whose stated
accuracy is **~1 cm in flatland and ~2 cm in mountains**.

That model uncertainty is the floor of this test. It is larger than the numbers the receiver
reports about itself.

| Error source | Magnitude | Behaviour |
|---|---|---|
| GCG2016 quasigeoid model | **~1–2 cm** | Systematic, dominates |
| Height transfer from mark to antenna (staff + level) | ~2–3 mm | Random, negligible here |
| RTK vertical solution, fixed, settled | ~10–15 mm | Mostly random, averages down over a session |
| Antenna phase-centre offset | **unknown — the quantity being measured** | Systematic, constant |

Therefore:

- ✅ **Can** detect an offset of several centimetres — the magnitude that would actually mislead
  a user.
- ❌ **Cannot** confirm or refute a millimetre-level offset. No free height point in Germany
  will, because the geoid model is the limit.

Stating this is part of the result. A number without its error budget is what got us here.

**To beat the 2 cm floor** you need a point with a published **ETRS89 ellipsoidal height** — a
Geodätischer Grundnetzpunkt (GGP) or a TP record that carries one — which removes the geoid
model from the chain entirely. Worth one email to `geodaten@lgl.bwl.de`; do not expect a
convenient one nearby.

---

## Candidate control points

All are **Höhenfestpunkte 1. Ordnung**, from the LGL AFIS open-data WFS. Heights are DHHN2016
(NHN), given to the millimetre.

**Test in the Rhine plain around Bühl, not in the Bühlertal valley.** The phase-centre offset
is a property of the antenna, not of the site — so measure it at the best site reachable. The
valley imposes a terrain horizon of 20°+ over whole sectors, which biases geometry
systematically and in the same direction every session. That is precisely the error class this
test is trying to isolate, so do not invite it.

### Primary

| Punktkennzeichen | NHN height | Marker | Location | UTM32 (E / N) | Map |
|---|---|---|---|---|---|
| **7314900059** | **139.555 m** | Pfeilerbolzen, Naturstein | Friedenskreuz path, Bühl | 435804.11 / 5392229.93 | [48.679812, 8.127890](https://www.google.com/maps?q=48.679812,8.127890) |
| 7314900286 | 155.620 m | Mauerbolzen, vertikal | Sign foundation, free-standing | 437822.36 / 5394190.07 | [48.697648, 8.155010](https://www.google.com/maps?q=48.697648,8.155010) |
| 7314900085 | 137.631 m | Turmbolzen mit Aufschrift TP | Rathaus tower, Bühl | 436407.90 / 5393965.88 | [48.695489, 8.135824](https://www.google.com/maps?q=48.695489,8.135824) |

`7314900059` is the pick: a pillar bolt in natural stone on a footpath — free-standing and
outdoors, rather than set into somebody's house wall. `7314900085` is on a tower and cannot be
occupied; it is listed because its `TP` inscription means a trigonometric point is co-located,
which may carry horizontal coordinates worth requesting.

### Alternates

| Punktkennzeichen | NHN height | Marker | Location | UTM32 (E / N) | Map |
|---|---|---|---|---|---|
| 7314900078 | 137.547 m | Mauerbolzen, vertikal | K3764, wayside cross, Ottersweier | 435333.03 / 5391742.65 | [48.675380, 8.121567](https://www.google.com/maps?q=48.675380,8.121567) |
| 7214900023 | 135.062 m | Mauerbolzen, horizontal | B3, field cross, Bühl | 436974.55 / 5395274.14 | [48.707314, 8.143324](https://www.google.com/maps?q=48.707314,8.143324) |
| 7314900080 | 136.946 m | Mauerbolzen, horizontal | K3764, drainage structure, Bühl | 435621.57 / 5392370.72 | [48.681060, 8.125389](https://www.google.com/maps?q=48.681060,8.125389) |
| 7314900008 | 140.364 m | Mauerbolzen, horizontal | Lindenkirche, Ottersweier | 435181.80 / 5391405.30 | [48.672330, 8.119566](https://www.google.com/maps?q=48.672330,8.119566) |

The wayside-cross and field-cross points sit in open farmland, which is good for sky view.

> Lat/lon are converted from the published EPSG:25832 values for map convenience only. **Use
> the UTM32 values and the location description on site** — the map pin is approximate, the
> published coordinate is not.

### Pull the data yourself

```
https://owsproxy.lgl-bw.de/owsproxy/wfs/WFS_LGL-BW_AFIS_Hoehenfestpunkte
  ?service=WFS&version=2.0.0&request=GetFeature
  &typeNames=nora:v_hoehenfestpunkt_ordnung_1
  &outputFormat=application/json
  &bbox=434000,5391000,441000,5397000,urn:ogc:def:crs:EPSG::25832
```

Feature types: `nora:v_hoehenfestpunkt_ordnung_1` (1. Ordnung), `_2` (2. Ordnung), `_3`
(Nivellementpunkte). Also loads directly into QGIS as a WFS layer, alongside the LGL orthophoto
WMS for sky-view screening.

---

## Prerequisites

**Define your Antenna Reference Point (ARP) and never change it.** Pick a physical feature of
the dome you can put a tape on — the shoulder of the 5/8″ thread, or the flat underside of the
mount — photograph it, and record the choice here. Every height in this test is referred to
that point. An undocumented ARP makes the whole exercise unrepeatable.

- Tripod or survey pole with a **fixed, known** height, ideally with a bubble level
- Levelling staff and a spirit or laser level for the height transfer
- SAPOS BW account — [Open SAPOS](https://www.sapos-bw.de/dienste.php) is free of charge
  (one-time €150 administrative fee per account); mountpoint `VRS_3_4G_BW`. As of 1 Jan 2026
  only HEPS and GPPS remain
- A phone running SW Maps with **Log To File** enabled, or the
  [dashboard](https://hmd83.github.io/ESP_RTK_ROVER/web/rtk-monitor.html) raw NMEA log

**Use SAPOS for this test, not Centipede or RTK2go.** The network's reference antennas are
themselves calibrated and its frame is unambiguous, which removes a variable rather than adding
one.

---

## Test A — absolute height (finds the offset)

You do **not** need to stand on the mark. Almost every HFP here is a wall bolt you cannot
centre over, and for a vertical test that does not matter: set up wherever the sky is best
within a few metres, then transfer the height across. The transfer costs a few millimetres
against a 2 cm floor.

1. Set the rover up within a few metres of the mark, **choosing the spot for sky view** — away
   from walls, vehicles, trees and overhead lines.
2. Transfer the mark's height to your ARP with staff and level. Record the transfer, with a
   photo. → `H_ARP` (NHN).
3. Connect SAPOS, wait for **RTK FIXED**, then let it settle at least **5 minutes** before
   recording anything.
4. Log **≥ 15 minutes** of NMEA at 1 Hz without touching the setup.
5. Record correction age, satellite count and PDOP throughout. Discard the session if the
   solution drops out of fixed.
6. **Repeat on a different day and at a different time of day.** Different satellite geometry
   and different multipath is the entire point; two sessions an hour apart prove nothing.

---

## Test B — height difference (validates the setup)

Occupy two marks and compare the measured height difference against the levelled one. Over a
short baseline the geoid error is common-mode and largely cancels, so this checks your
procedure to a much finer tolerance than Test A can.

**A constant antenna offset also cancels in a difference.** This test therefore cannot find the
offset — it exists to prove that everything *else* is sound, so that Test A's residual can be
attributed rather than guessed at.

Suggested pair: `7314900059` and `7314900080` — ~1.5 km apart, both on the same rural road.

---

## Reduction

**Use ellipsoidal height. Never use the GGA MSL field.**

GGA reports MSL altitude computed with the receiver's own internal geoid model, which is not
accurate enough and is not GCG2016. But GGA also carries the geoid separation it used, so
adding the two recovers the ellipsoidal height exactly, independent of that model:

```
h_ellipsoidal = GGA altitude (field 9) + GGA geoid separation (field 11)
```

The dashboard already does this — the **Ellipsoidal ht** cell in the GGA panel. That is the
number to record.

Then:

1. Average `h_ellipsoidal` across the session → `h_measured`
2. Convert to DHHN2016 using the **BKG GCG2016** service (LGL's own Höhentransformationsdienst
   converts only between physical frames — DHHN12/92/2016 — and will not do this step) →
   `H_measured`
3. **Offset = `H_measured` − `H_ARP`**

A positive offset means the receiver places the phase centre above the point you measure to.

---

## Sanity assertions

Check these before believing any result. Each catches a different blunder.

| Assertion | Expected | Catches |
|---|---|---|
| Quasigeoid undulation ζ = `h_ellipsoidal` − `H` | **47–50 m** in Baden-Württemberg | Frame or sign error |
| Test B residual | < 1–2 cm | Setup, ARP or transfer blunder |
| Session-to-session scatter of `h_measured` | < 2 cm | Unstable site, multipath, marginal fix |
| Correction age throughout | < 2 s | Degraded corrections invalidating the session |

If ζ lands outside 47–50 m, you have a coordinate-system error, not a discovery.

---

## Interpreting the result

| Measured offset | Conclusion |
|---|---|
| < 2 cm | **Not resolvable by this method.** Report as bounded: "offset < 2 cm, limited by GCG2016" |
| 2–5 cm | Real and characterisable. Publish it as a constant correction |
| > 5 cm | Suspect a blunder first — ARP, height transfer, frame. Re-check before publishing |

Whatever comes out gets published here, including if it is unflattering. A rover whose bias is
measured and stated is more useful than one whose bias is unknown and flattering.

---

## Results

*Not yet run.*

| Date | Point | Sessions | h_measured | H_measured | H_ARP | Offset | Notes |
|---|---|---|---|---|---|---|---|
| — | — | — | — | — | — | — | — |

---

## Data sources and attribution

- **Höhenfestpunkte** — Landesamt für Geoinformation und Landentwicklung Baden-Württemberg
  (LGL), AFIS open-data WFS. Licence: *Datenlizenz Deutschland – Namensnennung – Version 2.0*
  (`dl-de/by-2-0`). Attribution: **© LGL Baden-Württemberg (dl-de/by-2-0)**, see
  [lgl-bw.de/agb](https://www.lgl-bw.de/agb/)
- **GCG2016 quasigeoid** — Bundesamt für Kartographie und Geodäsie (BKG), open data
- **Corrections** — SAPOS® Baden-Württemberg (LGL)
- **Point locations** — [LGL Festpunktportal](https://festpunktportal.lgl-bw.de/festpunkte/)
