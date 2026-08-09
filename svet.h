#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <queue>
#include <utility>
#include <cmath>
#include <algorithm>

#include "postavy.h"   // kvoli struct Zbran v mozeZautocit

///////////////////////////////////////////////////////////////////////////////////////////////////
//   ROZMERY SVETA - VSETKO OSTATNE SA POCITA Z VELKOST, NIKDE INDE ZIADNE NATVRDO NAPISANE CISLA
//   1 dlazdica = 5 stop podla D&D. 64 px = sprite 32x32 zvacseny dvakrat (celociselne, kvoli pixel artu).
///////////////////////////////////////////////////////////////////////////////////////////////////
const int VELKOST = 64;                            // strana dlazdice v pixeloch
const float RYCHLOST = 5.f * VELKOST;              // 320 px/s = 5 dlazdic za sekundu (mimo boja)
const int POHYB_V_KOLE = 6;                        // 30 stop = 6 dlazdic za kolo (v boji)
const float DOSAH_MELEE = 1.5f * VELKOST;          // susedna dlazdica vratane diagonaly
const float RYCHLOST_BOJ = 6.f * VELKOST;          // ako rychlo postava kraca po dlazdiciach v boji
const float POSTAVA = 48.f;                        // strana postavy
const float ODSADENIE = (VELKOST - POSTAVA) / 2.f; // aby postava sedela v strede dlazdice (8 px)

// STRED TVARU - koniec rucneho pripocitavania +20 a +22.5 na roznych miestach
inline sf::Vector2f stredTvaru(const sf::RectangleShape& tvar) {
    return tvar.getPosition() + tvar.getSize() / 2.f;
}

// LAVY HORNY ROH DLAZDICE (r, c) pre postavu, aj s odsadenim do stredu
inline sf::Vector2f naDlazdici(int riadok, int stlpec) {
    return sf::Vector2f(stlpec * VELKOST + ODSADENIE, riadok * VELKOST + ODSADENIE);
}

// PRICHYTENIE NA MRIEZKU - mimo boja sa chodi po pixeloch, v boji po dlazdiciach,
// takze pri vstupe do boja treba postavu posadit presne na dlazdicu.
inline void naMriezku(sf::RectangleShape& tvar) {
    sf::Vector2f stred = stredTvaru(tvar);
    int riadok = (int)(stred.y / VELKOST);
    int stlpec = (int)(stred.x / VELKOST);
    tvar.setPosition(naDlazdici(riadok, stlpec));
}

// FUNKCIA NA DETEKCIU VODY
inline bool jePrekazka(float x, float y, std::vector<std::vector<int>>& mapa) {
    if (x < 0 || y < 0) {
        return false;   // mimo mapy vľavo/hore -> nie je prekážka
    }
    int stlpec = x / VELKOST;
    int riadok = y / VELKOST;
    if (riadok >= (int)mapa.size() || stlpec >= (int)mapa[0].size()) {
        return false;   // mimo mapy vpravo/dole
    }
    return mapa[riadok][stlpec] == 1;
}

//FUNKCIA NA DETEKCIU STROMOV
inline bool jeStrom(float x, float y, std::vector<std::vector<int>>& stromMapa) {
    if (x < 0 || y < 0) return false;
    int stlpec = x / VELKOST;
    int riadok = y / VELKOST;
    if (riadok >= (int)stromMapa.size() || stlpec >= (int)stromMapa[0].size()) return false;
    return stromMapa[riadok][stlpec] == 1;
}

