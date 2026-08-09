# RPG - ako to celé funguje

---

## 1. Rozdelenie do súborov

| Súbor | Čo v ňom je | Závisí od |
|---|---|---|
| `main.cpp` | `enum HernyStav` a celý `main()` s hernou slučkou | všetkých ostatných |
| `ui.h` | paleta farieb, rozmery, kresliace funkcie | SFML |
| `postavy.h` | `Bytost`, `Zbran`, kocky, útok, reset boja | nič (ani SFML) |
| `svet.h` | mriežka, kolízie, BFS, animácia, výhľad | SFML + `postavy.h` |
| `dialog.h` | `Odpoved`, `DialogUzol`, `Quest`, texty dialógu | nič |

Pravidlá, ktoré tie hlavičky držia pohromade:

- `#pragma once` na prvom riadku, aby sa obsah nevložil dvakrát
- každá hlavička si sama includuje, čo potrebuje (nespoliehať sa na to, že to už niekto includol pred ňou)
- funkcie v hlavičke musia byť `inline`, inak pri druhom `.cpp` súbore linker zahlási `already defined`
- `const` premenné `inline` nepotrebujú, majú v C++ internal linkage
- do `CMakeLists.txt` sa hlavičky nepridávajú, len `.cpp` súbory

To, že `postavy.h` nepotrebuje SFML, je dobrý znak. Znamená to, že bojová logika je nezávislá od grafiky.

---

## 2. Mriežka a rozmery

Všetko sa počíta z jednej konštanty:

```cpp
const int VELKOST = 64;                // 1 dlaždica = 5 stôp (D&D)
const float RYCHLOST = 5.f * VELKOST;  // pohyb mimo boja
const float RYCHLOST_BOJ = 6.f * VELKOST;
const int POHYB_V_KOLE = 6;            // 30 stôp za kolo
const float POSTAVA = 48.f;
const float ODSADENIE = (VELKOST - POSTAVA) / 2.f;
```

64 preto, že pixelové textúry sa musia zväčšovať celočíselne. 64 je 32x2 alebo 16x4, obe bežné veľkosti spritov. Pri 60 by si potreboval sprite 30x30 alebo 15x15.

Pomocné funkcie, ktoré ukončili ručné pripočítavanie čísel:

- `stredTvaru(tvar)` - stred postavy namiesto ručného `+20` a `+22.5`
- `naDlazdici(r, c)` - ľavý horný roh dlaždice aj s odsadením do stredu
- `naMriezku(tvar)` - prichytenie na najbližšiu dlaždicu
- `vzdialenostDlazdic(r1,c1,r2,c2)` - Chebyshev, diagonála sa ráta ako 1

Mriežka sa kreslí zadarmo: rovnaká `dlazdica`, čo už kreslíš, dostala `setOutlineThickness(-1.f)`. **Záporná hrúbka kreslí obrys dovnútra.** Kladná by presahovala do susedov a čiary by boli miestami dvojnásobne tmavé.

---

## 3. Herná slučka - poradie je dôležité

```
1.  dt = clock.restart()
2.  animácia chôdze (posunPoCeste)        <- musí byť pred výpočtom dlaždíc
3.  niektoIde = beží nejaká animácia?
4.  dlaždice hráča a vlka, vlkVDosahu
5.  rozmery UI (bar, tlačidlá, menu)      <- raz, použije klik aj kreslenie
6.  ak nie je pauza: časovače, vstup do boja, BFS dosahu
7.  pollEvent (klávesy, klik)
8.  niektoIde PREPOČÍTAŤ                  <- klik mohol práve rozbehnúť chôdzu
9.  ak nie je pauza: WSAD, koniec boja, útok po príchode, vlkov ťah
10. kreslenie sveta -> kreslenie UI -> menu
11. display()
```

Tri veci, ktoré z toho poradia vyplývajú:

**Pauza nezastavuje slučku, len logiku.** `pollEvent` a `display` bežia vždy. Keby si ich preskočil, Windows označí hru za neodpovedajúcu. To isté platí pre `sf::sleep`, ten sa v hernej slučke nepoužíva nikdy. Namiesto čakania sa odpočítava float časovač o `dt` každý frame.

