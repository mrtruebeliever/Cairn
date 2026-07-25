# Art-prompts voor Cairn-graphics

*(Heet nog CHATGPT_PROMPTS.md, maar de briefs werken voor elke generator —
ChatGPT, een Artlist-MCP, wat dan ook. `tools/gen_badges.py` accepteert zowel
zes losse bestanden `tools/art/badge_0.png`..`badge_5.png` als één sheet
`tools/art/badges_sheet.png`.)*

Palet van de app is **"nachtgetij"** (zie `src/c/config.h`): zwarte nachtlucht,
teal water, kalksteen-oever, oplichtend schuim, geel accent. Géén oranje, géén
marineblauw. Alle prompts hieronder gebruiken exact die kleuren.

| Kleur | Hex | Waarvoor |
|---|---|---|
| Zwart | `#000000` | nachtlucht / achtergrond |
| Teal | `#00AAAA` | water net onder het oppervlak |
| Diep teal | `#005555` | water in de diepte |
| Celeste | `#AAFFFF` | schuimlijn, secundaire waarden |
| Kalksteen | `#AAAA55` | de oever |
| Lichtgrijs | `#AAAAAA` | kiezels |
| Donkergrijs | `#555555` | kiezels (afwisselend), panelen |
| Icterine | `#FFFF55` | accent: score, reeks, bovenste kiezel |

Wat de app zelf tekent met primitives (waterlijn, cairn, trendpijlen, kleine
badges op het voortgangsscherm) heeft geen ChatGPT nodig. Wat wél als bitmap komt:

| Wat | Formaat in de app | Bron |
|---|---|---|
| Launcher-menu-icoon | 25×25 PNG | al gegenereerd door `tools/gen_icon.py`; prompt 1 alleen als je 'm mooier wil |
| 6 badge-medailles (felicitatiepagina na een sessie) | 72×72 PNG elk | **prompt 2** |
| Store-icoon 80×80 en 144×144 | PNG | **prompt 3** (pas bij release) |
| Store-banner 720×320 | PNG | **prompt 4** (pas bij release) |

Werkwijze voor prompt 2: laat ChatGPT één sheet maken, sla die op als
`tools/art/badges_sheet.png`, dan snijdt en verkleint `tools/gen_badges.py` de
zes cellen naar het PT2-palet.

---

## Prompt 1 (optioneel) — launcher-icoon

> Maak een vierkante afbeelding van 1024×1024 pixels met een **witte
> achtergrond**. In het midden staat een **cairn**: vier gestapelde, afgeronde
> kiezels, breed onderaan en smal bovenaan, op een korte horizontale
> oeverlijn. Stijl: platte vectorillustratie, dikke vormen, geen verloopjes,
> geen schaduw, geen tekst, geen kader. Kleurenpalet exact: kiezels afwisselend
> lichtgrijs `#AAAAAA` en donkergrijs `#555555`, de bovenste kiezel helder geel
> `#FFFF55`, de oeverlijn kalksteen `#AAAA55`. Laat rondom minstens 12% marge
> vrij. Het geheel moet leesbaar blijven als het naar 25×25 pixels wordt
> verkleind — dus grote, eenvoudige vormen en geen fijne details.

## Prompt 2 — sheet met 6 badge-medailles (dit heb ik nodig)

> Maak één afbeelding van **1536×1024 pixels** met een **volledig witte
> achtergrond**, ingedeeld als een raster van **3 kolommen × 2 rijen** met zes
> gelijke cellen van 512×512. In elke cel staat precies één **ronde medaille**,
> gecentreerd, met minstens 10% witte marge rond de medaille. Geen rasterlijnen,
> geen tekst, geen labels, geen nummers behalve waar hieronder gevraagd.
>
> Alle zes medailles hebben dezelfde opbouw: een dikke ring in helder geel
> `#FFFF55`, daarbinnen een veld in diep teal `#005555`, en daarin een symbool
> in lichtblauw-wit `#AAFFFF`. Platte vectorstijl, dikke lijnen, geen
> verloopjes, geen glans, geen schaduw, geen 3D. (Deze medailles komen op een
> zwarte achtergrond te staan, dus het geel moet echt fel zijn.)
>
> De zes symbolen, in leesrichting (links naar rechts, boven naar onder):
> 1. één enkele afgeronde kiezel
> 2. een cairn van drie gestapelde kiezels
> 3. een zon met precies zeven rechte stralen
> 4. drie concentrische cirkels met een gevulde stip in het midden
> 5. het getal **100** in een dikke schreefloze letter
> 6. twee gestapelde golflijnen (een getijdensymbool)
>
> Belangrijk: elke medaille moet herkenbaar blijven als hij naar 48×48 pixels
> wordt verkleind. Gebruik dus zeer dikke lijnen (minstens 6% van de
> medaillediameter) en vermijd fijne details.