// FUNKCIA NA KOLIZIE
inline void pohyb(sf::RectangleShape& hrac, sf::Vector2f smer,
                  std::vector<std::vector<int>>& mapa,
                  std::vector<std::vector<int>>& stromMapa) {
    sf::Vector2f stara = hrac.getPosition();
    hrac.move(smer);

    sf::Vector2f stred = stredTvaru(hrac);   // uz ziadne rucne +20
    bool voda = jePrekazka(stred.x, stred.y, mapa);
    bool strom_kolizia = jeStrom(stred.x, stred.y, stromMapa);

    if (voda || strom_kolizia) {
        hrac.setPosition(stara);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//   POHYB PO MRIEZKE V BOJI
///////////////////////////////////////////////////////////////////////////////////////////////////

// DA SA NA TU DLAZDICU VSTUPIT? (voda a stromy nie, kamen ano - rovnako ako mimo boja)
inline bool dlazdicaVolna(int riadok, int stlpec,
                          std::vector<std::vector<int>>& mapa,
                          std::vector<std::vector<int>>& stromMapa) {
    if (riadok < 0 || stlpec < 0) return false;
    if (riadok >= (int)mapa.size() || stlpec >= (int)mapa[0].size()) return false;
    if (mapa[riadok][stlpec] == 1) return false;        // voda
    if (stromMapa[riadok][stlpec] == 1) return false;   // strom
    return true;
}

// DOSAH POHYBU cez BFS (prehladavanie do sirky) - vrati mriezku nakladov okolo startu.
// -1 = sem sa neda dostat, inak pocet krokov. 8 smerov, diagonala stoji 1 (D&D 5e).
// BFS je tu lepsi nez rovna ciara: obide strom aj nepriatela a rovno da spravnu vzdialenost.
// blokR/blokC je dlazdica, na ktorej stoji ten druhy (cez neho sa nedá prejst).
inline std::vector<std::vector<int>> dosahPohybu(int startR, int startC, int kroky,
                                                 std::vector<std::vector<int>>& mapa,
                                                 std::vector<std::vector<int>>& stromMapa,
                                                 int blokR, int blokC) {
    int N = 2 * kroky + 1;                                        // mriezka nakladov je stvorec okolo startu
    std::vector<std::vector<int>> naklady(N, std::vector<int>(N, -1));
    naklady[kroky][kroky] = 0;                                    // stred = start, 0 krokov

    std::queue<std::pair<int, int>> front;
    front.push(std::make_pair(startR, startC));

    while (!front.empty()) {
        std::pair<int, int> teraz = front.front();
        front.pop();
        int r = teraz.first;
        int c = teraz.second;
        int cena = naklady[r - startR + kroky][c - startC + kroky];
        if (cena >= kroky) continue;                              // dalej uz nedociahne

        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;                 // seba preskoc
                int nr = r + dr;
                int nc = c + dc;
                int ir = nr - startR + kroky;                     // index v mriezke nakladov
                int ic = nc - startC + kroky;
                if (ir < 0 || ic < 0 || ir >= N || ic >= N) continue;   // mimo dosahu
                if (naklady[ir][ic] != -1) continue;                    // uz najdene lacnejsie
                if (!dlazdicaVolna(nr, nc, mapa, stromMapa)) continue;
                if (nr == blokR && nc == blokC) continue;               // tam niekto stoji
                naklady[ir][ic] = cena + 1;
                front.push(std::make_pair(nr, nc));
            }
        }
    }
    return naklady;
}

// KOLKO KROKOV NA DLAZDICU (r, c)? -1 = nedosiahnutelna alebo mimo mriezky
inline int nakladNa(const std::vector<std::vector<int>>& naklady,
                    int startR, int startC, int kroky, int riadok, int stlpec) {
    if (naklady.empty()) return -1;
    int ir = riadok - startR + kroky;
    int ic = stlpec - startC + kroky;
    if (ir < 0 || ic < 0 || ir >= (int)naklady.size() || ic >= (int)naklady[0].size()) return -1;
    return naklady[ir][ic];
}