**UI rozmery sa počítajú raz.** Klik aj kreslenie používajú tie isté `sf::FloatRect`. Keď boli rozmery napísané dvakrát, tlačidlá sa dali kliknúť inde, než boli vidieť.

**Bod 8 bola chyba, ktorú si našiel.** Viac nižšie.

---

## 4. Dva súradnicové systémy

Toto je najčastejší zdroj záhad v celej hre.

| | `view` (kamera) | `uiView` (obrazovka) |
|---|---|---|
| čo kreslí | mapa, postavy, hlášky, dialóg | bar, tlačidlá, quest, menu |
| ako prepočítať klik | `mapPixelToCoords(pos)` | `mapPixelToCoords(pos, uiView)` |

`window.getDefaultView()` sa **pri zmene veľkosti okna neaktualizuje**, preto má hra vlastný `uiView`, ktorý sa pri `Resized` prepočíta spolu s kamerou. Presne toto spôsobilo, že tlačidlá sa dali kliknúť nižšie, než sa zobrazovali.

Okno je `sf::Style::None` cez celú plochu (borderless fullscreen). Nemá krížik, takže Escape má kaskádu: dialóg -> zavri dialóg, inak prepni menu.

---

## 5. Boj

### Stav

```cpp
enum HernyStav { VOLNY_POHYB, COMBAT };
bool pauza;            // samostatne, nie tretia hodnota enumu
bool hracovTah;
bool akciaPouzita;     // akcia sa minie, pohyb je nezávislý
int zostavaPohyb;      // dlaždice
float cakanie;         // pauza medzi ťahmi
int vybranaZbran;      // -1 = necielim
int cakajuciUtok;      // zbraň, ktorou sa udrie po dôjdení
```

`pauza` je zámerne bool a nie hodnota v `HernyStav`. Keby bola v enume, zapnutie menu v boji by prepísalo `COMBAT` a po zavretí by sa hra nemala kam vrátiť.

### Kolo

Akcia a pohyb sú dve nezávislé veci ako v D&D. Môžeš sa presunúť a potom udrieť, alebo naopak. Kolo končíš sám tlačidlom.

### Zbraň je dáta

```cpp
struct Zbran { std::string nazov; int dosah; int kockyPocet; int kockySteny; bool dialkova; };
```

Zbrane sú `std::vector<Zbran>` a **tlačidlá v bare sa z toho vektora generujú**. Pridanie kuše je jeden riadok dát, nie nová vetva v `zautoc`. Diaľkové zbrane sa riadia obratnosťou, zblízka silou.

### Cielenie ako v BG3

1. klik na zbraň zapne cielenie (druhý klik ho zruší)
2. na vlkovi sa objaví rámik: zelený = vyjde to, červený = nie
3. pri luku sa navyše ťahá čiara výhľadu
4. klik na vlka: ak je dosah, útok hneď; ak nie, nájde sa **najlacnejšia dlaždica, z ktorej to vyjde**, postava tam dôjde a udrie až po dorazení
5. ak taká dlaždica nie je, vypíše sa `Nemam dosah!` a nič sa neminie

---

## 6. BFS - jeden algoritmus, tri použitia

`dosahPohybu(startR, startC, kroky, mapa, stromMapa, blokR, blokC)` je prehľadávanie do šírky. Vracia mriežku nákladov veľkosti `(2*kroky+1)^2`, kde `-1` znamená nedostupné a inak je tam počet krokov. Osem smerov, diagonála stojí 1.

Prečo BFS a nie priamka: **priamka sa o strom zastaví, BFS ho obíde** a rovno dá skutočnú vzdialenosť v krokoch.

Z jedného výpočtu dostaneš tri veci:

1. **kam sa dá dôjsť** - modré políčka dosahu
2. **koľko to stojí** - kontrola pri kliku
3. **kadiaľ ísť** - cesta pre animáciu

Cesta sa rekonštruuje **pozadu od cieľa**: sused s nákladom o 1 menším musí byť predchodca. Preto netreba pamätať žiadnu mapu predchodcov.

### Animácia chôdze

```cpp
struct AnimPohyb { std::vector<sf::Vector2f> body; int index; };
```