## Prompt 3 — store-iconen (pas bij release)

> Maak een vierkante afbeelding van 1024×1024 pixels. Achtergrond: de bovenste
> tweederde effen **zwart** `#000000` (nachtlucht), de onderste derde **teal**
> `#00AAAA` water met daaronder een band **diep teal** `#005555`. Op de grens
> een dunne, licht golvende schuimlijn in `#AAFFFF`. Op die waterlijn staat een
> **cairn** van vier gestapelde afgeronde kiezels (afwisselend `#AAAAAA` en
> `#555555`, de bovenste `#FFFF55`) op een korte oeverlijn in `#AAAA55`; de
> onderste kiezel staat half in het water. Platte vectorstijl, geen verloopjes,
> geen tekst, geen kader, 12% marge rondom.

## Prompt 4 — store-banner (pas bij release)

> Maak een liggende afbeelding van **1440×640 pixels**. De bovenste helft is
> effen **zwart** `#000000` (nachtlucht), de onderste helft **teal** `#00AAAA`
> water dat naar onderen overgaat in een band **diep teal** `#005555`. Op de
> grens tussen lucht en water een golvende schuimlijn in `#AAFFFF` die over de
> volle breedte loopt. **Rechts van het midden** staat een cairn van vijf
> gestapelde afgeronde kiezels (`#AAAAAA` / `#555555`, bovenste `#FFFF55`) op
> een oeverlijn in `#AAAA55`, met de onderste twee kiezels onder de waterlijn.
> Verder helemaal leeg — **geen tekst, geen logo, geen letters**, want de titel
> wordt later toegevoegd. Platte vectorstijl, geen verloopjes binnen de vormen,
> geen schaduwen.

## Prompt 2b — de zes badges los (voor generators die één beeld per keer maken)

Gemeenschappelijke basis, plak dit voor elk van de zes symbolen:

> Een vierkante afbeelding van 512×512 pixels, **volledig witte achtergrond**.
> In het midden één **ronde medaille** met minstens 10% witte marge eromheen.
> Opbouw: een dikke ring in helder geel `#FFFF55`, daarbinnen een veld in diep
> teal `#005555`, en daarin een symbool in lichtblauw-wit `#AAFFFF`. Platte
> vectorstijl, dikke lijnen, geen verloopjes, geen glans, geen schaduw, geen 3D,
> geen tekst behalve waar gevraagd. De medaille komt op een zwarte achtergrond te
> staan, dus het geel moet echt fel zijn. Hij moet herkenbaar blijven op 48×48
> pixels: zeer dikke lijnen (minstens 6% van de diameter), geen fijne details.
> **Het symbool is:** …

| Bestand | Badge | Symbool (vul in op de …) |
|---|---|---|
| `badge_0.png` | Eerste adem | één enkele afgeronde kiezel |
| `badge_1.png` | Standvastig | een cairn van drie gestapelde kiezels |
| `badge_2.png` | Zen-week | een zon met precies zeven rechte stralen |
| `badge_3.png` | Diepe rust | drie concentrische cirkels met een gevulde stip in het midden |
| `badge_4.png` | Eeuw | het getal 100 in een dikke schreefloze letter |
| `badge_5.png` | Getij | twee gestapelde golflijnen |

---

Zet de resultaten in `Cairn/tools/art/` (als `badge_0.png`..`badge_5.png`, of als
één `badges_sheet.png`) en zeg het — dan draai ik `tools/gen_badges.py`, dat ze
bijsnijdt, naar 72×72 schaalt en naar het 64-kleurenpalet van de PT2 quantiseert.

**Zo zijn de huidige zes gemaakt (2026-07-25, Artlist-MCP, Nano Banana 2):**
badge_0 als text-to-image met prompt 2b, de andere vijf als *image-to-image edit*
van dat eerste beeld ("vervang alleen het symbool, laat de rest identiek"). Dat
houdt ring, veld en kadrering over alle zes exact gelijk — zes losse
text-to-image-runs geven zichtbaar verschillende medailles. Prompt de losse
symbolen dik en simpel: het eerste ripple-symbool met drie dunne ringen werd
moiré op deze afmeting, twee dikke ringen plus een stip niet. De zon heeft 8
stralen geworden i.p.v. de gevraagde 7 — het model bleef ze symmetrisch
verdelen, en op 72 px telt niemand ze; de primitieve variant op het
voortgangsscherm tekent er wél 7.