// SUSEDI? (D&D melee dosah - vratane diagonaly)
inline bool suSusedia(int r1, int c1, int r2, int c2) {
    int dr = r1 - r2;
    int dc = c1 - c2;
    if (dr < 0) dr = -dr;
    if (dc < 0) dc = -dc;
    return (dr <= 1 && dc <= 1);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//   ANIMACIA CHODZE PO DLAZDICIACH
///////////////////////////////////////////////////////////////////////////////////////////////////

// Zoznam dlazdic, po ktorych postava prave kraca, a kolkatu z nich prave ide.
// Kym `bezi()`, hra caka - neda sa klikat ani ukoncit kolo.
struct AnimPohyb {
    std::vector<sf::Vector2f> body;   // rohy dlazdic po ceste (uz aj s odsadenim)
    int index = 0;                    // ku ktoremu bodu prave kracame

    bool bezi() const { return index < (int)body.size(); }

    void zastav() {                   // pri konci boja alebo uteku
        body.clear();
        index = 0;
    }
};

// POSUN O JEDEN FRAME smerom k dalsiemu bodu cesty.
// Doraz na dlazdicu je PRESNY (setPosition), nie priblizny - inak by sa chyby scitavali
// a postava by po niekolkych kolach stala mimo mriezky.
inline void posunPoCeste(sf::RectangleShape& tvar, AnimPohyb& anim, float dt, float rychlost) {
    if (!anim.bezi()) return;

    sf::Vector2f ciel = anim.body[anim.index];
    sf::Vector2f rozdiel = ciel - tvar.getPosition();
    float vzdialenost = std::sqrt(rozdiel.x * rozdiel.x + rozdiel.y * rozdiel.y);
    float krok = rychlost * dt;

    if (vzdialenost <= krok || vzdialenost <= 0.001f) {
        tvar.setPosition(ciel);       // presne na dlazdicu
        anim.index++;
        if (!anim.bezi()) anim.zastav();
    } else {
        tvar.move(rozdiel / vzdialenost * krok);   // jednotkovy smer krat dlzka kroku
    }
}

// CESTA NA DLAZDICU z uz vyratanej mriezky nakladov.
// Ide sa POZADU od ciela: susedna dlazdica s nakladom o 1 mensim je predchodca.
// Preto netreba pamatat ziadnu mapu predchodcov, staci samotny BFS vysledok.
inline std::vector<sf::Vector2f> cestaNaDlazdicu(const std::vector<std::vector<int>>& naklady,
                                                 int startR, int startC, int kroky,
                                                 int cielR, int cielC) {
    std::vector<sf::Vector2f> cesta;
    int cena = nakladNa(naklady, startR, startC, kroky, cielR, cielC);
    if (cena <= 0) return cesta;      // nedosiahnutelne alebo tam uz stojime

    int r = cielR;
    int c = cielC;

    // Susedov s nakladom o 1 mensim byva viac a VSETKY davaju rovnako dlhu cestu.
    // Keby sme brali prveho v poradi (hore-vlavo), cesta by kludne slo minimalny pocet
    // krokov, ale vizualne by kludkala do L. Preto z kandidatov berieme toho, ktory je
    // NAJBLIZSIE ROVNEJ CIARE zo startu do ciela - vysledok potom vyzera ako priama chodza.
    float sx = startC + 0.5f;
    float sy = startR + 0.5f;
    float smerX = (cielC + 0.5f) - sx;
    float smerY = (cielR + 0.5f) - sy;

    while (cena > 0) {
        cesta.push_back(naDlazdici(r, c));

        bool naslo = false;
        int najR = r;
        int najC = c;
        float najOdchylka = 0.f;

        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                if (nakladNa(naklady, startR, startC, kroky, r + dr, c + dc) != cena - 1) continue;

                // kolmá vzdialenosť kandidáta od priamky start->ciel (cross product)
                float px = (c + dc) + 0.5f - sx;
                float py = (r + dr) + 0.5f - sy;
                float odchylka = px * smerY - py * smerX;
                if (odchylka < 0.f) odchylka = -odchylka;

                if (!naslo || odchylka < najOdchylka) {
                    naslo = true;
                    najOdchylka = odchylka;
                    najR = r + dr;
                    najC = c + dc;
                }
            }
        }
        if (!naslo) break;            // poistka, nemalo by nastat
        r = najR;
        c = najC;
        cena--;
    }
    std::reverse(cesta.begin(), cesta.end());   // BFS islo pozadu, my chceme od seba k cielu
    return cesta;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//   DOSTREL A VYHLAD (line of sight)
///////////////////////////////////////////////////////////////////////////////////////////////////

// VZDIALENOST V DLAZDICIACH - Chebyshev, cize diagonala sa rata ako 1 (rovnako ako pohyb)
inline int vzdialenostDlazdic(int r1, int c1, int r2, int c2) {
    int dr = r1 - r2;
    int dc = c1 - c2;
    if (dr < 0) dr = -dr;
    if (dc < 0) dc = -dc;
    return std::max(dr, dc);
}

// VIDIET Z DLAZDICE NA DLAZDICU?
// Usecka medzi STREDMI dlazdic sa odkrokuje po stvrtinach dlazdice a na kazdom vzorku
// sa pozrieme, na akej dlazdici sme. Jednoduchsie nez Bresenham a v hre sa sprava lepsie.
// POZOR: vyhlad blokuju len STROMY. Voda zastavi pohyb, ale cez rieku vidno aj strielat sa da,
// preto sa sem NEDA pouzit dlazdicaVolna, hoci to vyzera ako ta ista otazka.
inline bool jeVidiet(int r1, int c1, int r2, int c2,
                     std::vector<std::vector<int>>& stromMapa) {
    float x1 = c1 + 0.5f;   // stredy dlazdic v jednotkach dlazdic
    float y1 = r1 + 0.5f;
    float x2 = c2 + 0.5f;
    float y2 = r2 + 0.5f;
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dlzka = std::sqrt(dx * dx + dy * dy);

    int vzorkov = (int)(dlzka * 4.f);   // 4 vzorky na dlazdicu
    for (int i = 1; i < vzorkov; i++) {
        float t = (float)i / (float)vzorkov;
        int r = (int)(y1 + dy * t);
        int c = (int)(x1 + dx * t);
        if (r == r1 && c == c1) continue;   // vlastna dlazdica nezavadzia
        if (r == r2 && c == c2) continue;   // ani cielova
        if (r < 0 || c < 0) continue;
        if (r >= (int)stromMapa.size() || c >= (int)stromMapa[0].size()) continue;
        if (stromMapa[r][c] == 1) return false;
    }
    return true;
}

// DA SA ODTIALTO ZAUTOCIT TOUTO ZBRANOU?
inline bool mozeZautocit(const Zbran& zbran, int r1, int c1, int r2, int c2,
                         std::vector<std::vector<int>>& stromMapa) {
    if (vzdialenostDlazdic(r1, c1, r2, c2) > zbran.dosah) return false;
    if (zbran.dialkova && !jeVidiet(r1, c1, r2, c2, stromMapa)) return false;
    return true;
}