`posunPoCeste` posunie tvar o `rychlost * dt` k ďalšiemu bodu. **Na dlaždicu dosadá presne cez `setPosition`**, nie priblížne. Keby sa len ďalej posúval, cieľ by vždy o kúsok prestrelil, chyby by sa sčítali a po pár kolách by postava stála mimo mriežky.

Kým `niektoIde`, hra čaká: žiadne kliky, žiadne tlačidlá, žiadny vlkov ťah a ani prepočet BFS (inak by dosah poskakoval podľa medzipolohy).

### Akcia sa rozpadá na "začni" a "dokonči"

Pri každej animácii platí ten istý vzor. Vlkov ťah má dve fázy cez `vlkSaPohol`: najprv sa rozbehne chôdza, a až keď dobehne, útočí. Hráčov útok po priblížení funguje rovnako cez `cakajuciUtok`.

---

## 7. Dostrel a výhľad

```cpp
mozeZautocit(zbran, r1, c1, r2, c2, stromMapa)
```

Skontroluje vzdialenosť a pri diaľkovej zbrani aj výhľad.

`jeVidiet` vezme úsečku medzi **stredmi** dlaždíc a odkroká ju po štvrtinách dlaždice. Na každom vzorku sa pozrie, na akej dlaždici stojí. Jednoduchšie než Bresenham a v hre sa správa lepšie.

**Výhľad blokujú len stromy, voda nie.** Cez rieku sa dá strieľať, hoci sa cez ňu nedá prejsť. Preto sa na LOS nedá použiť `dlazdicaVolna`, aj keď to vyzerá ako tá istá otázka. Pohyb a výhľad sú dve rôzne veci.

---

## 8. UI - stavebné kocky

Všetko UI ide cez štyri funkcie:

- `kresliPanel` - **jediné miesto, kde sa kreslí pozadie** okien, bublín a tlačidiel
- `kresliTextVStrede`, `kresliTextVlavo`
- `kresliBar`

`kresliTlacidlo` a `kresliHlasku` už len skladajú tie štyri. Farby a rozmery sú v palete hore v `ui.h`, v kóde nižšie nie je ani jedno ručne napísané `sf::Color`.

Až prídu textúry, prepíše sa **vnútro `kresliPanel`** (9-slice rám) a zmení sa celé UI naraz. K tomu bude treba `setSmooth(false)`, inak SFML pixely rozmaže, a pixelový font v násobkoch základnej veľkosti.

### Prečo bolo UI rozhádzané

`sf::Text::getLocalBounds()` **nezačína v nule**, má aj `position`, čo je odsadenie podľa tvaru písmen a stúpania fontu. Centrovanie cez `x + sirka/2 - tb.size.x/2` je preto vždy o pár pixelov vedľa a pri každom texte inak. Správne sa posúva kotva:

```cpp
t.setOrigin(tb.position + tb.size / 2.f);
t.setPosition(stred);
```

---

## 9. Naučené pasce

- **záporná hrúbka obrysu** kreslí dovnútra, kladná presahuje do susedov
- **`getLocalBounds()` nezačína v nule** - centrovať cez `setOrigin`
- **`getDefaultView()` sa pri resize neaktualizuje** - preto vlastný `uiView`
- **nikdy nemiešať dva súradnicové systémy** - `mapPixelToCoords` vie prepočítať do ľubovoľného view
- **nikdy `sf::sleep`** v hernej slučke - odpočítavať float o `dt`
- **presné dosadanie na mriežku**, inak sa chyby sčítajú
- **zatienené premenné** - vlkov HP bar mal `barY` rovnaké ako spodný bar
- **celočíselné delenie záporných** oreže k nule, preto `if (x < 0) return` pred delením
- **tranzitívne includy** - `<string>` ti dotiahne SFML, ale spoliehať sa naň nemáš
- **snímka stavu zastará** - pozri nižšie

---

## 10. Čo je ďalej

- streľba na susedného nepriateľa má v 5e disadvantage, zatiaľ sa neráta
- viac nepriateľov - `vlk` a `vlkShape` treba nahradiť zoznamom a pridať iniciatívu
- JSON character sheet (`nlohmann/json`)
- textúry a pixel art: sprite 32x32, scale 2, `setSmooth(false)`, pixelový font
- do menu: nastavenia, save/load
- generovanie mapy zabaliť do funkcie a presunúť do `svet.h`
